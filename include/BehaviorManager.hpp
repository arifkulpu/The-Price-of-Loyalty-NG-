#pragma once

#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>

namespace Loyalty {
    class BehaviorManager : public RE::BSTEventSink<RE::TESCellFullyLoadedEvent> {
    public:
        static BehaviorManager* GetSingleton() {
            static BehaviorManager singleton;
            return &singleton;
        }

        static void ProcessBribeResult(RE::Actor* a_actor, bool a_success, bool a_isLowOffer = false);
        static void DismissAlly(RE::Actor* a_actor);
        static void CallAllies();

        RE::BSEventNotifyControl ProcessEvent(const RE::TESCellFullyLoadedEvent* a_event, RE::BSTEventSource<RE::TESCellFullyLoadedEvent>* a_eventSource) override;
        
    private:
        BehaviorManager() = default;

        static void HandleTreacherousBehavior(RE::Actor* a_actor);
        static void HandleHonorableBehavior(RE::Actor* a_actor);
        static void HandleGreedyBehavior(RE::Actor* a_actor);
    };
}
