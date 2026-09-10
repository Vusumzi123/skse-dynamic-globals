#include "Condition.h"

namespace GlobalRules
{
    bool CheckPerk(RE::BGSPerk* a_perk, RE::TESObjectREFR* a_targetRef, bool a_invert)
    {
        bool passed = true;
        if (a_perk) {
            passed = a_perk->perkConditions.IsTrue(RE::PlayerCharacter::GetSingleton(), a_targetRef);
        }
        if (a_invert) {
            passed = !passed;
        }
        return passed;
    }
}
