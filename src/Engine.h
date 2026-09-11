#pragma once

#include "RE/Skyrim.h"

#include "Config.h"
#include "Events.h"
#include "Rules.h"

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace GlobalRules
{
    struct StringHash
    {
        using is_transparent = void;
        std::size_t operator()(std::string_view a_s) const noexcept
        {
            return std::hash<std::string_view>{}(a_s);
        }
    };

    class Engine
    {
    public:
        static Engine& Get();

        void OnDataLoaded();
        void Reload();

        [[nodiscard]] bool DebugEnabled() const;

    private:
        Engine() = default;

        void Load();
        void ApplyLogLevel();
        void OnEvent(std::string_view a_name, const EventContext& a_ctx);
        [[nodiscard]] bool TargetMatches(const Rule& a_rule, std::string_view a_event, const EventContext& a_ctx) const;

        Config config_;
        std::vector<Rule> rules_;
        std::unordered_map<std::string, std::vector<std::size_t>, StringHash, std::equal_to<>> byEvent_;
        RE::TESGlobal* debugGlobalForm_ = nullptr;
    };
}
