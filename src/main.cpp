#include <SKSE/SKSE.h>
#include "InteractionHandler.hpp"
#include "Settings.hpp"
#include "TraitManager.hpp"
#include "BehaviorManager.hpp"
#include "SerializationManager.hpp"

namespace Loyalty {
    void Initialize() {
        Settings::GetSingleton()->Load();
        
        auto input = RE::BSInputDeviceManager::GetSingleton();
        if (input) {
            input->AddEventSink(InteractionHandler::GetSingleton());
        }

        auto eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
        if (eventSourceHolder) {
            eventSourceHolder->AddEventSink<RE::TESCellFullyLoadedEvent>(BehaviorManager::GetSingleton());
            SKSE::log::info("Cell fully loaded event sink registered.");
        }

        SKSE::log::info("The Price of Loyalty Initialized.");
    }
}

void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
	if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
		Loyalty::Initialize();
	}
}

extern "C" [[maybe_unused]] __declspec(dllexport) const ::SKSE::PluginDeclaration SKSEPlugin_Version({
    .Version = REL::Version{ 1, 0, 0, 0 },
    .Name = "ThePriceOfLoyalty",
    .Author = "Antigravity",
    .StructCompatibility = SKSE::StructCompatibility::Independent,
    .RuntimeCompatibility = SKSE::PluginDeclaration::RuntimeCompatibility(SKSE::VersionIndependence::AddressLibrary)
});

extern "C" [[maybe_unused]] __declspec(dllexport) bool SKSEPlugin_Query(SKSE::QueryInterface*, SKSE::PluginInfo* pluginInfo)
{
    pluginInfo->infoVersion = SKSE::PluginInfo::kVersion;
    pluginInfo->name = SKSEPlugin_Version.GetName().data();
    pluginInfo->version = static_cast<std::uint32_t>(SKSEPlugin_Version.GetVersion().pack());
    return true;
}

extern "C" [[maybe_unused]] __declspec(dllexport) bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);

    // Serialization: NPC trait verilerini kayıt dosyasına yaz/oku
    Loyalty::SerializationManager::Register();
    
    auto messaging = SKSE::GetMessagingInterface();
	messaging->RegisterListener(MessageHandler);

	return true;
}
