#pragma once

#include "RE/Skyrim.h"

namespace GlobalRules
{
    // Evaluates the perk's top-level TESCondition (perkConditions) with the player as
    // actionRef and a_targetRef as targetRef. A null perk is treated as "always true".
    // a_invert flips the result.
    [[nodiscard]] bool CheckPerk(RE::BGSPerk* a_perk, RE::TESObjectREFR* a_targetRef, bool a_invert);
}
