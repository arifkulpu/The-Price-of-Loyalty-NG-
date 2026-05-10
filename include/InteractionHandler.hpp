#pragma once

#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>

namespace Loyalty {
    class InteractionHandler : public RE::BSTEventSink<RE::InputEvent*> {
    public:
        static InteractionHandler* GetSingleton() {
            static InteractionHandler singleton;
            return &singleton;
        }

        static void Register() {
            auto input = RE::BSInputDeviceManager::GetSingleton();
            if (input) {
                input->AddEventSink(GetSingleton());
                SKSE::log::info("Input event sink registered.");
            }
        }

        RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;

    private:
        InteractionHandler() = default;
        void HandleInteraction();
        void ShowBribeMenu(RE::Actor* a_target);

    private:
        bool _isActive = false;
        const uint32_t _hotkey = 48; // 'B' key (Scan Code)
    };
}
