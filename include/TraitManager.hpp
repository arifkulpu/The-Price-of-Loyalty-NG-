#pragma once

#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <map>

namespace Loyalty {
    enum class NPCTrait {
        None,
        Greedy,
        Honorable,
        Treacherous
    };

    class TraitManager {
    public:
        static TraitManager* GetSingleton() {
            static TraitManager singleton;
            return &singleton;
        }

        NPCTrait GetTrait(RE::Actor* a_actor);
        NPCTrait PeekTrait(RE::Actor* a_actor);          // For cost calc — no traitMap write
        NPCTrait GetOrAssignTrait(RE::Actor* a_actor);   // For successful bribe — writes to traitMap
        void SetTrait(RE::Actor* a_actor, NPCTrait a_trait);

        float CalculateBribeCost(RE::Actor* a_actor);
        bool RollForSuccess(RE::Actor* a_actor, float a_goldOffered);

        std::map<RE::FormID, NPCTrait>& GetTraitMap() { return _traitMap; }
        void Clear() { _traitMap.clear(); }

    private:
        TraitManager() = default;
        std::map<RE::FormID, NPCTrait> _traitMap;
        std::map<RE::FormID, NPCTrait> _previewMap; // Temporary cache, not saved

        NPCTrait GenerateRandomTrait(RE::Actor* a_actor); // Pure random, no map write
        NPCTrait AssignRandomTrait(RE::Actor* a_actor);   // Legacy, kept for compatibility
    };
}
