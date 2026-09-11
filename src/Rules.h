#pragma once

#include "RE/Skyrim.h"

#include "Expression.h"

#include <memory>
#include <string>
#include <vector>

namespace GlobalRules
{
    struct Rule
    {
        std::string event;

        // Target. For `menu` events targetName holds the menu name (empty = any);
        // otherwise target holds the resolved form (nullptr = any).
        bool          hasTarget = false;
        RE::TESForm*  target = nullptr;
        RE::FormID    targetFormID = 0;  // full runtime FormID; 0 = none
        std::string   targetName;

        RE::BGSPerk*  perk = nullptr;   // nullptr = always true
        bool          invert = false;

        RE::TESGlobal* global = nullptr;
        std::unique_ptr<Expression> expr;

        std::size_t index = 0;
    };

    // Loads all rule files under Data/SKSE/Plugins/<a_rulesDir>/*.json.
    // Invalid rules are logged and skipped.
    std::vector<Rule> LoadRules(const std::string& a_rulesDir);
}
