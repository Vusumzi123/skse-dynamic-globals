#include "Engine.h"
#include "FormId.h"
#include "Persistence.h"
#include "version.h"

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

    SKSE::log::info("GlobalRules plugin v{} loaded", GLOBALRULES_VERSION_STRING);

    if (GlobalRules::IsPo3TweaksLoaded()) {
        SKSE::log::info("po3 Tweaks detected: editorID references resolve for all form types");
    } else {
        SKSE::log::warn("po3 Tweaks not detected: editorID references only resolve for natively-cached types (Global, Keyword, Quest, Race, Cell, ...); use FormID references for perks");
    }

    return true;
}
