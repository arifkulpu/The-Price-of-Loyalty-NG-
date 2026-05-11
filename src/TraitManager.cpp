#include "TraitManager.hpp"
#include "Settings.hpp"
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

        auto settings = Settings::GetSingleton();

        float baseCost = static_cast<float>(settings->baseBribeCost);
        
        // Scale by level
        baseCost += a_actor->GetLevel() * static_cast<float>(settings->costPerLevel);

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
        // Apply Global Difficulty Multiplier
        baseCost *= settings->baseDifficulty;

        return baseCost;
    }

    bool TraitManager::RollForSuccess(RE::Actor* a_actor, float a_goldOffered) {
        float required = CalculateBribeCost(a_actor);

        // Speech skill influence (0-100 -> 1.0x - 0.5x discount)
        auto player = RE::PlayerCharacter::GetSingleton();
        if (player) {
            auto avOwner = player->AsActorValueOwner();
            if (avOwner) {
                float speech = avOwner->GetActorValue(RE::ActorValue::kSpeech);
                speech = std::clamp(speech, 0.0f, 100.0f);
                float discountFactor = 1.0f - (speech / 100.0f) * 0.5f;
                required *= discountFactor;
            }
        }

        // Probability based success
        // Base chance is 75% if offered == required.
        // Formula: (offered / required) * 75.0
        float ratio = a_goldOffered / required;
        float chance = ratio * 75.0f;

        // Cap the maximum chance at 95% to ensure it's never 100% guaranteed
        if (chance > 95.0f) chance = 95.0f;

        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(0.0f, 100.0f);

        float roll = dis(gen);
        bool success = (roll <= chance);

        SKSE::log::debug("Bribe Roll: Offered=%.0f, Required=%.0f, Ratio=%.2f, Chance=%.1f%%, Roll=%.1f, Success=%s",
                         a_goldOffered, required, ratio, chance, roll, success ? "YES" : "NO");

        return success;
    }
}
