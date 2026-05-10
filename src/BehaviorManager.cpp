#include "BehaviorManager.hpp"
#include "TraitManager.hpp"
#include "SurrenderHandler.hpp"
#include "EffectManager.hpp"
#include <RE/A/Actor.h>
#include <RE/A/ActorValueOwner.h>
#include <random>

namespace Loyalty {
    void BehaviorManager::ProcessBribeResult(RE::Actor* a_actor, bool a_success) {
        if (!a_actor) return;

        if (a_success) {
            EffectManager::GetSingleton()->PlayAcceptanceEffects(a_actor);
            
            // --- CORE ALLY LOGIC ---
            // 1. Stop combat immediately
            a_actor->StopCombat();
            
            // 2. Set Player Teammate flag (This makes them follow and fight for you)
            auto& runtimeData = a_actor->GetActorRuntimeData();
            runtimeData.boolBits.set(RE::Actor::BOOL_BITS::kPlayerTeammate);

            // 3. Set Aggression and Confidence via ActorValueOwner
            auto avOwner = a_actor->AsActorValueOwner();
            if (avOwner) {
                avOwner->SetBaseActorValue(RE::ActorValue::kAggression, 0.0f);
                avOwner->SetBaseActorValue(RE::ActorValue::kConfidence, 4.0f);
            }

            RE::DebugNotification("NPC is now your ally.");
            // ------------------------

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

    void BehaviorManager::HandleTreacherousBehavior(RE::Actor* a_actor) {
        SKSE::log::info("Treacherous NPC {} accepted but watch your back.", a_actor->GetName());
    }

    void BehaviorManager::HandleHonorableBehavior(RE::Actor* a_actor) {
        SKSE::log::info("Honorable NPC {} is now a staunch ally.", a_actor->GetName());
    }

    void BehaviorManager::HandleGreedyBehavior(RE::Actor* a_actor) {
        SKSE::log::info("Greedy NPC {} is satisfied for now.", a_actor->GetName());
    }
}
