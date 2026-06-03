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

        return NPCTrait::None; // Not in map = not bribed yet, return None
    }

    // PeekTrait: returns a stable trait for cost/UI purposes WITHOUT adding to traitMap.
    // Uses a separate preview cache keyed by FormID.
    NPCTrait TraitManager::PeekTrait(RE::Actor* a_actor) {
        if (!a_actor) return NPCTrait::None;

        // Check real map first
        auto it = _traitMap.find(a_actor->GetFormID());
        if (it != _traitMap.end()) {
            return it->second;
        }

        // Check preview cache
        auto prev = _previewMap.find(a_actor->GetFormID());
        if (prev != _previewMap.end()) {
            return prev->second;
        }

        // Generate and cache in preview map (not traitMap)
        NPCTrait trait = GenerateRandomTrait(a_actor);
        _previewMap[a_actor->GetFormID()] = trait;
        return trait;
    }

    // GetOrAssignTrait: called only on successful bribe. Moves from preview to real map.
    NPCTrait TraitManager::GetOrAssignTrait(RE::Actor* a_actor) {
        if (!a_actor) return NPCTrait::None;

        auto it = _traitMap.find(a_actor->GetFormID());
        if (it != _traitMap.end()) {
            return it->second;
        }

        // Promote from preview map if available, else generate fresh
        auto prev = _previewMap.find(a_actor->GetFormID());
        NPCTrait trait;
        if (prev != _previewMap.end()) {
            trait = prev->second;
            _previewMap.erase(prev);
        } else {
            trait = GenerateRandomTrait(a_actor);
        }

        _traitMap[a_actor->GetFormID()] = trait;
        return trait;
    }

    void TraitManager::SetTrait(RE::Actor* a_actor, NPCTrait a_trait) {
        if (a_actor) {
            _traitMap[a_actor->GetFormID()] = a_trait;
        }
    }

    NPCTrait TraitManager::GenerateRandomTrait(RE::Actor* a_actor) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        
        bool canBeTreacherous = false;
        if (a_actor) {
            static const RE::FormID lawlessFactions[] = {
                0x0001B0E4, // Bandit
                0x00043599, // Forsworn
                0x0003DF17, // Warlock
                0x00027242  // Vampire
            };
            for (auto id : lawlessFactions) {
                auto faction = RE::TESForm::LookupByID<RE::TESFaction>(id);
                if (faction && a_actor->IsInFaction(faction)) {
                    canBeTreacherous = true;
                    break;
                }
            }
        }

        std::uniform_int_distribution<> dis(1, canBeTreacherous ? 3 : 2);
        return static_cast<NPCTrait>(dis(gen));
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

        // Adjust by Trait — use PeekTrait (NO traitMap write!)
        NPCTrait trait = PeekTrait(a_actor);
        switch (trait) {
            case NPCTrait::Greedy:      baseCost *= 0.7f; break;
            case NPCTrait::Honorable:   baseCost *= 2.0f; break;
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
