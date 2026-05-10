#pragma once

#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>

namespace Loyalty {
    class EffectManager {
    public:
        static EffectManager* GetSingleton() {
            static EffectManager singleton;
            return &singleton;
        }

        void PlayAcceptanceEffects(RE::Actor* a_actor);
        void PlayRejectionEffects(RE::Actor* a_actor);

    private:
        EffectManager() = default;

        void PlayCoinSound(RE::Actor* a_actor);
        void PlayDialogue(RE::Actor* a_actor, bool a_success);
    };
}
