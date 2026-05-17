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
                    trait = NPCTrait::Treacherous; // Update local variable for the switch below
                    SKSE::log::info("Bandit-like NPC {} decided to betray because of {} bribe (Chance: {}%).", 
                                     a_actor->GetName(), a_isLowOffer ? "LOW" : "HIGH", betrayalChance);
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
                player->StopCombat(); // Oyuncunun hedefini temizle
            }

            // ÇATIŞMAYI SIFIRLA VE TAKIM ARKADAŞI İLİŞKİLERİNİ GÜNCELLE:
            // 1. Çevredeki TÜM aktörlerin savaş durumunu durduruyoruz.
            if (processLists) {
                for (auto& handle : processLists->highActorHandles) {
                    auto actor = handle.get();
                    if (actor) {
                        actor->StopCombat();
                        actor->EvaluatePackage(true, true);
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

            // Animasyon Bug'ını Düzelt: Animated Armoury gibi modlar özel animasyonlar kullandığı için
            // standart WeapUnequip bazen yetersiz kalabiliyor. Engine'in kendi fonksiyonunu kullanalım.
            a_actor->DrawWeaponMagicHands(false);
            a_actor->StopCombat(); // İlişkiler değiştikten sonra ana aktörün savaşını ve hedeflerini tekrar durdur
            a_actor->EvaluatePackage(true, true); // AI paketlerini ve müttefik ilişkilerini zorla yenile

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
        SKSE::log::info("Treacherous NPC {} will betray in 5-10 seconds.", a_actor->GetName());

        // Oyuncuya uyarı: "Bir şeyler yanlış hissettiriyor..."
        RE::DebugNotification("Something feels off about this one...");

        // Thread güvenliği: Ham pointer yerine ActorHandle alıyoruz (CTD önlemi)
        RE::ActorHandle actorHandle = a_actor->GetHandle();

        // Ayarları al
        auto settings = Settings::GetSingleton();
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

        if (a_event->cell == player->GetParentCell()) {
            SKSE::log::info("Player cell changed or fully loaded. Checking allies...");

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
                            // Savaş durumlarını ve silahlarını her hücre yüklenmesinde/ışınlanmasında HER ZAMAN sıfırla!
                            auto processLists = RE::ProcessLists::GetSingleton();
                            if (processLists) {
                                processLists->StopCombatAndAlarmOnActor(actor, true);
                            }
                            actor->StopCombat();
                            actor->DrawWeaponMagicHands(false);

                            // Eğer farklı bir hücredelerse veya oyuncudan çok uzaktalarsa yanına ışınla
                            if (actor->GetParentCell() != player->GetParentCell() || 
                                actor->GetPosition().GetDistance(player->GetPosition()) > 4000.0f) {
                                SKSE::log::info("Teleporting ally {} to player.", actor->GetName());
                                actor->MoveTo(player);
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
        }

        return RE::BSEventNotifyControl::kContinue;
    }
}
