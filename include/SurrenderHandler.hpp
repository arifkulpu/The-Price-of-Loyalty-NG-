#pragma once

#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>

namespace Loyalty {
    class SurrenderHandler {
    public:
        static void InstallHooks();
        static void Update(); // Called periodically or via hook

    private:
        static void HandleBleedout(RE::Actor* a_actor);
    };
}
