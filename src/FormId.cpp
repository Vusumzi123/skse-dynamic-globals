#include "FormId.h"

#include <fmt/format.h>

#include <charconv>

#include "REX/W32.h"

namespace GlobalRules
{
    namespace
    {
        std::string_view Trim(std::string_view s)
        {
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
                s.remove_prefix(1);
            }
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
                s.remove_suffix(1);
            }
            return s;
        }

        bool IEquals(std::string_view a, std::string_view b)
        {
            if (a.size() != b.size()) {
                return false;
            }
            for (std::size_t i = 0; i < a.size(); ++i) {
                if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
                    return false;
                }
            }
            return true;
        }

        // Parses "0x000ABC" into a local FormID; returns false if not hex-shaped.
        bool ParseHexID(std::string_view a_idText, std::uint32_t& a_out)
        {
            if (a_idText.size() > 2 && (a_idText[0] == '0' && (a_idText[1] == 'x' || a_idText[1] == 'X'))) {
                const auto* begin = a_idText.data() + 2;
                const auto* end = a_idText.data() + a_idText.size();
                const auto  res = std::from_chars(begin, end, a_out, 16);
                return res.ec == std::errc{} && res.ptr == end;
            }
            return false;
        }

        RE::TESForm* ResolveLocalFormID(std::string_view a_plugin, std::string_view a_idText)
        {
            std::uint32_t localID = 0;
            if (ParseHexID(a_idText, localID)) {
                return RE::TESDataHandler::GetSingleton()->LookupForm(localID, a_plugin);
            }
            if (a_idText.size() > 2 && (a_idText[0] == '0' && (a_idText[1] == 'x' || a_idText[1] == 'X'))) {
                SKSE::log::warn("invalid local FormID '{}'", a_idText);
                return nullptr;
            }

            // EditorID within a plugin.
            auto form = RE::TESForm::LookupByEditorID(a_idText);
            if (!form) {
                SKSE::log::warn("unresolved editorID '{}' in '{}'", a_idText, a_plugin);
                return nullptr;
            }
            const auto file = form->GetFile(0);
            if (file && !IEquals(file->GetFilename(), a_plugin)) {
                SKSE::log::warn("editorID '{}' resolves to a form in '{}', not '{}'", a_idText, file->GetFilename(), a_plugin);
            }
            return form;
        }

        using GetFormEditorIDFn = const char* (*)(std::uint32_t);

        GetFormEditorIDFn GetPo3EditorIDFn()
        {
            static GetFormEditorIDFn fn = [] {
                const auto mod = REX::W32::GetModuleHandleW(L"po3_Tweaks.dll");
                return mod ? reinterpret_cast<GetFormEditorIDFn>(REX::W32::GetProcAddress(mod, "GetFormEditorID")) : nullptr;
            }();
            return fn;
        }
    }

    RE::TESForm* ResolveForm(std::string_view a_id)
    {
        a_id = Trim(a_id);
        if (a_id.empty()) {
            return nullptr;
        }

        const auto bar = a_id.find('|');
        if (bar == std::string_view::npos) {
            auto form = RE::TESForm::LookupByEditorID(a_id);
            if (!form) {
                SKSE::log::warn("unresolved editorID '{}'", a_id);
            }
            return form;
        }

        const auto plugin = Trim(a_id.substr(0, bar));
        const auto rest = Trim(a_id.substr(bar + 1));
        if (plugin.empty() || rest.empty()) {
            SKSE::log::warn("malformed form identifier '{}'", a_id);
            return nullptr;
        }
        return ResolveLocalFormID(plugin, rest);
    }

    RE::FormID ResolveFormID(std::string_view a_id)
    {
        a_id = Trim(a_id);
        if (a_id.empty()) {
            return 0;
        }

        const auto bar = a_id.find('|');
        if (bar == std::string_view::npos) {
            const auto form = RE::TESForm::LookupByEditorID(a_id);
            return form ? form->GetFormID() : 0;
        }

        const auto plugin = Trim(a_id.substr(0, bar));
        const auto rest = Trim(a_id.substr(bar + 1));
        if (plugin.empty() || rest.empty()) {
            SKSE::log::warn("malformed form identifier '{}'", a_id);
            return 0;
        }

        std::uint32_t localID = 0;
        if (ParseHexID(rest, localID)) {
            return RE::TESDataHandler::GetSingleton()->LookupFormID(localID, plugin);
        }

        const auto form = RE::TESForm::LookupByEditorID(rest);
        return form ? form->GetFormID() : 0;
    }

    std::string FormName(RE::TESForm* a_form)
    {
        if (!a_form) {
            return "<none>";
        }
        const auto editorID = a_form->GetFormEditorID();
        if (editorID && *editorID) {
            return editorID;
        }
        if (const auto fn = GetPo3EditorIDFn()) {
            const auto po3ID = fn(a_form->GetFormID());
            if (po3ID && *po3ID) {
                return po3ID;
            }
        }
        const auto name = a_form->GetName();
        if (name && *name) {
            return name;
        }
        return fmt::format("0x{:08X}", a_form->GetFormID());
    }

    bool IsPo3TweaksLoaded()
    {
        return REX::W32::GetModuleHandleW(L"po3_Tweaks.dll") != nullptr;
    }
}
