#include "TraitManager.hpp"
#include <random>

namespace Loyalty {
    NPCTrait TraitManager::GetTrait(RE::Actor* a_actor) {
        if (!a_actor) return NPCTrait::None;

        auto it = _traitMap.find(a_actor->GetFormID());
        if (it != _traitMap.end()) {
            return it->second;
        }

        return AssignRandomTrait(a_actor);
    }

    void TraitManager::SetTrait(RE::Actor* a_actor, NPCTrait a_trait) {
        if (a_actor) {
            _traitMap[a_actor->GetFormID()] = a_trait;
        }
    }

    NPCTrait TraitManager::AssignRandomTrait(RE::Actor* a_actor) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1, 3);

        NPCTrait trait = static_cast<NPCTrait>(dis(gen));
        SetTrait(a_actor, trait);
        return trait;
    }

    float TraitManager::CalculateBribeCost(RE::Actor* a_actor) {
        if (!a_actor) return 0.0f;

        float baseCost = 100.0f;
        
        // Scale by level
        baseCost += a_actor->GetLevel() * 10.0f;

        // Scale by Social Status (simplified check)
        if (a_actor->IsGuard()) {
            baseCost *= 10.0f; // Guards are expensive to corrupt
        } else if (a_actor->IsInFaction(RE::TESForm::LookupByID<RE::TESFaction>(0x0001B0E4))) { // Bandit faction example
            baseCost *= 0.5f; // Bandits are cheap
        }

        // Adjust by Trait
        NPCTrait trait = GetTrait(a_actor);
        switch (trait) {
            case NPCTrait::Greedy: baseCost *= 0.7f; break;
            case NPCTrait::Honorable: baseCost *= 2.0f; break;
            case NPCTrait::Treacherous: baseCost *= 0.9f; break;
            default: break;
        }

        return baseCost;
    }

    bool TraitManager::RollForSuccess(RE::Actor* a_actor, float a_goldOffered) {
        float required = CalculateBribeCost(a_actor);
        
        // Simple success check: gold offered vs required
        // In a full version, this would include Speech skill influence
        if (a_goldOffered >= required) {
            return true;
        }

        return false;
    }
}
