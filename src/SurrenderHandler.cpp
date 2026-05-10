#include "SurrenderHandler.hpp"

namespace Loyalty {
    void SurrenderHandler::InstallHooks() {
        SKSE::log::info("Surrender hooks installed (Simulated).");
    }

    void SurrenderHandler::HandleBleedout(RE::Actor* a_actor) {
        if (!a_actor) return;

        // IsBleedingOut lives on ActorState
        auto state = a_actor->AsActorState();
        if (!state || !state->IsBleedingOut()) return;

        SKSE::log::info("Actor {} is in bleedout. Ready for loyalty check.", a_actor->GetName());
    }

    void ForceLoyalty(RE::Actor* a_actor) {
        if (!a_actor) return;

        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;

        // StopCombat is a confirmed virtual on Actor (0E5)
        a_actor->StopCombat();

        // Mark as player teammate via the BOOL_FLAGS flag
        a_actor->GetActorRuntimeData().boolBits.set(RE::Actor::BOOL_BITS::kPlayerTeammate);

        SKSE::log::info("Loyalty forced for: {}.", a_actor->GetName());
    }
}
