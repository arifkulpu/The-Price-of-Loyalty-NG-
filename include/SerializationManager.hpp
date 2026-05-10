#pragma once

#include <SKSE/SKSE.h>

namespace Loyalty {
    class SerializationManager {
    public:
        static constexpr uint32_t SerializationID = 'LOYT';
        static constexpr uint32_t SerializationVersion = 1;

        static void Register();

        static void Save(SKSE::SerializationInterface* a_intfc);
        static void Load(SKSE::SerializationInterface* a_intfc);
        static void Revert(SKSE::SerializationInterface* a_intfc);
    };
}
