#include "BehaviorManager.hpp"
#include "TraitManager.hpp"
#include "SurrenderHandler.hpp"
#include "EffectManager.hpp"
#include "Settings.hpp"
#include <RE/C/Character.h>
#include <RE/A/ActorValueOwner.h>
#include <RE/E/ExtraTextDisplayData.h>
#include <REL/Relocation.h>
#include <random>
#include <thread>
#include <chrono>
#include <mutex>
#include <unordered_set>
#include <new>
#include <utility>

#include <unordered_map>

// Debounce: prevent multiple simultaneous re-pacify tasks for the same actor
static std::mutex g_pacifyMutex;
static std::unordered_set<RE::FormID> g_beingPacified;

// Map to track dismissed clones for cleanup: FormID -> Game Day of Dismissal
static std::mutex g_cleanupMutex;
static std::unordered_map<RE::FormID, float> g_dismissedClones;

// Blacklist: actors we explicitly dismissed — blocks RE-PACIFY and RestoreAllyState from re-adding them
static std::mutex g_dismissedMutex;
static std::unordered_set<RE::FormID> g_dismissedActors;

namespace Loyalty {
    // Helper to call engine's SetRelationshipRank (SSE: 36544, AE: 37544)
    void SetRelationshipRank(RE::Actor* a_actor, RE::Actor* a_target, std::int32_t a_rank) {
        using func_t = void(RE::Actor*, RE::Actor*, std::int32_t);
        static REL::Relocation<func_t> func{ RELOCATION_ID(36544, 37544) };
        if (a_actor && a_target) {
            func(a_actor, a_target, a_rank);
        }
    }

    // Helper to call engine's StartCombat (SSE: 36254, AE: 37243)
    void StartCombat(RE::Actor* a_actor, RE::Actor* a_target) {
        using func_t = void(RE::Actor*, RE::Actor*, bool);
        static REL::Relocation<func_t> func{ RELOCATION_ID(36254, 37243) };
        if (a_actor && a_target) {
            func(a_actor, a_target, false);
        }
    }

    // Helper to programmatically execute console command using engine's Script class
    void ExecuteConsoleCommand(std::string_view a_command, RE::TESObjectREFR* a_target = nullptr) {
        auto factory = RE::IFormFactory::GetFormFactoryByType(RE::FormType::Script);
        auto script = factory ? factory->Create()->As<RE::Script>() : nullptr;
        if (script) {
            script->SetCommand(a_command);
            script->CompileAndRun(a_target);
            delete script;
        }
    }

    bool BehaviorManager::IsMerchantOrServiceNPC(RE::Actor* a_actor) {
        if (!a_actor) return false;

        // 1. Check engine-level Vendor Keyword (0x00096637 = ActorTypeVendor)
        auto vendorKeyword = RE::TESForm::LookupByID<RE::BGSKeyword>(0x00096637);
        if (vendorKeyword && a_actor->HasKeyword(vendorKeyword)) {
            return true;
        }

        auto base = a_actor->GetActorBase();
        if (base) {
            if (vendorKeyword && base->HasKeyword(vendorKeyword)) {
                return true;
            }

            // Check template form for keyword
            if (base->baseTemplateForm) {
                auto tempNPC = base->baseTemplateForm->As<RE::TESNPC>();
                if (tempNPC && vendorKeyword && tempNPC->HasKeyword(vendorKeyword)) {
                    return true;
                }
            }
        }

        static const RE::FormID merchantFactions[] = {
            0x00051596, // JobMerchantFaction
            0x00051599, // JobBlacksmithFaction
            0x0005C013, // JobInnkeeperFaction
            0x0009D1B9, // JobSellsSpellsFaction
            0x00051598, // JobApothecaryFaction
            0x00050689, // ServicesApothecary
            0x0005068F, // ServicesBlacksmith
            0x00050688, // ServicesInnkeeper
            0x0005068C, // ServicesMerchant
            0x0005068B, // ServicesSpellTrader
            0x00085A6A  // ServicesTrainer
        };

        // 2. Check actor's active factions
        for (auto fID : merchantFactions) {
            auto faction = RE::TESForm::LookupByID<RE::TESFaction>(fID);
            if (faction && a_actor->IsInFaction(faction)) {
                return true;
            }
        }

        if (base) {
            // 3. Check base form's faction list (handles silent/inactive/rank -1 factions)
            for (const auto& fData : base->factions) {
                if (fData.faction) {
                    for (auto fID : merchantFactions) {
                        if (fData.faction->GetFormID() == fID) {
                            return true;
                        }
                    }
                }
            }

            // 4. Check Template Factions (for templated merchants/innkeepers)
            if (base->baseTemplateForm) {
                auto tempNPC = base->baseTemplateForm->As<RE::TESNPC>();
                if (tempNPC) {
                    for (const auto& fData : tempNPC->factions) {
                        if (fData.faction) {
                            for (auto fID : merchantFactions) {
                                if (fData.faction->GetFormID() == fID) {
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
        }

        // 5. Fail-safe text check on display name
        if (a_actor->GetName()) {
            std::string name = a_actor->GetName();
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            static const std::vector<std::string> merchantKeywords = {
                "merchant", "innkeeper", "vendor", "shopkeeper", "trader", 
                "bartender", "apothecary", "blacksmith", "inn keeper", "shop keeper"
            };
            for (const auto& kw : merchantKeywords) {
                if (name.find(kw) != std::string::npos) {
                    return true;
                }
            }
        }

        return false;
    }

    // -----------------------------------------------------------------------
    // ResetActorAI: Cleanly flushes all follower/combat AI state from an actor
    // and schedules a deferred EvaluatePackage so NFF's Papyrus cleanup scripts
    // can run first (preventing race-conditions on dismissal).
    // -----------------------------------------------------------------------
    void BehaviorManager::ResetActorAI(RE::Actor* a_actor) {
        if (!a_actor) return;

        // 1. Immediate state flush — clear any active look-at/head-track and combat
        //    kResetAI flag tells the engine to flush the AI controller on next update
        auto& runtimeFlags = a_actor->GetActorRuntimeData();
        runtimeFlags.boolBits.set(RE::Actor::BOOL_BITS::kResetAI);
        a_actor->StopInteractingQuick(true);
        a_actor->StopCombat();
        a_actor->DrawWeaponMagicHands(false);

        auto processLists = RE::ProcessLists::GetSingleton();
        if (processLists) {
            processLists->StopCombatAndAlarmOnActor(a_actor, true);
        }

        // 2. First EvaluatePackage — immediate pass to pop the follower package stack
        a_actor->EvaluatePackage(true, true);

        // 3. Deferred second EvaluatePackage via SKSE task interface
        //    This runs on the next game frame, AFTER NFF's Papyrus scripts have had
        //    a chance to release their own follower tracking (faction-based).
        RE::ActorHandle handle = a_actor->GetHandle();
        SKSE::GetTaskInterface()->AddTask([handle]() {
            auto actor = handle.get();
            if (actor && !actor->IsDead()) {
                actor->EvaluatePackage(true, true);
                SKSE::log::info("[AI_RESET] Deferred EvaluatePackage executed for '{}' (FormID={:08X})",
                    actor->GetName(), actor->GetFormID());
            }
        });

        SKSE::log::info("[AI_RESET] ResetActorAI called for '{}' (FormID={:08X})", 
            a_actor->GetName(), a_actor->GetFormID());
    }

    bool IsBanditLike(RE::Actor* a_actor) {
        if (!a_actor) return false;

        static const RE::FormID banditFactions[] = {
            0x0001B0E4, // Bandit
            0x00043599, // Forsworn
            0x0003DF17, // Warlock
            0x00027242  // Vampire
        };

        for (auto id : banditFactions) {
            auto faction = RE::TESForm::LookupByID<RE::TESFaction>(id);
            if (faction && a_actor->IsInFaction(faction)) {
                return true;
            }
        }
        return false;
    }

    enum class MercenaryClass {
        kMelee,
        kTwoHanded,
        kArcher,
        kMage,
        kGeneric
    };

    std::string GenerateRandomNameForClassAndGender(MercenaryClass a_class, bool a_isFemale) {
        static std::random_device rd;
        static std::mt19937 gen(rd());

        // %3 şansla efsanevi Easter Egg ismi
        std::uniform_int_distribution<> easterDis(1, 100);
        if (easterDis(gen) <= 3) {
            return a_isFemale ? "Queen Arif" : "King Arif";
        }

        static const std::vector<std::string> maleNames = {
            "Bjorn", "Thorald", "Rurik", "Sigurd", "Valgard", "Hrogar", "Torvald", "Erik",
            "Gunther", "Karl", "Sven", "Olaf", "Marcus", "Lucan", "Quintus", "Decimus",
            "Tiberius", "Alistair", "Hadvar", "Aldis", "Jean", "Raymond", "Giraud", "Arvel",
            "Ragnar", "Skjold", "Einar", "Sigtryggr", "Ulfric", "Gunnar", "Rollo", "Leif",
            "Ingolf", "Harald", "Knut", "Ivar", "Arif",
            "Tarkan", "Alp", "Baybars", "Mete", "Ertugrul", "Battal", "Polat", "Suleyman", "Cuneyt", "Bamsi"
        };

        static const std::vector<std::string> femaleNames = {
            "Freya", "Astrid", "Yrsa", "Hilda", "Gerd", "Sigrid", "Ingrid", "Solveig",
            "Dagny", "Alfhild", "Borghild", "Aela", "Mjoll", "Lydia", "Valeria", "Camilla",
            "Serana", "Karliah", "Ingun", "Delphine", "Elisif", "Irileth", "Senna", "Muiri",
            "Sylgja", "Temba", "Vex", "Lisette", "Runa", "Lyra", "Ria", "Jordis", "Anska",
            "Tomris", "Asena", "Sabiha", "Nene", "Leyla", "Sirin", "Banu", "Malhun", "Ayse", "Fatma"
        };

        static const std::vector<std::string> meleeTitles = {
            "the Bold", "Iron-Sides", "Shield-Wall", "the Fierce", "Steel-Heart", "the Grim", "Bear-Skin"
        };
        static const std::vector<std::string> twoHandedTitles = {
            "Iron-Eye", "Blood-Drinker", "Stone-Fist", "Bone-Breaker", "Beard-Splitter", "the Unbreakable", "Red-Hand"
        };
        static const std::vector<std::string> archerTitles = {
            "the Quick", "Sharp-Blade", "Swift-Foot", "Shadow-Stalker", "Swift-Arrow", "the Whisperer", "Silent-Step", "Crow-Feather"
        };
        static const std::vector<std::string> mageTitles = {
            "Cold-Heart", "Frost-Veins", "Storm-Bringer", "Dusk-Walker", "the Spell-Weaver", "Fire-Heart", "the Wise"
        };
        static const std::vector<std::string> genericTitles = {
            "the Bold", "Iron-Eye", "Blood-Drinker", "the Quick", "Cold-Heart",
            "Sharp-Blade", "Stone-Fist", "Swift-Foot", "Shadow-Stalker", "Bone-Breaker",
            "the Grim", "Shield-Wall", "One-Eye", "Swift-Arrow", "the Fierce",
            "Frost-Veins", "Wolf-Claw", "Beard-Splitter", "the Unbreakable", "Iron-Sides",
            "the Whisperer", "Silent-Step", "Bear-Skin", "Red-Hand", "the Cruel",
            "Dusk-Walker", "Storm-Bringer", "Crow-Feather", "Gold-Chaser", "Steel-Heart"
        };

        std::string firstName;
        if (a_isFemale) {
            std::uniform_int_distribution<> firstNameDis(0, femaleNames.size() - 1);
            firstName = femaleNames[firstNameDis(gen)];
        } else {
            std::uniform_int_distribution<> firstNameDis(0, maleNames.size() - 1);
            firstName = maleNames[firstNameDis(gen)];
        }

        std::string title;
        if (a_class == MercenaryClass::kMelee) {
            std::uniform_int_distribution<> titleDis(0, meleeTitles.size() - 1);
            title = meleeTitles[titleDis(gen)];
        } else if (a_class == MercenaryClass::kTwoHanded) {
            std::uniform_int_distribution<> titleDis(0, twoHandedTitles.size() - 1);
            title = twoHandedTitles[titleDis(gen)];
        } else if (a_class == MercenaryClass::kArcher) {
            std::uniform_int_distribution<> titleDis(0, archerTitles.size() - 1);
            title = archerTitles[titleDis(gen)];
        } else if (a_class == MercenaryClass::kMage) {
            std::uniform_int_distribution<> titleDis(0, mageTitles.size() - 1);
            title = mageTitles[titleDis(gen)];
        } else {
            std::uniform_int_distribution<> titleDis(0, genericTitles.size() - 1);
            title = genericTitles[titleDis(gen)];
        }

        return firstName + " " + title;
    }

    void AssignRandomNameToActor(RE::Actor* a_actor, MercenaryClass a_class) {
        if (!a_actor) return;
        
        bool isFemale = false;
        auto base = a_actor->GetActorBase();
        if (base) {
            isFemale = (base->GetSex() == RE::SEX::kFemale);
        }

        std::string randomName = GenerateRandomNameForClassAndGender(a_class, isFemale);
        
        auto extraText = a_actor->extraList.GetByType<RE::ExtraTextDisplayData>();
        if (extraText) {
            extraText->SetName(randomName.c_str());
        } else {
            auto newExtra = RE::BSExtraData::Create<RE::ExtraTextDisplayData>();
            if (newExtra) {
                new (newExtra) RE::ExtraTextDisplayData(randomName.c_str());
                a_actor->extraList.Add(newExtra);
            }
        }
        
        SKSE::log::info("[NAME_ASSIGN] Assigned name '{}' to actor FormID={:08X}", randomName, a_actor->GetFormID());
    }

    void SetupAllyFactionsAndAI(RE::Actor* a_actor, RE::PlayerCharacter* a_player) {
        if (!a_actor || !a_player) return;

        // 1. Teammate bayrağını ve kalıcılığı ayarla
        auto& runtimeData = a_actor->GetActorRuntimeData();
        runtimeData.boolBits.set(RE::Actor::BOOL_BITS::kPlayerTeammate);
        runtimeData.boolFlags.reset(RE::Actor::BOOL_FLAGS::kIsCommandedActor);

        a_actor->formFlags |= static_cast<std::uint32_t>(RE::TESForm::RecordFlags::kPersistent);

        // 2. Dost Faction'ları ekle
        auto potentialFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84D);
        if (potentialFollowerFaction) {
            a_actor->AddToFaction(potentialFollowerFaction, 0);
        }
        // NFF COMPATIBILITY NOTE:
        // NFF (Nether's Follower Framework) auto-detects followers by checking:
        //   1. Actor is in CurrentFollowerFaction (0x0005C84E) with rank >= 1
        //   2. Relationship rank with player is >= 3 (Ally)
        // Both conditions are satisfied here, so NFF will auto-import our allies.
        // On dismissal, removing this faction signals NFF to release its tracking.
        auto currentFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84E);
        if (currentFollowerFaction) {
            a_actor->AddToFaction(currentFollowerFaction, 1); // Rank 1 = actively following (NFF-compatible)
        }
        auto playerAllyFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005A1A4);
        if (playerAllyFaction) {
            a_actor->AddToFaction(playerAllyFaction, 0);
        }
        auto playerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x00000013);
        if (playerFaction) {
            a_actor->AddToFaction(playerFaction, 0);
        }

        // 3. Düşman Faction'ları temizle
        // A) Aktörün kendi özel/zindan faction'larını temizle (eski arkadaşlarına yardım etmemesi için)
        auto baseNPC = a_actor->GetActorBase();
        if (baseNPC) {
            for (auto& factionRank : baseNPC->factions) {
                if (factionRank.faction) {
                    a_actor->RemoveFromFaction(factionRank.faction);
                }
            }
        }

        // B) Sabit (dinamik eklenebilecek) genel düşman faction'larını da garanti olsun diye temizle
        static const std::vector<RE::FormID> hostileFactionIDs = {
            0x0001B0E4, // BanditFaction
            0x000E0CDA, // dunBanditFaction
            0x000E1643, // CrimeFactionBandit
            0x000E1644, // CrimeFactionForsworn
            0x000E1645, // CrimeFactionWarlock
            0x00043599, // ForswornFaction
            0x000E0CDB, // dunForswornFaction
            0x0003DF17, // WarlockFaction
            0x000E0CDD, // dunWarlockFaction
            0x00027242, // VampireFaction
            0x000E0CDE, // dunVampireFaction
            0x0005830E, // NecromancerFaction
            0x000E0CDC, // dunNecromancerFaction
            0x00017009  // dunPlayerEnemyFaction (Player's Enemy Faction)
        };

        for (auto id : hostileFactionIDs) {
            auto faction = RE::TESForm::LookupByID<RE::TESFaction>(id);
            if (faction) {
                a_actor->RemoveFromFaction(faction);
            }
        }

        // 4. İlişkileri müttefik yap
        SetRelationshipRank(a_actor, a_player, 3);
        SetRelationshipRank(a_player, a_actor, 3);

        // 5. Agresyon ve AI değerlerini ayarla
        auto avOwner = a_actor->AsActorValueOwner();
        if (avOwner) {
            avOwner->SetBaseActorValue(RE::ActorValue::kAggression, 0.0f); // Unaggressive
            avOwner->SetBaseActorValue(RE::ActorValue::kConfidence, 4.0f); // Foolhardy
            avOwner->SetBaseActorValue(RE::ActorValue::kAssistance, 2.0f);  // Helps friends & allies
            avOwner->SetBaseActorValue(RE::ActorValue::kWaitingForPlayer, 0.0f);
            avOwner->SetBaseActorValue(RE::ActorValue::kMorality, 0.0f); // Any Crime
        }
    }

    void BehaviorManager::ProcessBribeResult(RE::Actor* a_actor, bool a_success, bool a_isLowOffer) {
        if (!a_actor) return;

        if (a_success) {
            auto player = RE::PlayerCharacter::GetSingleton();
            RE::Actor* targetActor = a_actor;
            auto processLists = RE::ProcessLists::GetSingleton();

            if (player) {
                auto base = a_actor->GetActorBase();
                bool isUnique = base && base->IsUnique();
                bool isEssential = base && base->IsEssential();
                
                bool isMerchant = IsMerchantOrServiceNPC(a_actor);

                if (isEssential || isMerchant) {
                    // ==========================================================
                    // YÖNTEM 2: Benim Yerime Adamımı Al (Esnaf ve Quest NPC'leri)
                    // ==========================================================
                    SKSE::log::info("[BRIBE_RECRUIT] Unique Quest/Merchant NPC '{}' bribed. Spawning a mercenary instead.", a_actor->GetName());
                    
                    RE::DebugNotification("I must remain here, but my hired mercenary will protect you!");

                    // Define Low-tier (Apprentice) and High-tier (Veteran) solid vanilla templates
                    static const std::vector<std::pair<std::uint32_t, MercenaryClass>> lowMelee = {
                        { 0x00020DF4, MercenaryClass::kMelee },      // Imperial Recruit (level 4)
                        { 0x00039CF7, MercenaryClass::kMelee }       // Bandit Melee (level 1-5)
                    };
                    static const std::vector<std::pair<std::uint32_t, MercenaryClass>> highMelee = {
                        { 0x00045BE0, MercenaryClass::kMelee },      // Whiterun Guard
                        { 0x00046794, MercenaryClass::kMelee },      // Imperial Soldier (Sword & Shield)
                        { 0x00046EFD, MercenaryClass::kMelee }       // Stormcloak Soldier (Sword & Shield)
                    };

                    static const std::vector<std::pair<std::uint32_t, MercenaryClass>> lowTwoHanded = {
                        { 0x00037C2E, MercenaryClass::kTwoHanded }   // Bandit Thug (level 5)
                    };
                    static const std::vector<std::pair<std::uint32_t, MercenaryClass>> highTwoHanded = {
                        { 0x00046EFE, MercenaryClass::kTwoHanded }   // Stormcloak Soldier (Two-Handed)
                    };

                    static const std::vector<std::pair<std::uint32_t, MercenaryClass>> lowArcher = {
                        { 0x0004622A, MercenaryClass::kArcher }      // Imperial Archer (level 4)
                    };
                    static const std::vector<std::pair<std::uint32_t, MercenaryClass>> highArcher = {
                        { 0x0004622B, MercenaryClass::kArcher },      // Whiterun Guard Archer
                        { 0x0004622A, MercenaryClass::kArcher },      // Imperial Archer
                        { 0x00046EFC, MercenaryClass::kArcher }       // Stormcloak Archer
                    };

                    static const std::vector<std::pair<std::uint32_t, MercenaryClass>> lowMage = {
                        { 0x00039CFA, MercenaryClass::kMage }        // Apprentice Necromancer (level 1)
                    };
                    static const std::vector<std::pair<std::uint32_t, MercenaryClass>> highMage = {
                        { 0x000B3291, MercenaryClass::kMage },       // Vigilant of Stendarr Mage (level 12+)
                        { 0x00039CFB, MercenaryClass::kMage }        // Pyromancer Mage (level 16+)
                    };

                    static std::random_device rd;
                    static std::mt19937 gen(rd());
                    std::uniform_int_distribution<> classDis(0, 3);
                    int rolledClass = classDis(gen);

                    std::uint32_t selectedBaseID = 0x00045BE0;
                    MercenaryClass selectedClass = MercenaryClass::kMelee;

                    if (rolledClass == 0) {
                        const auto& list = a_isLowOffer ? lowMelee : highMelee;
                        std::uniform_int_distribution<> dis(0, list.size() - 1);
                        auto pair = list[dis(gen)];
                        selectedBaseID = pair.first;
                        selectedClass = pair.second;
                    } else if (rolledClass == 1) {
                        const auto& list = a_isLowOffer ? lowTwoHanded : highTwoHanded;
                        std::uniform_int_distribution<> dis(0, list.size() - 1);
                        auto pair = list[dis(gen)];
                        selectedBaseID = pair.first;
                        selectedClass = pair.second;
                    } else if (rolledClass == 2) {
                        const auto& list = a_isLowOffer ? lowArcher : highArcher;
                        std::uniform_int_distribution<> dis(0, list.size() - 1);
                        auto pair = list[dis(gen)];
                        selectedBaseID = pair.first;
                        selectedClass = pair.second;
                    } else {
                        const auto& list = a_isLowOffer ? lowMage : highMage;
                        std::uniform_int_distribution<> dis(0, list.size() - 1);
                        auto pair = list[dis(gen)];
                        selectedBaseID = pair.first;
                        selectedClass = pair.second;
                    }

                    auto mercenaryBase = RE::TESForm::LookupByID<RE::TESNPC>(selectedBaseID);
                    if (!mercenaryBase) {
                        SKSE::log::error("[BRIBE_RECRUIT] Failed to lookup solid template {:08X}. Falling back to 00045BE0.", selectedBaseID);
                        mercenaryBase = RE::TESForm::LookupByID<RE::TESNPC>(0x00045BE0); // Fallback to Whiterun Guard
                        selectedClass = MercenaryClass::kMelee;
                    }

                    if (mercenaryBase) {
                        SKSE::log::info("[BRIBE_RECRUIT] Spawning mercenary from template {:08X} ('{}')", mercenaryBase->GetFormID(), mercenaryBase->GetName());
                        // Spawn relative to the NPC (a_actor) just like cloning, for maximum stability
                        auto spawnedRef = a_actor->PlaceObjectAtMe(mercenaryBase, false);
                        auto spawnedActor = spawnedRef ? spawnedRef->As<RE::Actor>() : nullptr;
                        if (spawnedActor) {
                            targetActor = spawnedActor;
                            AssignRandomNameToActor(targetActor, selectedClass);
                            SKSE::log::info("[BRIBE_RECRUIT] Successfully spawned mercenary '{}' (FormID={:08X})", targetActor->GetName(), targetActor->GetFormID());
                        } else {
                            SKSE::log::error("[BRIBE_RECRUIT] PlaceObjectAtMe failed to spawn actor for template {:08X}!", mercenaryBase->GetFormID());
                        }
                    } else {
                        SKSE::log::error("[BRIBE_RECRUIT] Fallback template 00045BE0 not found in database!");
                    }

                    // Orijinal satıcı/quest NPC'sini barışçıl duruma getirip dükkanına geri gönderiyoruz
                    a_actor->StopCombat();
                    if (processLists) {
                        processLists->StopCombatAndAlarmOnActor(a_actor, true);
                    }
                    a_actor->EvaluatePackage(true, true);

                } else if (isUnique && !isEssential) {
                    // ==========================================================
                    // YÖNTEM 1: Kimlik Değişimi (Gizli Klonlama / Sihirbazlık)
                    // ==========================================================
                    SKSE::log::info("[BRIBE_RECRUIT] Unique minor NPC '{}' bribed. Performing seamless Identity Swap.", a_actor->GetName());
                    
                    auto cloneRef = a_actor->PlaceObjectAtMe(base, false);
                    auto cloneActor = cloneRef ? cloneRef->As<RE::Actor>() : nullptr;
                    if (cloneActor) {
                        // Klonu orijinalin konumuna ve açısına tam olarak oturtuyoruz
                        cloneActor->SetPosition(a_actor->GetPosition(), true);
                        cloneActor->SetAngle(a_actor->GetAngle());

                        // Orijinal eski buggy aktörü anında disable edip siliyoruz
                        a_actor->StopCombat();
                        if (processLists) {
                            processLists->StopCombatAndAlarmOnActor(a_actor, true);
                        }
                        a_actor->Disable();
                        a_actor->SetDelete(true);

                        // Hedefi klona kaydırıyoruz
                        targetActor = cloneActor;
                    }
                } else {
                    // ==========================================================
                    // YÖNTEM 3: Doğal Haliyle Bırakmak (Generic NPC'ler)
                    // ==========================================================
                    SKSE::log::info("[BRIBE_RECRUIT] Generic NPC '{}' bribed. Recruiting the original actor directly.", a_actor->GetName());
                    // Herhangi bir işlem yapmıyoruz, targetActor = a_actor olarak kalıyor!
                    
                    if (!isUnique) {
                        AssignRandomNameToActor(targetActor, MercenaryClass::kGeneric);
                    }
                }
            }

            // Seviyeyi rüşvet miktarına (düşük teklif / yüksek teklif) göre motor seviyesinde güvenli niteliklerle ayarla
            {
                auto avOwner = targetActor->AsActorValueOwner();
                if (avOwner) {
                    float currentMaxHP = avOwner->GetPermanentActorValue(RE::ActorValue::kHealth);
                    float currentMaxMagicka = avOwner->GetPermanentActorValue(RE::ActorValue::kMagicka);
                    float currentMaxStamina = avOwner->GetPermanentActorValue(RE::ActorValue::kStamina);

                    if (a_isLowOffer) {
                        // Düşük rüşvet: Acemi nitelikleri (%25 Can ve Kondisyon azaltılır, motoru bozmaz)
                        avOwner->SetBaseActorValue(RE::ActorValue::kHealth, (std::max)(currentMaxHP * 0.75f, 50.f));
                        avOwner->SetBaseActorValue(RE::ActorValue::kMagicka, (std::max)(currentMaxMagicka * 0.75f, 50.f));
                        avOwner->SetBaseActorValue(RE::ActorValue::kStamina, (std::max)(currentMaxStamina * 0.75f, 50.f));
                        SKSE::log::info("[BRIBE_LEVEL] Scaled actor '{}' (FormID={:08X}) as Apprentice (HP={:.1f})", 
                            targetActor->GetName(), targetActor->GetFormID(), (std::max)(currentMaxHP * 0.75f, 50.f));
                    } else {
                        // Yüksek rüşvet: Elit kıdemli nitelikleri (%35 Can ve Kondisyon artırılır)
                        avOwner->SetBaseActorValue(RE::ActorValue::kHealth, currentMaxHP * 1.35f);
                        avOwner->SetBaseActorValue(RE::ActorValue::kMagicka, currentMaxMagicka * 1.35f);
                        avOwner->SetBaseActorValue(RE::ActorValue::kStamina, currentMaxStamina * 1.35f);
                        SKSE::log::info("[BRIBE_LEVEL] Scaled actor '{}' (FormID={:08X}) as Veteran (HP={:.1f})", 
                            targetActor->GetName(), targetActor->GetFormID(), currentMaxHP * 1.35f);
                    }
                }
            }

            EffectManager::GetSingleton()->PlayAcceptanceEffects(targetActor);
            
            targetActor->StopInteractingQuick(true);
            targetActor->StopCombat();
            targetActor->DrawWeaponMagicHands(false);

            if (processLists) {
                processLists->StopCombatAndAlarmOnActor(targetActor, true);
            }
            
            // If targetActor has changed due to Identity Swap (clone), we need to register the clone's FormID
            // by using GetOrAssignTrait on a_actor (where preview trait was stored) but writing to targetActor (the new clone).
            // Or easier: get the trait for a_actor, write it to targetActor, and erase a_actor.
            NPCTrait trait = TraitManager::GetSingleton()->GetOrAssignTrait(a_actor);
            if (targetActor != a_actor) {
                TraitManager::GetSingleton()->SetTrait(targetActor, trait);
            }
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 100);
            int roll = dis(gen);

            // DYNAMIC BETRAYAL FOR BANDITS:
            if (IsBanditLike(a_actor)) {
                auto settings = Settings::GetSingleton();
                int betrayalChance = a_isLowOffer ? settings->betrayalChanceLowBribe : settings->betrayalChanceHighBribe;
                if (roll <= betrayalChance) {
                    TraitManager::GetSingleton()->SetTrait(targetActor, NPCTrait::Treacherous);
                    trait = NPCTrait::Treacherous;
                    SKSE::log::info("[BRIBE] {} assigned TREACHEROUS trait (roll={}, chance={}%, offer={}).",
                                     targetActor->GetName(), roll, betrayalChance, a_isLowOffer ? "LOW" : "HIGH");
                } else {
                    SKSE::log::info("[BRIBE] {} NOT treacherous (roll={}, chance={}%, offer={}).",
                                     targetActor->GetName(), roll, betrayalChance, a_isLowOffer ? "LOW" : "HIGH");
                }
            } else {
                TraitManager::GetSingleton()->SetTrait(targetActor, trait);
            }

            // Eğer orijinal aktör kimlik değişimi/koruma yüzünden değiştirilmişse,
            // orijinal aktörün FormID'sini TraitMap'ten siliyoruz ki çökme ve ışınlanma yapmasın!
            if (targetActor != a_actor) {
                TraitManager::GetSingleton()->GetTraitMap().erase(a_actor->GetFormID());
            }

            auto avOwner = targetActor->AsActorValueOwner();
            if (avOwner) {
                // Restore Health, Magicka, and Stamina to 100% (as if giving them a potion)
                float maxHealth = avOwner->GetPermanentActorValue(RE::ActorValue::kHealth);
                float currentHealth = avOwner->GetActorValue(RE::ActorValue::kHealth);
                if (currentHealth < maxHealth) {
                    avOwner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, maxHealth - currentHealth);
                }

                float maxMagicka = avOwner->GetPermanentActorValue(RE::ActorValue::kMagicka);
                float currentMagicka = avOwner->GetActorValue(RE::ActorValue::kMagicka);
                if (currentMagicka < maxMagicka) {
                    avOwner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka, maxMagicka - currentMagicka);
                }

                float maxStamina = avOwner->GetPermanentActorValue(RE::ActorValue::kStamina);
                float currentStamina = avOwner->GetActorValue(RE::ActorValue::kStamina);
                if (currentStamina < maxStamina) {
                    avOwner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina, maxStamina - currentStamina);
                }
            }

            if (player) {
                // Setup ally factions, relationships, and unaggressive AI securely
                SetupAllyFactionsAndAI(targetActor, player);
                // ResetActorAI flushes any stale pre-recruitment AI state (combat, look-at)
                // and schedules a deferred EvaluatePackage so NFF has one full frame
                // to detect the new CurrentFollowerFaction rank and auto-import the ally.
                ResetActorAI(targetActor);
                player->StopCombat();
            }

            SKSE::log::info("[BRIBE] {} joined as ally. Trait={}, FormID={:08X}",
                targetActor->GetName(),
                static_cast<int>(trait),
                targetActor->GetFormID());
            LogAllyStatus("Post-Bribe");

            // ÇATIŞMAYI SIFIRLA:
            if (processLists) {
                for (auto& handle : processLists->highActorHandles) {
                    auto nearby = handle.get();
                    if (nearby) {
                        nearby->StopCombat();
                        if (nearby.get() != targetActor && player && nearby->IsHostileToActor(player)) {
                            SetRelationshipRank(targetActor, nearby.get(), -3);
                            SetRelationshipRank(nearby.get(), targetActor, -3);
                        }
                    }
                }
            }

            // Diğer aktif müttefiklerimizle ilişki kur
            auto& traitMap = TraitManager::GetSingleton()->GetTraitMap();
            for (const auto& [otherFormID, otherTrait] : traitMap) {
                if (otherFormID != targetActor->GetFormID()) {
                    auto otherForm = RE::TESForm::LookupByID(otherFormID);
                    if (otherForm) {
                        auto otherActor = otherForm->As<RE::Actor>();
                        if (otherActor && !otherActor->IsDead()) {
                            SetRelationshipRank(targetActor, otherActor, 3); // 3 = Ally
                            SetRelationshipRank(otherActor, targetActor, 3); // 3 = Ally
                            
                            otherActor->StopCombat(); // İlişki değiştikten sonra tekrar durdur
                            otherActor->EvaluatePackage(true, true); // AI paketlerini zorla yenile
                        }
                    }
                }
            }

            // Silahı indir — EvaluatePackage yok (AI tekrar saldırı seçer)
            targetActor->DrawWeaponMagicHands(false);
            targetActor->StopCombat();

            RE::DebugNotification("NPC is now your loyal teammate. (Healed!)");
            
            // Trigger Trait Behaviors
            switch (trait) {
                case NPCTrait::Treacherous:
                    HandleTreacherousBehavior(targetActor);
                    break;
                case NPCTrait::Honorable:
                    HandleHonorableBehavior(targetActor);
                    break;
                case NPCTrait::Greedy:
                    HandleGreedyBehavior(targetActor);
                    break;
                default:
                    break;
            }
        } else {
            EffectManager::GetSingleton()->PlayRejectionEffects(a_actor);
            RE::DebugNotification("Bribe rejected!");
        }
    }

    void BehaviorManager::DismissAlly(RE::Actor* a_actor) {
        if (!a_actor) return;

        // CRITICAL: Add to dismissed blacklist FIRST so RE-PACIFY and RestoreAllyState
        // cannot race-condition re-add this actor before we finish cleanup.
        {
            std::lock_guard<std::mutex> lock(g_dismissedMutex);
            g_dismissedActors.insert(a_actor->GetFormID());
        }

        // CRITICAL: Remove from traitMap immediately so no further ally logic applies.
        TraitManager::GetSingleton()->GetTraitMap().erase(a_actor->GetFormID());

        auto& runtimeData = a_actor->GetActorRuntimeData();
        runtimeData.boolBits.reset(RE::Actor::BOOL_BITS::kPlayerTeammate);
        runtimeData.boolFlags.reset(RE::Actor::BOOL_FLAGS::kIsCommandedActor);

        // Remove persistent flag so the engine can garbage collect it if needed
        a_actor->formFlags &= ~static_cast<std::uint32_t>(RE::TESForm::RecordFlags::kPersistent);

        auto avOwner = a_actor->AsActorValueOwner();
        if (avOwner) {
            avOwner->SetBaseActorValue(RE::ActorValue::kAggression, 1.0f);
            avOwner->SetBaseActorValue(RE::ActorValue::kConfidence, 4.0f); // 4.0 = Korkusuz (Foolhardy)
            avOwner->SetBaseActorValue(RE::ActorValue::kAssistance, 0.0f);
            avOwner->SetBaseActorValue(RE::ActorValue::kWaitingForPlayer, 0.0f);
        }

        auto player = RE::PlayerCharacter::GetSingleton();
        if (player) {
            SetRelationshipRank(a_actor, player, -3); // -3 = Düşman
        }

        // Diğer aktif müttefiklerle olan ilişkileri düşmana çevir
        {
            auto& traitMap = TraitManager::GetSingleton()->GetTraitMap();
            for (const auto& [otherFormID, otherTrait] : traitMap) {
                if (otherFormID != a_actor->GetFormID()) {
                    auto otherForm = RE::TESForm::LookupByID(otherFormID);
                    if (otherForm) {
                        auto otherActor = otherForm->As<RE::Actor>();
                        if (otherActor && !otherActor->IsDead()) {
                            SetRelationshipRank(a_actor, otherActor, -3); // -3 = Enemy
                            SetRelationshipRank(otherActor, a_actor, -3); // -3 = Enemy
                        }
                    }
                }
            }
            // traitMap'ten tamamen çıkar (üstte de silindi ama döngüden önce kopyalandığı için tekrar)
            traitMap.erase(a_actor->GetFormID());
        }

        // Eğer dinamik bir klonsa (FormID >= 0xFF000000), 1 gün sonra temizlenmek üzere kaydet
        if (a_actor->GetFormID() >= 0xFF000000) {
            std::lock_guard<std::mutex> lock(g_cleanupMutex);
            float currentTime = RE::Calendar::GetSingleton() ? RE::Calendar::GetSingleton()->GetCurrentGameTime() : 0.0f;
            g_dismissedClones[a_actor->GetFormID()] = currentTime;
            SKSE::log::info("[CLEANUP] Registered dynamic clone '{}' (FormID={:08X}) for deletion in 1 in-game day.", a_actor->GetName(), a_actor->GetFormID());
        }

        auto potentialFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84D);
        if (potentialFollowerFaction) {
            a_actor->RemoveFromFaction(potentialFollowerFaction);
        }
        auto currentFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84E);
        if (currentFollowerFaction) {
            a_actor->RemoveFromFaction(currentFollowerFaction);
        }
        auto playerAllyFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005A1A4);
        if (playerAllyFaction) {
            a_actor->RemoveFromFaction(playerAllyFaction);
        }
        auto playerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x00000013);
        if (playerFaction) {
            a_actor->RemoveFromFaction(playerFaction);
        }

        // Kovulduğunda tekrardan haydut grubuna katılsın ki diğer haydutlar ona saldırmasın
        auto banditFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0001B0E4);
        if (banditFaction) {
            a_actor->AddToFaction(banditFaction, 0);
        }

        a_actor->DrawWeaponMagicHands(false);
        // ResetActorAI flushes look-at, interaction, and combat state, then schedules
        // a deferred EvaluatePackage so NFF has one game frame to release its tracking.
        ResetActorAI(a_actor);

        bool isBandit = IsBanditLike(a_actor);

        if (isBandit) {
            // Rastgele karar: Ya saldıracak ya da kaçacak (Haydutlar için)
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 100);
            int roll = dis(gen);

            if (player) {
                if (roll <= 50) {
                    // Saldır
                    StartCombat(a_actor, player);
                    std::string attackMsg = std::string(a_actor->GetName()) + " is not happy about being dismissed and attacks!";
                    RE::DebugNotification(attackMsg.c_str());
                } else {
                    // Kaç
                    if (avOwner) {
                        avOwner->SetBaseActorValue(RE::ActorValue::kAggression, 0.0f); // Korkaklaştır
                        avOwner->SetBaseActorValue(RE::ActorValue::kConfidence, 0.0f); // Korkaklaştır
                    }
                    a_actor->InitiateFlee(player, true, true, false, nullptr, nullptr, 0.0f, 2000.0f);
                    ResetActorAI(a_actor);
                    std::string runMsg = std::string(a_actor->GetName()) + " decided to run for their life!";
                    RE::DebugNotification(runMsg.c_str());
                }
            }

            std::string dismissMsg = std::string(a_actor->GetName()) + " has been dismissed.";
            RE::DebugNotification(dismissMsg.c_str());
        } else {
            // Normal Vatandaşlar, Korumalar ve Paralı Askerler için barışçıl sıfırlama
            if (avOwner) {
                // Varsayılan Skyrim motoru değerlerine geri döndür (Sakin ve Yardımsever/Normal)
                if (a_actor->IsGuard()) {
                    avOwner->SetBaseActorValue(RE::ActorValue::kAggression, 0.0f); // Unaggressive
                    avOwner->SetBaseActorValue(RE::ActorValue::kConfidence, 3.0f);  // Brave
                    avOwner->SetBaseActorValue(RE::ActorValue::kAssistance, 1.0f);  // Helps Allies
                } else {
                    avOwner->SetBaseActorValue(RE::ActorValue::kAggression, 0.0f); // Unaggressive
                    avOwner->SetBaseActorValue(RE::ActorValue::kConfidence, 1.0f);  // Average
                    avOwner->SetBaseActorValue(RE::ActorValue::kAssistance, 0.0f);  // Helps Nobody
                }
            }

            if (player) {
                // İlişki durumunu düşman yerine Nötr (0) yapıyoruz ki saldırmasınlar
                SetRelationshipRank(a_actor, player, 0);
            }

            // Full AI reset — flush follower package, let NFF de-register, then
            // re-evaluate so the NPC picks up their native sandbox/patrol package.
            ResetActorAI(a_actor);

            std::string peaceDismissMsg = std::string(a_actor->GetName()) + " leaves peacefully and returns to their duties.";
            RE::DebugNotification(peaceDismissMsg.c_str());
        }
    }

    void BehaviorManager::HandleTreacherousBehavior(RE::Actor* a_actor) {
        auto settings = Settings::GetSingleton();
        SKSE::log::info("Treacherous NPC {} (FormID={:08X}) will betray in {}-{} seconds.", 
            a_actor->GetName(), a_actor->GetFormID(), settings->betrayalMinTime, settings->betrayalMaxTime);

        // Oyuncuya uyarı: "Bir şeyler yanlış hissettiriyor..."
        RE::DebugNotification("Something feels off about this one...");

        // Thread güvenliği: Ham pointer yerine ActorHandle alıyoruz (CTD önlemi)
        RE::ActorHandle actorHandle = a_actor->GetHandle();

        // Ayarları al
        int minDelay = settings->betrayalMinTime * 1000;
        int maxDelay = settings->betrayalMaxTime * 1000;

        // Rastgele gecikme — ayrı thread'de beklenir,
        // ardından güvenli biçimde ana oyun thread'ine ihanet eylemi gönderilir.
        std::thread([actorHandle, minDelay, maxDelay]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(minDelay, maxDelay);
            int delayMs = dis(gen);

            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

            // Oyun thread'ine gönder (thread-safe)
            SKSE::GetTaskInterface()->AddTask([actorHandle]() {
                auto player = RE::PlayerCharacter::GetSingleton();
                auto actorPtr = actorHandle.get();
                
                if (!player || !actorPtr) return;
                
                RE::Actor* safeActor = actorPtr.get();
                if (!safeActor || safeActor->IsDead()) return;

                // Müttefik statüsünü kaldır
                auto& runtimeData = safeActor->GetActorRuntimeData();
                runtimeData.boolBits.reset(RE::Actor::BOOL_BITS::kPlayerTeammate);

                // Remove persistent flag so the engine can garbage collect it if needed
                safeActor->formFlags &= ~static_cast<std::uint32_t>(RE::TESForm::RecordFlags::kPersistent);

                // Factions: Remove from Follower, Player and dunPlayerAllyFaction
                auto potentialFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84D);
                if (potentialFollowerFaction) {
                    safeActor->RemoveFromFaction(potentialFollowerFaction);
                }
                auto currentFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84E);
                if (currentFollowerFaction) {
                    safeActor->RemoveFromFaction(currentFollowerFaction);
                }
                auto playerAllyFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005A1A4);
                if (playerAllyFaction) {
                    safeActor->RemoveFromFaction(playerAllyFaction);
                }
                auto playerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x00000013);
                if (playerFaction) {
                    safeActor->RemoveFromFaction(playerFaction);
                }

                // Düşman ilişki seviyesine ayarla
                SetRelationshipRank(safeActor, player, -4);

                // Diğer müttefiklerle de düşman yap
                auto& traitMap = TraitManager::GetSingleton()->GetTraitMap();
                for (const auto& [otherFormID, otherTrait] : traitMap) {
                    if (otherFormID != safeActor->GetFormID()) {
                        auto otherForm = RE::TESForm::LookupByID(otherFormID);
                        if (otherForm) {
                            auto otherActor = otherForm->As<RE::Actor>();
                            if (otherActor && !otherActor->IsDead()) {
                                SetRelationshipRank(safeActor, otherActor, -4); // Enemy
                                SetRelationshipRank(otherActor, safeActor, -4); // Enemy
                            }
                        }
                    }
                }
                traitMap.erase(safeActor->GetFormID());

                // Eğer dinamik bir klonsa, ihanet sonrasında 1 gün sonra silinmek üzere kaydet
                if (safeActor->GetFormID() >= 0xFF000000) {
                    std::lock_guard<std::mutex> lock(g_cleanupMutex);
                    float currentTime = RE::Calendar::GetSingleton() ? RE::Calendar::GetSingleton()->GetCurrentGameTime() : 0.0f;
                    g_dismissedClones[safeActor->GetFormID()] = currentTime;
                    SKSE::log::info("[CLEANUP] Treacherous dynamic clone '{}' (FormID={:08X}) registered for deletion in 1 in-game day.", safeActor->GetName(), safeActor->GetFormID());
                }

                // Factions: Add back to dunPlayerEnemyFaction so guards/modded followers attack them
                auto enemyFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x00017009);
                if (enemyFaction) {
                    safeActor->AddToFaction(enemyFaction, 0);
                }

                auto avOwner = safeActor->AsActorValueOwner();
                if (avOwner) {
                    avOwner->SetBaseActorValue(RE::ActorValue::kAggression, 2.0f); // Çılgın saldırgan
                }

                // Savaşı başlat
                StartCombat(safeActor, player);
                safeActor->EvaluatePackage(true, true);

                RE::DebugNotification("BETRAYAL! The NPC is attacking you!");
            });
        }).detach(); // Thread'i serbest bırak (join bekleme)
    }


    void BehaviorManager::HandleHonorableBehavior(RE::Actor* a_actor) {
        SKSE::log::info("Honorable NPC {} is now a staunch ally.", a_actor->GetName());
    }

    void BehaviorManager::HandleGreedyBehavior(RE::Actor* a_actor) {
        SKSE::log::info("Greedy NPC {} is satisfied for now.", a_actor->GetName());
    }

    void RestoreAllyStateIfReset(RE::Actor* a_actor, RE::PlayerCharacter* a_player) {
        if (!a_actor || !a_player) return;

        // Silinmiş, devre dışı bırakılmış veya ölen aktörleri asla onarma!
        if (a_actor->IsDeleted() || a_actor->IsDisabled() || a_actor->IsDead()) {
            return;
        }

        // Kara listede olan (kovulmuş) aktörleri asla geri yükleme!
        {
            std::lock_guard<std::mutex> lock(g_dismissedMutex);
            if (g_dismissedActors.count(a_actor->GetFormID())) {
                SKSE::log::info("[RESTORE_BLOCKED] '{}' is in dismissed blacklist — skipping restore.", a_actor->GetName());
                return;
            }
        }

        auto& runtimeData = a_actor->GetActorRuntimeData();
        bool hasTeammateFlag = runtimeData.boolBits.any(RE::Actor::BOOL_BITS::kPlayerTeammate);
        
        auto currentFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84E);
        bool hasFollowerFaction = currentFollowerFaction && a_actor->IsInFaction(currentFollowerFaction);

        if (!hasTeammateFlag || !hasFollowerFaction) {
            SKSE::log::info("Restoring reset ally {} state...", a_actor->GetName());
            SetupAllyFactionsAndAI(a_actor, a_player);
        }
    }

    void BehaviorManager::CallAllies() {
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;

        auto& traitMap = TraitManager::GetSingleton()->GetTraitMap();
        int count = 0;
        std::vector<RE::FormID> deadAllies;

        for (const auto& [formID, trait] : traitMap) {
            auto form = RE::TESForm::LookupByID(formID);
            if (form) {
                auto actor = form->As<RE::Actor>();
                if (actor && actor != player) {
                    if (actor->IsDeleted() || actor->IsDisabled() || actor->IsDead()) {
                        deadAllies.push_back(formID);
                        continue;
                    }

                    // Resetlenmiş durumları otomatik onar
                    RestoreAllyStateIfReset(actor, player);

                    auto& runtimeData = actor->GetActorRuntimeData();
                    if (runtimeData.boolBits.any(RE::Actor::BOOL_BITS::kPlayerTeammate)) {
                        auto processLists = RE::ProcessLists::GetSingleton();
                        if (processLists) {
                            processLists->StopCombatAndAlarmOnActor(actor, true);
                        }
                        actor->StopCombat();
                        actor->DrawWeaponMagicHands(false);
                        actor->MoveTo(player);
                        actor->EvaluatePackage(true, true);
                        count++;
                    }
                }
            }
        }

        // Ölen aktörleri takip listesinden tamamen sil
        for (auto id : deadAllies) {
            traitMap.erase(id);
        }

        if (count > 0) {
            RE::DebugNotification("Allies called to your position.");
        }
    }

    RE::BSEventNotifyControl BehaviorManager::ProcessEvent(const RE::TESCellFullyLoadedEvent* a_event, RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*) {
        if (!a_event || !a_event->cell) return RE::BSEventNotifyControl::kContinue;

        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return RE::BSEventNotifyControl::kContinue;

        // Herhangi bir hücre yüklendiğinde (han, zindan, kapı vb.), müttefik temizliğini bir kare sonra ana iş parçacığında çalıştır.
        // Bu sayede oyun motorunun hücre geçişini tamamen bitirmesini bekleyip savaşı ve alarm durumlarını mükemmel şekilde sıfırlayabiliriz.
        auto taskQueue = SKSE::GetTaskInterface();
        if (taskQueue) {
            taskQueue->AddTask([]() {
                auto player = RE::PlayerCharacter::GetSingleton();
                if (!player) return;

                auto& traitMap = TraitManager::GetSingleton()->GetTraitMap();
                std::vector<RE::FormID> deadAllies;

                for (const auto& [formID, trait] : traitMap) {
                    auto form = RE::TESForm::LookupByID(formID);
                    if (form) {
                        auto actor = form->As<RE::Actor>();
                        if (actor && actor != player) {
                            if (actor->IsDeleted() || actor->IsDisabled() || actor->IsDead()) {
                                deadAllies.push_back(formID);
                                continue;
                            }

                            // Resetlenmiş durumları otomatik onar
                            RestoreAllyStateIfReset(actor, player);

                            auto& runtimeData = actor->GetActorRuntimeData();
                            if (runtimeData.boolBits.any(RE::Actor::BOOL_BITS::kPlayerTeammate)) {
                                // Savaş durumlarını ve silahlarını her yüklemeden sonra kesin olarak sıfırla!
                                auto processLists = RE::ProcessLists::GetSingleton();
                                if (processLists) {
                                    processLists->StopCombatAndAlarmOnActor(actor, true);
                                }
                                actor->StopCombat();
                                actor->DrawWeaponMagicHands(false);

                                // Eğer farklı bir hücredelerse veya oyuncudan çok uzaktalarsa yanına ışınla
                                if (actor->GetParentCell() != player->GetParentCell() || 
                                    actor->GetPosition().GetDistance(player->GetPosition()) > 4000.0f) {
                                    SKSE::log::info("Teleporting ally {} to player on cell load task.", actor->GetName());
                                    actor->MoveTo(player);

                                    // Işınlandıktan sonra savaşı ve alarmı tekrar sıfırla
                                    if (processLists) {
                                        processLists->StopCombatAndAlarmOnActor(actor, true);
                                    }
                                    actor->StopCombat();
                                    actor->DrawWeaponMagicHands(false);
                                }
                                actor->EvaluatePackage(true, true);
                            }
                        }
                    }
                }

                // Ölen aktörleri takip listesinden tamamen sil
                for (auto id : deadAllies) {
                    traitMap.erase(id);
                }

                // Dinamik Klonların Temizliği (1 oyun içi gün geçtikten sonra silme)
                {
                    std::lock_guard<std::mutex> lock(g_cleanupMutex);
                    float currentTime = RE::Calendar::GetSingleton() ? RE::Calendar::GetSingleton()->GetCurrentGameTime() : 0.0f;
                    
                    std::vector<RE::FormID> toDelete;
                    for (const auto& [cloneID, dismissTime] : g_dismissedClones) {
                        // 1.0f oyun içi gün = 24 saat
                        if (currentTime >= dismissTime + 1.0f) {
                            toDelete.push_back(cloneID);
                        }
                    }

                    for (auto cloneID : toDelete) {
                        auto form = RE::TESForm::LookupByID(cloneID);
                        auto actor = form ? form->As<RE::Actor>() : nullptr;
                        if (actor) {
                            SKSE::log::info("[CLEANUP] 1 in-game day passed. Disabling and deleting dynamic clone '{}' (FormID={:08X}).", actor->GetName(), cloneID);
                            actor->StopCombat();
                            actor->Disable();
                            actor->SetDelete(true);
                        }
                        g_dismissedClones.erase(cloneID);
                    }
                }
            });
        }

        return RE::BSEventNotifyControl::kContinue;
    }

    // -----------------------------------------------------------------------
    // TESCombatEvent: Log whenever an actor in our traitMap enters/exits combat
    // -----------------------------------------------------------------------
    RE::BSEventNotifyControl BehaviorManager::ProcessEvent(
        const RE::TESCombatEvent* a_event,
        RE::BSTEventSource<RE::TESCombatEvent>*)
    {
        if (!a_event) return RE::BSEventNotifyControl::kContinue;

        // TESCombatEvent fields are NiPointer<TESObjectREFR> — cast to Actor*
        RE::Actor* actor  = a_event->actor       ? a_event->actor->As<RE::Actor>()       : nullptr;
        RE::Actor* target = a_event->targetActor ? a_event->targetActor->As<RE::Actor>() : nullptr;
        if (!actor) return RE::BSEventNotifyControl::kContinue;

        auto& traitMap = TraitManager::GetSingleton()->GetTraitMap();
        bool actorIsAlly  = traitMap.contains(actor->GetFormID());
        bool targetIsAlly = target && traitMap.contains(target->GetFormID());

        auto player = RE::PlayerCharacter::GetSingleton();
        bool targetIsPlayer = target && player && (target == player);

        // Only log if our mod has a stake in this event
        if (!actorIsAlly && !targetIsAlly) return RE::BSEventNotifyControl::kContinue;

        const char* state = "UNKNOWN";
        switch (a_event->newState.get()) {
            case RE::ACTOR_COMBAT_STATE::kNone:       state = "STOPPED";    break;
            case RE::ACTOR_COMBAT_STATE::kCombat:     state = "IN_COMBAT";  break;
            case RE::ACTOR_COMBAT_STATE::kSearching:  state = "SEARCHING";  break;
        }

        auto avOwner = actor->AsActorValueOwner();
        float aggression = avOwner ? avOwner->GetBaseActorValue(RE::ActorValue::kAggression) : -1.f;
        bool  hasTeammate = actor->GetActorRuntimeData().boolBits.any(RE::Actor::BOOL_BITS::kPlayerTeammate);
        bool  isHostileToPlayer = player && actor->IsHostileToActor(player);

        SKSE::log::info(
            "[COMBAT_EVENT] Actor='{}' (FormID={:08X}, Ally={}, Teammate={}, Aggr={:.1f}, HostileToPlayer={}) "
            "-> state={}, Target='{}'{}",
            actor->GetName(),
            actor->GetFormID(),
            actorIsAlly,
            hasTeammate,
            aggression,
            isHostileToPlayer,
            state,
            target ? target->GetName() : "(none)",
            targetIsPlayer ? " [[[TARGET IS PLAYER]]]" : ""
        );

        // Müttefikimiz oyuncuya veya başka bir müttefike saldırıyorsa debounce ile müdahale et
        if (actorIsAlly && (targetIsPlayer || targetIsAlly) &&
            a_event->newState.get() == RE::ACTOR_COMBAT_STATE::kCombat) {

            // Kara listede olan (kovulmuş) aktörlere müdahale etme!
            {
                std::lock_guard<std::mutex> lock(g_dismissedMutex);
                if (g_dismissedActors.count(actor->GetFormID())) {
                    SKSE::log::info("[COMBAT_EVENT] '{}' is in dismissed blacklist — skipping RE-PACIFY.", actor->GetName());
                    return RE::BSEventNotifyControl::kContinue;
                }
            }

            NPCTrait currentTrait = traitMap.count(actor->GetFormID()) ? traitMap.at(actor->GetFormID()) : NPCTrait::None;

            SKSE::log::warn(
                "[COMBAT_EVENT] !!! ALLY '{}' ATTACKING PLAYER OR ALLY! Trait={}, Teammate={}, Aggr={:.1f}. RE-PACIFYING...",
                actor->GetName(), static_cast<int>(currentTrait), hasTeammate, aggression
            );

            // Treacherous ise — ihanet bekleniyor, müdahale etme
            if (currentTrait == NPCTrait::Treacherous) {
                SKSE::log::info("[COMBAT_EVENT] Treacherous trait — intended betrayal, skipping.");
                return RE::BSEventNotifyControl::kContinue;
            }

            // Debounce: bu aktör zaten işleniyorsa yeni task kuyruğa alma
            RE::FormID actorID = actor->GetFormID();
            {
                std::lock_guard<std::mutex> lock(g_pacifyMutex);
                if (g_beingPacified.count(actorID)) {
                    return RE::BSEventNotifyControl::kContinue; // Zaten işleniyor
                }
                g_beingPacified.insert(actorID);
            }

            RE::ActorHandle actorHandle = actor->GetHandle();
            SKSE::GetTaskInterface()->AddTask([actorHandle, actorID, player]() {
                // Debounce'u serbest bırak
                auto cleanup = [&actorID]() {
                    std::lock_guard<std::mutex> lock(g_pacifyMutex);
                    g_beingPacified.erase(actorID);
                };

                auto actorPtr = actorHandle.get();
                if (!actorPtr || actorPtr->IsDead()) { cleanup(); return; }
                RE::Actor* safeActor = actorPtr.get();

                SKSE::log::info("[RE-PACIFY] Applying to '{}'...", safeActor->GetName());

                // 1. Sadece aktörün kendi savaşını durdur — player->StopCombat() yok
                //    (player->StopCombat() motoru tekrar tetikleyerek döngü yaratıyor)
                auto processLists = RE::ProcessLists::GetSingleton();
                if (processLists) {
                    processLists->StopCombatAndAlarmOnActor(safeActor, true);
                }
                safeActor->StopInteractingQuick(true);
                safeActor->StopCombat();

                // 2. Faction + ilişki + AI değerlerini tekrar uygula
                SetupAllyFactionsAndAI(safeActor, player);

                // 2B. Diğer tüm aktif müttefiklerle olan ilişkileri müttefik (3) olarak pekiştir
                auto& traitMap = TraitManager::GetSingleton()->GetTraitMap();
                for (const auto& [otherFormID, otherTrait] : traitMap) {
                    if (otherFormID != safeActor->GetFormID()) {
                        auto otherForm = RE::TESForm::LookupByID(otherFormID);
                        if (otherForm) {
                            auto otherActor = otherForm->As<RE::Actor>();
                            if (otherActor && !otherActor->IsDead()) {
                                SetRelationshipRank(safeActor, otherActor, 3);
                                SetRelationshipRank(otherActor, safeActor, 3);
                                otherActor->StopCombat();
                            }
                        }
                    }
                }

                // 3. Silahı indir — EvaluatePackage yok
                safeActor->DrawWeaponMagicHands(false);

                SKSE::log::info("[RE-PACIFY] '{}' done. Releasing debounce.", safeActor->GetName());
                cleanup();
            });
        }

        return RE::BSEventNotifyControl::kContinue;
    }


    // -----------------------------------------------------------------------
    // LogAllyStatus: Snapshot of all tracked allies — useful for debugging
    // -----------------------------------------------------------------------
    void BehaviorManager::LogAllyStatus(const char* a_context) {
        auto& traitMap = TraitManager::GetSingleton()->GetTraitMap();
        auto player = RE::PlayerCharacter::GetSingleton();

        SKSE::log::info("[ALLY_STATUS @ {}] {} tracked actor(s):", a_context, traitMap.size());
        for (const auto& [formID, trait] : traitMap) {
            auto form = RE::TESForm::LookupByID(formID);
            auto actor = form ? form->As<RE::Actor>() : nullptr;
            if (!actor) {
                SKSE::log::info("  - FormID={:08X} -> Actor not found (dead/unloaded).", formID);
                continue;
            }

            auto avOwner = actor->AsActorValueOwner();
            float aggr   = avOwner ? avOwner->GetBaseActorValue(RE::ActorValue::kAggression) : -1.f;
            float assist = avOwner ? avOwner->GetBaseActorValue(RE::ActorValue::kAssistance)  : -1.f;
            float moral  = avOwner ? avOwner->GetBaseActorValue(RE::ActorValue::kMorality)    : -1.f;
            bool  tm     = actor->GetActorRuntimeData().boolBits.any(RE::Actor::BOOL_BITS::kPlayerTeammate);
            bool  hostile = player && actor->IsHostileToActor(player);
            bool  dead    = actor->IsDead();

            auto currentFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84E);
            bool hasFaction = currentFollowerFaction && actor->IsInFaction(currentFollowerFaction);

            SKSE::log::info(
                "  - '{}' (FormID={:08X}, Trait={}, Dead={}, Teammate={}, FollowerFaction={}, "
                "HostileToPlayer={}, Aggr={:.1f}, Assist={:.1f}, Moral={:.1f})",
                actor->GetName(), formID, static_cast<int>(trait),
                dead, tm, hasFaction, hostile, aggr, assist, moral
            );
        }
    }
}
