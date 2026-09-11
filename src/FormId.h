#pragma once

#include "RE/Skyrim.h"

#include <string_view>

namespace GlobalRules
{
    // Resolves a form identifier:
    //   "Plugin.esp|0xLOCALID"   -> TESDataHandler::LookupForm
    //   "Plugin.esp|EditorID"    -> editorID scoped to the plugin (best-effort)
    //   "EditorID"               -> TESForm::LookupByEditorID
    // Returns nullptr if unresolved.
    RE::TESForm* ResolveForm(std::string_view a_id);

    // Resolves a form identifier to its full runtime FormID without requiring the
    // form object to be loaded yet. For "Plugin.esp|0xLOCALID" this uses
    // LookupFormID, which succeeds for lazily-created forms (e.g. exterior cells).
    // Returns 0 if unresolved.
    RE::FormID ResolveFormID(std::string_view a_id);

    template <class T>
    T* ResolveFormAs(std::string_view a_id)
    {
        auto form = ResolveForm(a_id);
        return form ? form->As<T>() : nullptr;
    }

    // Human-readable name for logging: editorID, else display name, else FormID hex.
    std::string FormName(RE::TESForm* a_form);

    // True if powerofthree's Tweaks (po3_Tweaks.dll) is loaded. Its
    // SetFormEditorID vfunc hook makes TESForm::LookupByEditorID resolve form
    // types the engine does not natively cache (e.g. Perk).
    bool IsPo3TweaksLoaded();
}
