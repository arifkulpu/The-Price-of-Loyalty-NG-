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
    void BehaviorManager::ProcessBribeResult(RE::Actor* a_actor, bool a_success) {
        if (!a_actor) return;

        if (a_success) {
            EffectManager::GetSingleton()->PlayAcceptanceEffects(a_actor);
            
            a_actor->NotifyAnimationGraph("IdleForceDefaultState");
            a_actor->StopInteractingQuick(true);
            a_actor->StopCombat();
            
            NPCTrait trait = TraitManager::GetSingleton()->GetTrait(a_actor);
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 100);
            int roll = dis(gen);

            // SPECIAL CASE: Runaway (Parayı alıp kaçanlar)
            // Chance: Greedy (40%), Others (10%)
            bool shouldFlee = (trait == NPCTrait::Greedy && roll <= 40) || (roll <= 10);

            if (shouldFlee) {
                auto avOwner = a_actor->AsActorValueOwner();
                if (avOwner) {
                    avOwner->SetBaseActorValue(RE::ActorValue::kAggression, 0.0f);
                    avOwner->SetBaseActorValue(RE::ActorValue::kConfidence, 0.0f);
                    avOwner->SetBaseActorValue(RE::ActorValue::kAssistance, 0.0f);
                }

                auto player = RE::PlayerCharacter::GetSingleton();

                // Önce her iki tarafın da savaş durumunu temizle
                // (aksi hâlde savaş müziği çalmaya devam eder)
                a_actor->StopCombat();
                if (player) {
                    player->StopCombat();
                }

                // a_combatMode = false: NPC kaçış sırasında savaş modunda olmayacak
                // Bu sayede oyun motoru combat state'i temizler ve müzik durur
                a_actor->InitiateFlee(player, true, true, false, nullptr, nullptr, 0.0f, 2000.0f);

                a_actor->EvaluatePackage(true, true);
                RE::DebugNotification("He took the gold and ran away!");
                return; // Do not become a teammate
            }


            // Normal Teammate Setup
            auto& runtimeData = a_actor->GetActorRuntimeData();
            runtimeData.boolBits.set(RE::Actor::BOOL_BITS::kPlayerTeammate);
            runtimeData.boolFlags.reset(RE::Actor::BOOL_FLAGS::kIsCommandedActor);

            auto avOwner = a_actor->AsActorValueOwner();
            if (avOwner) {
                avOwner->SetBaseActorValue(RE::ActorValue::kAggression, 1.0f); // Aggressive (saldırgan)
                avOwner->SetBaseActorValue(RE::ActorValue::kConfidence, 4.0f); // Foolhardy (korkusuz)
                avOwner->SetBaseActorValue(RE::ActorValue::kAssistance, 2.0f); // Helps friends and allies
                avOwner->SetBaseActorValue(RE::ActorValue::kWaitingForPlayer, 0.0f);
            }

            a_actor->EvaluatePackage(true, true);
            
            // Factions: Add to Follower and Player factions
            auto followerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84D);
            if (followerFaction) {
                a_actor->AddToFaction(followerFaction, 0);
            }
            auto playerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x00000013);
            if (playerFaction) {
                a_actor->AddToFaction(playerFaction, 0);
            }

            // Remove from common hostile factions so guards/adventurers don't attack them
            auto banditFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0001B0E4);
            if (banditFaction) a_actor->RemoveFromFaction(banditFaction);

            auto forswornFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x00043599);
            if (forswornFaction) a_actor->RemoveFromFaction(forswornFaction);
            
            auto warlockFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0003DF17);
            if (warlockFaction) a_actor->RemoveFromFaction(warlockFaction);
            
            auto vampireFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x00027242);
            if (vampireFaction) a_actor->RemoveFromFaction(vampireFaction);

            auto player = RE::PlayerCharacter::GetSingleton();
            if (player) {
                player->StopCombat(); // Oyuncunun hedefini temizle
            }

            // ÇATIŞMAYI SIFIRLA: 
            // Daha önce rüşvet verdiğimiz müttefiklerin yeni katılanı düşman olarak görüp saldırmasını engellemek için
            // çevredeki tüm müttefiklerin (ve yeni NPC'nin) savaş durumunu durduruyoruz.
            auto processLists = RE::ProcessLists::GetSingleton();
            if (processLists) {
                for (auto& handle : processLists->highActorHandles) {
                    auto actor = handle.get();
                    if (actor) {
                        auto& rd = actor->GetActorRuntimeData();
                        // Eğer bu aktör bir müttefikse VEYA yeni katılan aktörse
                        if (rd.boolBits.any(RE::Actor::BOOL_BITS::kPlayerTeammate) || actor.get() == a_actor) {
                            actor->StopCombat();
                        }
                    }
                }
            }

            // Animasyon Bug'ını Düzelt: Savaştan zorla çıkarıldıkları için animasyonlar bug'a girebiliyor
            // (Silahı elde tutup saldırmama sorunu). Silahlarını kınlarına sokmaya zorla ve state'i sıfırla.
            a_actor->DrawWeaponMagicHands(false);
            a_actor->NotifyAnimationGraph("IdleForceDefaultState");

            if (player) {
                a_actor->MoveTo(player);
            }

            RE::DebugNotification("NPC is now your loyal teammate.");
            
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

        auto avOwner = a_actor->AsActorValueOwner();
        if (avOwner) {
            avOwner->SetBaseActorValue(RE::ActorValue::kAggression, 1.0f);
            avOwner->SetBaseActorValue(RE::ActorValue::kConfidence, 2.0f); 
            avOwner->SetBaseActorValue(RE::ActorValue::kAssistance, 0.0f);
            avOwner->SetBaseActorValue(RE::ActorValue::kWaitingForPlayer, 0.0f);
        }

        auto followerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84D);
        if (followerFaction) {
            a_actor->RemoveFromFaction(followerFaction);
        }

        a_actor->NotifyAnimationGraph("IdleForceDefaultState");
        a_actor->EvaluatePackage(true, true);

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

                // Düşman ilişki seviyesine ayarla
                SetRelationshipRank(safeActor, player, -4);

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

    void BehaviorManager::CallAllies() {
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;

        auto processLists = RE::ProcessLists::GetSingleton();
        if (!processLists) return;

        int count = 0;
        for (auto& handle : processLists->highActorHandles) {
            auto actor = handle.get();
            if (actor && actor.get() != player) {
                auto& runtimeData = actor->GetActorRuntimeData();
                if (runtimeData.boolBits.any(RE::Actor::BOOL_BITS::kPlayerTeammate)) {
                    actor->MoveTo(player);
                    actor->EvaluatePackage(true, true);
                    count++;
                }
            }
        }

        if (count > 0) {
            RE::DebugNotification("Allies called to your position.");
        }
    }
}
