#include "Engine.h"
#include "Persistence.h"

namespace
{
    void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
    {
        if (!a_msg) {
            return;
        }
        if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
            GlobalRules::Engine::Get().OnDataLoaded();
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);
    SKSE::log::init();

    SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);

    GlobalRules::Persistence::Register(SKSE::GetSerializationInterface());

    SKSE::log::info("GlobalRules plugin loaded");
    return true;
}
