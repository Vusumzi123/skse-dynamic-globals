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
        switch (a_msg->type) {
        case SKSE::MessagingInterface::kPostLoad:
            // Defer this check to kPostLoad: SKSE loads plugins sequentially and
            // "GlobalRules" sorts before "po3_Tweaks", so po3_Tweaks.dll is not
            // yet in the process during SKSEPluginLoad.
            if (GlobalRules::IsPo3TweaksLoaded()) {
                SKSE::log::info("po3 Tweaks detected: editorID references resolve for all form types");
            } else {
                SKSE::log::warn("po3 Tweaks not detected: editorID references only resolve for natively-cached types (Global, Keyword, Quest, Race, Cell, ...); use FormID references for perks");
            }
            break;
        case SKSE::MessagingInterface::kDataLoaded:
            GlobalRules::Engine::Get().OnDataLoaded();
            break;
        default:
            break;
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

    return true;
}
