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
            
            // 1. Stop combat immediately
            a_actor->StopCombat();
            
            // 2. Set Player Teammate flag (Bitflag method - tested and working)
            auto& runtimeData = a_actor->GetActorRuntimeData();
            runtimeData.boolBits.set(RE::Actor::BOOL_BITS::kPlayerTeammate);

            // 3. Set Aggression, Confidence and ASSISTANCE
            auto avOwner = a_actor->AsActorValueOwner();
            if (avOwner) {
                avOwner->SetBaseActorValue(RE::ActorValue::kAggression, 0.0f);
                avOwner->SetBaseActorValue(RE::ActorValue::kConfidence, 4.0f);
                avOwner->SetBaseActorValue(RE::ActorValue::kAssistance, 2.0f); 
            }

            // 4. Force AI Evaluation
            a_actor->EvaluatePackage(true, true);

            // 5. Add to Follower Faction
            auto followerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84D);
            if (followerFaction) {
                a_actor->AddToFaction(followerFaction, 0);
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
