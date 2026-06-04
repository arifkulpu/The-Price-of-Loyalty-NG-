#include "InteractionHandler.hpp"
#include "TraitManager.hpp"
#include "BehaviorManager.hpp"
#include "Settings.hpp"
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

namespace RE {
    MessageBoxData::~MessageBoxData() {}
}

namespace Loyalty {
    RE::BSEventNotifyControl InteractionHandler::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_eventSource) {
        if (!a_event || !*a_event) return RE::BSEventNotifyControl::kContinue;

        auto hotkey = (std::uint32_t)Settings::GetSingleton()->bribeHotkey;

        for (auto event = *a_event; event; event = event->next) {
            if (event->GetEventType() != RE::INPUT_EVENT_TYPE::kButton) continue;

            auto button = event->AsButtonEvent();
            if (!button || !button->HasIDCode() || !button->IsDown()) continue;

            if (button->GetIDCode() == hotkey) {
                SKSE::GetTaskInterface()->AddTask([this]() {
                    HandleInteraction();
                });
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }

    void InteractionHandler::HandleInteraction() {
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;

        RE::Actor* targetActor = nullptr;
        auto crosshair = RE::CrosshairPickData::GetSingleton();
        
        if (crosshair) {
            auto getActorFromHandle = [](RE::ObjectRefHandle& a_handle) -> RE::Actor* {
                if (a_handle) {
                    auto ref = a_handle.get();
                    if (ref) return ref->As<RE::Actor>();
                }
                return nullptr;
            };

            for (int i = 0; i < 2; ++i) {
                targetActor = getActorFromHandle(crosshair->targetActor[i]);
                if (targetActor) break;
                targetActor = getActorFromHandle(crosshair->target[i]);
                if (targetActor) break;
            }
        }

        if (targetActor && targetActor != player) {
            try {
                if (targetActor->IsDead()) return;
                
                if (targetActor->IsPlayerTeammate()) {
                    ShowDismissMenu(targetActor);
                    return;
                }

                auto baseNPC = targetActor->GetActorBase();
                bool isUnique = baseNPC && baseNPC->IsUnique();
                bool isMerchant = BehaviorManager::IsMerchantOrServiceNPC(targetActor);

                // Exclude unique NPCs from bribery unless they are merchants/service NPCs
                if (isUnique && !isMerchant) {
                    RE::DebugNotification("This unique individual is too honorable to accept bribes.");
                    return;
                }

                if (targetActor->IsEssential()) {
                    RE::DebugNotification("This individual cannot be bribed.");
                    return;
                }

                // HEALTH CHECK FOR HOSTILES via ActorValueOwner
                if (targetActor->IsHostileToActor(player)) {
                    auto avOwner = targetActor->AsActorValueOwner();
                    if (avOwner) {
                        float currentHP = avOwner->GetActorValue(RE::ActorValue::kHealth);
                        float maxHP = avOwner->GetPermanentActorValue(RE::ActorValue::kHealth);
                        float healthPct = (maxHP > 0) ? (currentHP / maxHP) : 1.0f;

                        if (healthPct > 0.30f) {
                            RE::DebugNotification("Target is too strong to be bribed. Beat them more!");
                            return;
                        }
                    }
                }

                ShowBribeMenu(targetActor);
            } catch (...) {}
        } else {
            BehaviorManager::GetSingleton()->CallAllies();
        }
    }

    class DismissMenuCallback : public RE::IMessageBoxCallback {
    public:
        RE::ActorHandle targetHandle;

        DismissMenuCallback(RE::Actor* a_target) {
            if (a_target) targetHandle = a_target->GetHandle();
        }

        void Run(Message a_msg) override {
            auto target = targetHandle.get().get();
            if (!target) return;

            SKSE::log::info("[DISMISS_MENU_CALLBACK] Run triggered. Msg={}, Target='{}'", static_cast<int>(a_msg), target->GetName());

            if (a_msg == Message::kUnk0) {
                SKSE::log::info("[DISMISS_MENU_CALLBACK] Player chose to Dismiss '{}'.", target->GetName());
                BehaviorManager::GetSingleton()->DismissAlly(target);
            } else {
                SKSE::log::info("[DISMISS_MENU_CALLBACK] Player cancelled or chose to keep '{}'.", target->GetName());
            }
        }
    };

    void InteractionHandler::ShowDismissMenu(RE::Actor* a_target) {
        if (!a_target) return;

        std::string bodyText = "THE PRICE OF LOYALTY\n\n";
        bodyText += a_target->GetName();
        bodyText += " is currently following you.\nWhat do you want to do?";

        auto cb = RE::BSTSmartPointer<RE::IMessageBoxCallback>(new DismissMenuCallback(a_target));
        
        auto msgBox = new RE::MessageBoxData();
        if (msgBox) {
            msgBox->unk08 = 0;
            msgBox->pad0A = 0;
            msgBox->pad0C = 0;
            msgBox->menuDepth = 3;
            msgBox->optionIndexOffset = 0;
            msgBox->useHtml = false;
            msgBox->verticalButtons = false;
            msgBox->cancelOptionIndex = -1;
            msgBox->isCancellable = false;

            msgBox->callback = cb;
            msgBox->bodyText = bodyText.c_str();
            msgBox->type = 1;

            msgBox->buttonText.push_back("Part Ways (Dismiss)");
            msgBox->buttonText.push_back("Stay with me");

            msgBox->QueueMessage();
        }
    }

    class BribeMenuCallback : public RE::IMessageBoxCallback {
    public:
        RE::ActorHandle targetHandle;
        float amountLow;
        float amountHigh;

        BribeMenuCallback(RE::Actor* a_target, float a_low, float a_high) 
            : amountLow(a_low), amountHigh(a_high) {
            if (a_target) targetHandle = a_target->GetHandle();
        }

        void Run(Message a_msg) override {
            auto player = RE::PlayerCharacter::GetSingleton();
            auto target = targetHandle.get().get();
            if (!player || !target) return;

            SKSE::log::info("[BRIBE_MENU_CALLBACK] Run triggered. Msg={}, Target='{}'", static_cast<int>(a_msg), target->GetName());

            float choiceAmount = 0;
            if (a_msg == Message::kUnk0) choiceAmount = amountLow;
            else if (a_msg == Message::kUnk1) choiceAmount = amountHigh;
            else {
                SKSE::log::info("[BRIBE_MENU_CALLBACK] Player cancelled or chose invalid option. Exiting callback.");
                return;
            }

            auto goldForm = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F);
            int32_t currentGold = 0;
            if (goldForm) {
                currentGold = player->GetItemCount(goldForm);
            }

            if (currentGold < static_cast<std::int32_t>(choiceAmount)) {
                RE::DebugNotification("Not enough gold!");
                SKSE::log::info("[BRIBE_MENU_CALLBACK] Not enough gold (Required={}, Current={}).", choiceAmount, currentGold);
                return;
            }

            bool success = TraitManager::GetSingleton()->RollForSuccess(target, choiceAmount);

            if (success) {
                if (goldForm) {
                    player->RemoveItem(goldForm, static_cast<int32_t>(choiceAmount), RE::ITEM_REMOVE_REASON::kRemove, nullptr, target);
                    RE::DebugNotification("Gold paid.");
                }
            }

            bool isLow = (a_msg == Message::kUnk0);
            BehaviorManager::GetSingleton()->ProcessBribeResult(target, success, isLow);
        }
    };

    void InteractionHandler::ShowBribeMenu(RE::Actor* a_target) {
        if (!a_target) return;

        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;

        float cost = TraitManager::GetSingleton()->CalculateBribeCost(a_target);
        float low = std::round(cost * 0.5f);
        float high = std::round(cost * 1.2f);

        int32_t goldCount = 0;
        auto goldForm = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F);
        if (goldForm) {
            goldCount = player->GetItemCount(goldForm);
        }

        std::string bodyText = "THE PRICE OF LOYALTY\n\nTarget: ";
        bodyText += a_target->GetName();
        bodyText += "\nLevel: ";
        bodyText += std::to_string(a_target->GetLevel());
        bodyText += "\nYour Gold: ";
        bodyText += std::to_string(goldCount);
        bodyText += "\n\nChoose your bribe offer:";

        auto cb = RE::BSTSmartPointer<RE::IMessageBoxCallback>(new BribeMenuCallback(a_target, low, high));
        
        auto msgBox = new RE::MessageBoxData();
        if (msgBox) {
            msgBox->unk08 = 0;
            msgBox->pad0A = 0;
            msgBox->pad0C = 0;
            msgBox->menuDepth = 3;
            msgBox->optionIndexOffset = 0;
            msgBox->useHtml = false;
            msgBox->verticalButtons = false;

            msgBox->callback = cb;
            msgBox->bodyText = bodyText.c_str();
            msgBox->type = 1;
            msgBox->cancelOptionIndex = 2;
            msgBox->isCancellable = true;

            char btn1[64], btn2[64];
            sprintf_s(btn1, "Bribe %.0f G", low);
            sprintf_s(btn2, "Bribe %.0f G", high);

            msgBox->buttonText.push_back(btn1);
            msgBox->buttonText.push_back(btn2);
            msgBox->buttonText.push_back("Cancel");

            msgBox->QueueMessage();
        }
    }
}
