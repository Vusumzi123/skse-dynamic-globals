#pragma once

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <unordered_map>

namespace GlobalRules::Persistence
{
    constexpr std::uint32_t kUniqueID = 'GLBL';
    constexpr std::uint32_t kRecordType = 'GLBL';
    constexpr std::uint32_t kVersion = 1;

    // Framework-touched globals: FormID -> last applied value.
    std::unordered_map<RE::FormID, float>& AppliedValues();

    void Save(SKSE::SerializationInterface* a_intfc);
    void Load(SKSE::SerializationInterface* a_intfc);
    void Revert(SKSE::SerializationInterface* a_intfc);

    void Register(const SKSE::SerializationInterface* a_intfc);
}
