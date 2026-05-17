#include "BehaviorManager.hpp"
#include "TraitManager.hpp"
#include "SurrenderHandler.hpp"
#include "EffectManager.hpp"
#include "Settings.hpp"
#include <RE/C/Character.h>
#include <RE/A/ActorValueOwner.h>
#include <REL/Relocation.h>
#include <random>
#include <thread>
#include <chrono>
#include <mutex>
#include <unordered_set>

// Debounce: prevent multiple simultaneous re-pacify tasks for the same actor
static std::mutex g_pacifyMutex;
static std::unordered_set<RE::FormID> g_beingPacified;

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
        auto currentFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84E);
        if (currentFollowerFaction) {
            a_actor->AddToFaction(currentFollowerFaction, 0);
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
            EffectManager::GetSingleton()->PlayAcceptanceEffects(a_actor);
            
            a_actor->StopInteractingQuick(true);
            a_actor->StopCombat();
            a_actor->DrawWeaponMagicHands(false);

            auto processLists = RE::ProcessLists::GetSingleton();
            if (processLists) {
                processLists->StopCombatAndAlarmOnActor(a_actor, true);
            }
            
            NPCTrait trait = TraitManager::GetSingleton()->GetTrait(a_actor);
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 100);
            int roll = dis(gen);

            // DYNAMIC BETRAYAL FOR BANDITS:
            // Low bribe -> chance from settings (default 60%)
            // High bribe -> chance from settings (default 15%)
            if (IsBanditLike(a_actor)) {
                auto settings = Settings::GetSingleton();
                int betrayalChance = a_isLowOffer ? settings->betrayalChanceLowBribe : settings->betrayalChanceHighBribe;
                if (roll <= betrayalChance) {
                    TraitManager::GetSingleton()->SetTrait(a_actor, NPCTrait::Treacherous);
                    trait = NPCTrait::Treacherous;
                    SKSE::log::info("[BRIBE] {} assigned TREACHEROUS trait (roll={}, chance={}%, offer={}).",
                                     a_actor->GetName(), roll, betrayalChance, a_isLowOffer ? "LOW" : "HIGH");
                } else {
                    SKSE::log::info("[BRIBE] {} NOT treacherous (roll={}, chance={}%, offer={}).",
                                     a_actor->GetName(), roll, betrayalChance, a_isLowOffer ? "LOW" : "HIGH");
                }
            }

            auto avOwner = a_actor->AsActorValueOwner();
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

            auto player = RE::PlayerCharacter::GetSingleton();
            if (player) {
                // Setup ally factions, relationships, and unaggressive AI securely
                SetupAllyFactionsAndAI(a_actor, player);
                player->StopCombat();
            }

            SKSE::log::info("[BRIBE] {} joined as ally. Trait={}, FormID={:08X}",
                a_actor->GetName(),
                static_cast<int>(trait),
                a_actor->GetFormID());
            LogAllyStatus("Post-Bribe");

            // ÇATIŞMAYI SIFIRLA:
            // Müttefik olan NPC ve çevresindeki aktörlerin savaşını durdur.
            // NOT: EvaluatePackage çağrılmıyor — çağrılırsa AI tekrar saldırı paketi seçiyor.
            if (processLists) {
                for (auto& handle : processLists->highActorHandles) {
                    auto nearby = handle.get();
                    if (nearby) {
                        nearby->StopCombat();
                        if (nearby.get() != a_actor && player && nearby->IsHostileToActor(player)) {
                            SetRelationshipRank(a_actor, nearby.get(), -3);
                            SetRelationshipRank(nearby.get(), a_actor, -3);
                        }
                    }
                }
            }

            // 2. Diğer aktif müttefiklerimiz (traitMap içindekiler) ile yeni katılan müttefik 
            // arasındaki ilişkiyi de 3 (Ally / Müttefik) yaparak birbirleriyle savaşmalarını engelliyoruz.
            // Bu sayede uzaktaki okçular veya farklı mesafedeki yoldaşlar asla gözden kaçmıyor!
            auto& traitMap = TraitManager::GetSingleton()->GetTraitMap();
            for (const auto& [otherFormID, otherTrait] : traitMap) {
                if (otherFormID != a_actor->GetFormID()) {
                    auto otherForm = RE::TESForm::LookupByID(otherFormID);
                    if (otherForm) {
                        auto otherActor = otherForm->As<RE::Actor>();
                        if (otherActor && !otherActor->IsDead()) {
                            SetRelationshipRank(a_actor, otherActor, 3); // 3 = Ally
                            SetRelationshipRank(otherActor, a_actor, 3); // 3 = Ally
                            
                            otherActor->StopCombat(); // İlişki değiştikten sonra tekrar durdur
                            otherActor->EvaluatePackage(true, true); // AI paketlerini zorla yenile
                        }
                    }
                }
            }

            // Silahı indir — EvaluatePackage yok (AI tekrar saldırı seçer)
            a_actor->DrawWeaponMagicHands(false);
            a_actor->StopCombat();

            RE::DebugNotification("NPC is now your loyal teammate. (Healed!)");
            
            // Trigger Trait Behaviors
            switch (trait) {
                case NPCTrait::Treacherous:
                    HandleTreacherousBehavior(a_actor);
                    break;
                case NPCTrait::Honorable:
                    HandleHonorableBehavior(a_actor);
                    break;
                case NPCTrait::Greedy:
                    HandleGreedyBehavior(a_actor);
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
        
        // Bu aktörü takip listesinden tamamen çıkar
        traitMap.erase(a_actor->GetFormID());

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
        a_actor->EvaluatePackage(true, true);

        // Rastgele karar: Ya saldıracak ya da kaçacak
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1, 100);
        int roll = dis(gen);

        if (player) {
            if (roll <= 50) {
                // Saldır
                StartCombat(a_actor, player);
                RE::DebugNotification("He is not happy about being dismissed and attacks!");
            } else {
                // Kaç
                if (avOwner) {
                    avOwner->SetBaseActorValue(RE::ActorValue::kAggression, 0.0f); // Korkaklaştır
                    avOwner->SetBaseActorValue(RE::ActorValue::kConfidence, 0.0f); // Korkaklaştır
                }
                a_actor->InitiateFlee(player, true, true, false, nullptr, nullptr, 0.0f, 2000.0f);
                a_actor->EvaluatePackage(true, true);
                RE::DebugNotification("He decided to run for his life!");
            }
        }

        RE::DebugNotification("You have dismissed your ally.");
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

        // Ölen aktörleri asla onarma!
        if (a_actor->IsDead()) {
            return;
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
                    if (actor->IsDead()) {
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
                            if (actor->IsDead()) {
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
