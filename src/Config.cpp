#include "Config.h"

#include <REX/W32.h>

#include <fstream>
#include <vector>

namespace GlobalRules
{
    std::filesystem::path PluginDir()
    {
        constexpr std::uint32_t kBufSize = 32768;
        std::vector<wchar_t> buf(kBufSize);

        const auto result = REX::W32::GetModuleFileNameW(
            REX::W32::GetCurrentModule(),
            buf.data(),
            kBufSize);

        if (result && result < kBufSize) {
            std::filesystem::path p(buf.begin(), buf.begin() + result);
            return p.parent_path();
        }
        return {};
    }

    std::filesystem::path ConfigPath()
    {
        return PluginDir() / "GlobalRules.json";
    }

    namespace
    {
        nlohmann::json DefaultConfig()
        {
            return nlohmann::json{
                { "enabled", true },
                { "debug", false },
                { "logChanges", true },
                { "logLevel", "info" },
                { "dryRun", false },
                { "rulesDirectory", "GlobalRules" },
                { "debugGlobal", "" }
            };
        }

        void WriteDefaultConfig(const std::filesystem::path& a_path)
        {
            try {
                std::ofstream file(a_path);
                if (!file) {
                    SKSE::log::warn("cannot create default config at '{}'", a_path.string());
                    return;
                }
                file << DefaultConfig().dump(2) << "\n";
                SKSE::log::info("wrote default config to '{}'", a_path.string());
            } catch (const std::exception& e) {
                SKSE::log::warn("failed to write default config: {}", e.what());
            }
        }
    }

    Config Config::Load()
    {
        Config config;

        const auto path = ConfigPath();
        if (!std::filesystem::exists(path)) {
            WriteDefaultConfig(path);
            return config;
        }

        try {
            std::ifstream file(path);
            if (!file) {
                SKSE::log::warn("cannot open config '{}'; using defaults", path.string());
                return config;
            }
            auto root = nlohmann::json::parse(file, nullptr, false);
            if (root.is_discarded() || !root.is_object()) {
                SKSE::log::warn("config '{}' is not a valid JSON object; using defaults", path.string());
                return config;
            }

            const auto readBool = [&root](const char* a_key, bool& a_out) {
                if (root.contains(a_key) && root[a_key].is_boolean()) {
                    a_out = root[a_key].get<bool>();
                }
            };
            const auto readStr = [&root](const char* a_key, std::string& a_out) {
                if (root.contains(a_key) && root[a_key].is_string()) {
                    a_out = root[a_key].get<std::string>();
                }
            };

            readBool("enabled",        config.enabled);
            readBool("debug",          config.debug);
            readBool("logChanges",     config.logChanges);
            readStr("logLevel",        config.logLevel);
            readBool("dryRun",         config.dryRun);
            readStr("rulesDirectory",  config.rulesDirectory);
            readStr("debugGlobal",     config.debugGlobal);
        } catch (const std::exception& e) {
            SKSE::log::warn("failed to parse config '{}': {}", path.string(), e.what());
        }

        return config;
    }
}
