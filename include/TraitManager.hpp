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
        void SetTrait(RE::Actor* a_actor, NPCTrait a_trait);

        float CalculateBribeCost(RE::Actor* a_actor);
        bool RollForSuccess(RE::Actor* a_actor, float a_goldOffered);

        std::map<RE::FormID, NPCTrait>& GetTraitMap() { return _traitMap; }
        void Clear() { _traitMap.clear(); }

    private:
        TraitManager() = default;
        std::map<RE::FormID, NPCTrait> _traitMap;

        NPCTrait AssignRandomTrait(RE::Actor* a_actor);
    };
}
