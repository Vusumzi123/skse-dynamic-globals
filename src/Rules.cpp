#include "Rules.h"

#include "Config.h"
#include "Events.h"
#include "FormId.h"

namespace GlobalRules
{
    namespace
    {
        std::string Trim(const std::string& s)
        {
            std::size_t b = 0;
            std::size_t e = s.size();
            while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) {
                ++b;
            }
            while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) {
                --e;
            }
            return s.substr(b, e - b);
        }

        bool IsWildcard(const std::string& s)
        {
            return s.empty() || s == "*";
        }

        std::optional<Rule> ParseRule(const nlohmann::json& j, std::size_t a_index)
        {
            if (!j.is_object()) {
                SKSE::log::warn("rule #{} is not an object; skipping", a_index);
                return std::nullopt;
            }

            Rule rule;
            rule.index = a_index;

            if (!j.contains("event") || !j["event"].is_string()) {
                SKSE::log::warn("rule #{} missing 'event'; skipping", a_index);
                return std::nullopt;
            }
            rule.event = j["event"].get<std::string>();
            if (!IsKnownEvent(rule.event)) {
                SKSE::log::warn("rule #{} unknown event '{}'; skipping", a_index, rule.event);
                return std::nullopt;
            }

            if (!j.contains("global") || !j["global"].is_string()) {
                SKSE::log::warn("rule #{} missing 'global'; skipping", a_index);
                return std::nullopt;
            }
            rule.global = ResolveFormAs<RE::TESGlobal>(j["global"].get<std::string>());
            if (!rule.global) {
                SKSE::log::warn("rule #{} unresolved global '{}'; skipping", a_index, j["global"].get<std::string>());
                return std::nullopt;
            }

            if (!j.contains("value") || !j["value"].is_string()) {
                SKSE::log::warn("rule #{} missing 'value'; skipping", a_index);
                return std::nullopt;
            }
            rule.expr = std::make_unique<Expression>();
            if (!rule.expr->Compile(j["value"].get<std::string>())) {
                SKSE::log::warn("rule #{} bad expression; skipping", a_index);
                return std::nullopt;
            }

            if (j.contains("perk") && j["perk"].is_string()) {
                const auto perkID = j["perk"].get<std::string>();
                rule.perk = ResolveFormAs<RE::BGSPerk>(perkID);
                if (!rule.perk) {
                    SKSE::log::warn("rule #{} unresolved perk '{}'; skipping", a_index, perkID);
                    return std::nullopt;
                }
            }

            if (j.contains("invert") && j["invert"].is_boolean()) {
                rule.invert = j["invert"].get<bool>();
            }

            if (j.contains("target") && j["target"].is_string()) {
                const auto targetID = j["target"].get<std::string>();
                if (rule.event == "menu") {
                    if (!IsWildcard(targetID)) {
                        rule.targetName = Trim(targetID);
                    }
                } else if (!IsWildcard(targetID)) {
                    rule.target = ResolveForm(targetID);
                    if (!rule.target) {
                        SKSE::log::warn("rule #{} unresolved target '{}'; skipping", a_index, targetID);
                        return std::nullopt;
                    }
                    rule.hasTarget = true;
                }
            }

            return rule;
        }
    }

    std::vector<Rule> LoadRules(const std::string& a_rulesDir)
    {
        std::vector<Rule> rules;

        const auto dir = PluginDir() / a_rulesDir;
        if (!std::filesystem::exists(dir)) {
            SKSE::log::info("no rules directory at '{}'", dir.string());
            return rules;
        }

        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext != ".json") {
                continue;
            }
            files.push_back(entry.path());
        }
        std::sort(files.begin(), files.end());

        for (const auto& file : files) {
            try {
                std::ifstream stream(file);
                if (!stream) {
                    SKSE::log::warn("cannot open rule file '{}'", file.string());
                    continue;
                }
                auto root = nlohmann::json::parse(stream, nullptr, false);
                if (root.is_discarded() || !root.is_array()) {
                    SKSE::log::warn("rule file '{}' is not a JSON array; skipping", file.string());
                    continue;
                }
                std::size_t idx = 0;
                for (const auto& entry : root) {
                    auto rule = ParseRule(entry, idx++);
                    if (rule) {
                        rules.push_back(std::move(*rule));
                    }
                }
            } catch (const std::exception& e) {
                SKSE::log::warn("failed to read rule file '{}': {}", file.string(), e.what());
            }
        }

        SKSE::log::info("loaded {} rule(s) from {} file(s)", rules.size(), files.size());
        return rules;
    }
}
