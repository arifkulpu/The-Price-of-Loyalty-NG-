#include "SerializationManager.hpp"
#include "TraitManager.hpp"

namespace Loyalty {
    void SerializationManager::Register() {
        auto serialization = SKSE::GetSerializationInterface();
        serialization->SetUniqueID(SerializationID);
        serialization->SetSaveCallback(Save);
        serialization->SetLoadCallback(Load);
        serialization->SetRevertCallback(Revert);
        SKSE::log::info("Serialization callbacks registered.");
    }

    void SerializationManager::Save(SKSE::SerializationInterface* a_intfc) {
        if (!a_intfc->OpenRecord(SerializationID, SerializationVersion)) {
            SKSE::log::error("Failed to open record for saving.");
            return;
        }

        auto& traitMap = TraitManager::GetSingleton()->GetTraitMap();
        uint32_t numEntries = static_cast<uint32_t>(traitMap.size());

        a_intfc->WriteRecordData(numEntries);

        for (const auto& [formID, trait] : traitMap) {
            a_intfc->WriteRecordData(formID);
            a_intfc->WriteRecordData(static_cast<uint32_t>(trait));
        }

        SKSE::log::info("Saved {} NPC trait records.", numEntries);
    }

    void SerializationManager::Load(SKSE::SerializationInterface* a_intfc) {
        uint32_t type;
        uint32_t version;
        uint32_t length;

        while (a_intfc->GetNextRecordInfo(type, version, length)) {
            if (type != SerializationID) continue;
            if (version != SerializationVersion) {
                SKSE::log::error("Invalid record version.");
                continue;
            }

            uint32_t numEntries;
            a_intfc->ReadRecordData(numEntries);

            for (uint32_t i = 0; i < numEntries; ++i) {
                RE::FormID oldFormID;
                uint32_t traitValue;

                a_intfc->ReadRecordData(oldFormID);
                a_intfc->ReadRecordData(traitValue);
                
                NPCTrait trait = static_cast<NPCTrait>(traitValue);

                // Handle FormID translation for different plugin orders (Essential for SKSE)
                RE::FormID newFormID;
                if (!a_intfc->ResolveFormID(oldFormID, newFormID)) {
                    SKSE::log::warn("Failed to resolve FormID: {:X}", oldFormID);
                    continue;
                }

                TraitManager::GetSingleton()->GetTraitMap()[newFormID] = trait;
            }
        }
        
        SKSE::log::info("Loaded NPC trait records.");
    }

    void SerializationManager::Revert(SKSE::SerializationInterface*) {
        TraitManager::GetSingleton()->Clear();
        SKSE::log::info("Serialization data reverted.");
    }
}
