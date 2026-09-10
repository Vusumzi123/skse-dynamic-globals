#include "Persistence.h"

namespace GlobalRules::Persistence
{
    std::unordered_map<RE::FormID, float>& AppliedValues()
    {
        static std::unordered_map<RE::FormID, float> values;
        return values;
    }

    void Save(SKSE::SerializationInterface* a_intfc)
    {
        if (!a_intfc) {
            return;
        }
        auto& values = AppliedValues();
        if (values.empty()) {
            return;
        }

        if (!a_intfc->OpenRecord(kRecordType, kVersion)) {
            SKSE::log::error("failed to open co-save record");
            return;
        }

        const std::uint32_t count = static_cast<std::uint32_t>(values.size());
        a_intfc->WriteRecordData(count);
        for (const auto& [id, value] : values) {
            a_intfc->WriteRecordData(id);
            a_intfc->WriteRecordData(value);
        }

        SKSE::log::info("co-save: wrote {} global value(s)", count);
    }

    void Load(SKSE::SerializationInterface* a_intfc)
    {
        if (!a_intfc) {
            return;
        }

        std::uint32_t type = 0;
        std::uint32_t version = 0;
        std::uint32_t length = 0;
        while (a_intfc->GetNextRecordInfo(type, version, length)) {
            if (type != kRecordType) {
                continue;
            }
            if (version != kVersion) {
                SKSE::log::warn("co-save: unsupported version {}", version);
                continue;
            }

            std::uint32_t count = 0;
            if (a_intfc->ReadRecordData(&count, sizeof(count)) != sizeof(count)) {
                continue;
            }

            for (std::uint32_t i = 0; i < count; ++i) {
                RE::FormID id = 0;
                float      value = 0.0f;
                if (a_intfc->ReadRecordData(&id, sizeof(id)) != sizeof(id)) {
                    break;
                }
                if (a_intfc->ReadRecordData(&value, sizeof(value)) != sizeof(value)) {
                    break;
                }

                auto* global = RE::TESForm::LookupByID<RE::TESGlobal>(id);
                if (global) {
                    global->value = value;
                    AppliedValues()[id] = value;
                }
            }

            SKSE::log::info("co-save: restored {} global value(s)", count);
        }
    }

    void Revert(SKSE::SerializationInterface*)
    {
        AppliedValues().clear();
    }

    void Register(const SKSE::SerializationInterface* a_intfc)
    {
        if (!a_intfc) {
            SKSE::log::error("serialization interface unavailable");
            return;
        }
        a_intfc->SetUniqueID(kUniqueID);
        a_intfc->SetSaveCallback(Save);
        a_intfc->SetLoadCallback(Load);
        a_intfc->SetRevertCallback(Revert);
    }
}
