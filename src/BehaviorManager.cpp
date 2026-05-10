#include "BehaviorManager.hpp"
#include "TraitManager.hpp"
#include "SurrenderHandler.hpp"
#include "EffectManager.hpp"
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
                    avOwner->SetBaseActorValue(RE::ActorValue::kConfidence, 0.0f); // Cowardly
                    avOwner->SetBaseActorValue(RE::ActorValue::kAssistance, 0.0f);
                }
                
                // Properly initiate flee from player
                // Parameters: a_fleeRef (player), a_runOnce, a_knows, a_combatMode, a_cell, a_ref (target ref), a_fleeFromDist, a_fleeToDist
                auto player = RE::PlayerCharacter::GetSingleton();
                a_actor->InitiateFlee(player, true, true, true, nullptr, nullptr, 0.0f, 2000.0f);
                
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
                avOwner->SetBaseActorValue(RE::ActorValue::kAggression, 0.0f);
                avOwner->SetBaseActorValue(RE::ActorValue::kConfidence, 4.0f);
                avOwner->SetBaseActorValue(RE::ActorValue::kAssistance, 2.0f);
                avOwner->SetBaseActorValue(RE::ActorValue::kWaitingForPlayer, 0.0f);
            }

            a_actor->EvaluatePackage(true, true);
            auto followerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84D);
            if (followerFaction) {
                a_actor->AddToFaction(followerFaction, 0);
            }

            auto player = RE::PlayerCharacter::GetSingleton();
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

        // Rastgele 5-10 saniyelik gecikme — ayrı thread'de beklenir,
        // ardından güvenli biçimde ana oyun thread'ine ihanet eylemi gönderilir.
        std::thread([a_actor]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(5000, 10000); // 5-10 saniye (ms)
            int delayMs = dis(gen);

            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

            // Oyun thread'ine gönder (thread-safe)
            SKSE::GetTaskInterface()->AddTask([a_actor]() {
                auto player = RE::PlayerCharacter::GetSingleton();
                if (!player || !a_actor) return;

                // Ölü veya yüklenmemiş aktörleri atla
                if (a_actor->IsDead()) return;

                // Müttefik statüsünü kaldır
                auto& runtimeData = a_actor->GetActorRuntimeData();
                runtimeData.boolBits.reset(RE::Actor::BOOL_BITS::kPlayerTeammate);

                // Düşman ilişki seviyesine ayarla
                SetRelationshipRank(a_actor, player, -4);

                auto avOwner = a_actor->AsActorValueOwner();
                if (avOwner) {
                    avOwner->SetBaseActorValue(RE::ActorValue::kAggression, 2.0f); // Çılgın saldırgan
                }

                // Savaşı başlat
                StartCombat(a_actor, player);
                a_actor->EvaluatePackage(true, true);

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
