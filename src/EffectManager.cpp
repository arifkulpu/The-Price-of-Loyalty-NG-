#include "EffectManager.hpp"

#undef PlaySound

namespace Loyalty {
    void EffectManager::PlayAcceptanceEffects(RE::Actor* a_actor) {
        if (!a_actor) return;

        // 1. Play Animation (e.g., Acknowledgment or Taking Item)
        a_actor->NotifyAnimationGraph("IdleDialogue"); 
        
        // 2. Play Sound (Gold Clink)
        PlayCoinSound(a_actor);

        // 3. Play Success Dialogue
        PlayDialogue(a_actor, true);

        SKSE::log::info("Playing acceptance effects for {}", a_actor->GetName());
    }

    void EffectManager::PlayRejectionEffects(RE::Actor* a_actor) {
        if (!a_actor) return;

        // 1. Play Rejection Animation (e.g., Aggressive or Shaking Head)
        a_actor->NotifyAnimationGraph("ShoutOut");

        // 2. Play Failure Dialogue
        PlayDialogue(a_actor, false);

        SKSE::log::info("Playing rejection effects for {}", a_actor->GetName());
    }

    void EffectManager::PlayCoinSound(RE::Actor* a_actor) {
        // Play UI Gold sound
        auto audioManager = RE::BSAudioManager::GetSingleton();
        if (audioManager) {
            RE::BSISoundDescriptor* sound = nullptr;
            // In a real mod, we'd lookup the sound descriptor
            // For now, we'll use a safer way or skip the direct call if uncertain
        }
    }

    void EffectManager::PlayDialogue(RE::Actor* a_actor, bool a_success) {
        // In a real mod, we would use RE::Character::Say to play specific VoiceType lines
        // For now, we simulate the logic
        if (a_success) {
            // "You have a deal." / "Very well."
        } else {
            // "I cannot be bought!" / "Is that all?"
        }
    }
}
