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

        static void ProcessBribeResult(RE::Actor* a_actor, bool a_success, bool a_isLowOffer = false);
        static void DismissAlly(RE::Actor* a_actor);
        static void CallAllies();
        
    private:
        BehaviorManager() = default;

        static void HandleTreacherousBehavior(RE::Actor* a_actor);
        static void HandleHonorableBehavior(RE::Actor* a_actor);
        static void HandleGreedyBehavior(RE::Actor* a_actor);
    };
}
