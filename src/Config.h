#pragma once

#include <filesystem>
#include <string>

namespace GlobalRules
{
    struct Config
    {
        bool        enabled = true;
        bool        debug = false;
        bool        logChanges = true;
        std::string logLevel = "info";
        bool        dryRun = false;
        std::string rulesDirectory = "GlobalRules";
        std::string debugGlobal;  // raw form identifier; resolved by the engine

        static Config Load();
    };

    // Returns the plugin's directory (Data/SKSE/Plugins).
    std::filesystem::path PluginDir();

    // Returns the config path (Data/SKSE/Plugins/GlobalRules.json).
    std::filesystem::path ConfigPath();
}
