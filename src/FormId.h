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

    template <class T>
    T* ResolveFormAs(std::string_view a_id)
    {
        auto form = ResolveForm(a_id);
        return form ? form->As<T>() : nullptr;
    }

    // Human-readable name for logging: editorID, else display name, else FormID hex.
    std::string FormName(RE::TESForm* a_form);
}
