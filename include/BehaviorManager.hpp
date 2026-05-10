#pragma once

#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>

namespace Loyalty {
    class BehaviorManager {
    public:
        static BehaviorManager* GetSingleton() {
            static BehaviorManager singleton;
            return &singleton;
        }

        void ProcessBribeResult(RE::Actor* a_actor, bool a_success);
        
    private:
        BehaviorManager() = default;

        void HandleTreacherousBehavior(RE::Actor* a_actor);
        void HandleHonorableBehavior(RE::Actor* a_actor);
        void HandleGreedyBehavior(RE::Actor* a_actor);
    };
}
