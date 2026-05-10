#include "BehaviorManager.hpp"
#include "TraitManager.hpp"
#include "SurrenderHandler.hpp"
#include "EffectManager.hpp"
#include <RE/C/Character.h>
#include <RE/A/ActorValueOwner.h>
#include <random>

namespace Loyalty {
    void BehaviorManager::ProcessBribeResult(RE::Actor* a_actor, bool a_success) {
        if (!a_actor) return;

        if (a_success) {
            EffectManager::GetSingleton()->PlayAcceptanceEffects(a_actor);
            
            // 1. Reset state and stop moaning (Removed kIsCommandedActor)
            a_actor->NotifyAnimationGraph("IdleForceDefaultState");
            a_actor->StopInteractingQuick(true);
            a_actor->StopCombat();
            
            // 2. Set Player Teammate flag (This is enough for ally status)
            auto& runtimeData = a_actor->GetActorRuntimeData();
            runtimeData.boolBits.set(RE::Actor::BOOL_BITS::kPlayerTeammate);
            runtimeData.boolFlags.reset(RE::Actor::BOOL_FLAGS::kIsCommandedActor); // ENSURE NO MOANING

            // 3. Set AI Values
            auto avOwner = a_actor->AsActorValueOwner();
            if (avOwner) {
                avOwner->SetBaseActorValue(RE::ActorValue::kAggression, 0.0f);
                avOwner->SetBaseActorValue(RE::ActorValue::kConfidence, 4.0f);
                avOwner->SetBaseActorValue(RE::ActorValue::kAssistance, 2.0f);
                avOwner->SetBaseActorValue(RE::ActorValue::kWaitingForPlayer, 0.0f);
            }

            // 4. Force AI Evaluation
            a_actor->EvaluatePackage(true, true);

            // 5. Add to Follower Faction
            auto followerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84D);
            if (followerFaction) {
                a_actor->AddToFaction(followerFaction, 0);
            }

            // 6. Instant Teleport to Player (To ensure they start following immediately)
            auto player = RE::PlayerCharacter::GetSingleton();
            if (player) {
                a_actor->MoveTo(player);
            }

            RE::DebugNotification("NPC is now your loyal teammate.");
            
            NPCTrait trait = TraitManager::GetSingleton()->GetTrait(a_actor);
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
        SKSE::log::info("Dismissed NPC: {}", a_actor->GetName());
    }

    void BehaviorManager::HandleTreacherousBehavior(RE::Actor* a_actor) {
        SKSE::log::info("Treacherous NPC {} accepted but watch your back.", a_actor->GetName());
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
