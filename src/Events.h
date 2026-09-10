#pragma once

#include "RE/Skyrim.h"

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace GlobalRules
{
    struct EventContext
    {
        RE::Actor*        subject = nullptr;    // always the player
        RE::TESObjectREFR* targetRef = nullptr;  // condition target; falls back to player
        RE::TESForm*      targetForm = nullptr;  // for exact/wildcard matching
        std::string       targetName;            // menu name for `menu`
        std::unordered_map<std::string, double> params;
    };

    // Returns true if a_name is one of the events the engine can dispatch.
    [[nodiscard]] bool IsKnownEvent(std::string_view a_name);

    class EventManager
    {
    public:
        using Handler = std::function<void(std::string_view, const EventContext&)>;

        static EventManager& Get();

        void SetHandler(Handler a_handler);
        void RegisterAll();
        void UnregisterAll();

        void Dispatch(std::string_view a_name, const EventContext& a_ctx);

    private:
        EventManager() = default;

        Handler handler_;
        bool    registered_ = false;
    };
}
