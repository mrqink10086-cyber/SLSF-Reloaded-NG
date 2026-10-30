#include "pch.h"

#include "slsf/boundary/LegacyRecords.h"
#include "slsf/engine/RecordLookup.h"

#include <utility>

namespace slsf::boundary {
namespace {

void SetGlobal(const StableFormKey& key, int value) {
    if (auto* global = engine::LookupLoadedRecord<RE::TESGlobal>(
            RE::TESDataHandler::GetSingleton(), key)) {
        global->value = static_cast<float>(value);
    }
}

}

EngineLegacyOutputSink::EngineLegacyOutputSink(StorageWriter storageWriter)
    : storageWriter_(std::move(storageWriter)) {}

void EngineLegacyOutputSink::SetCurrentFameGlobal(const StableFormKey& key, int value) {
    SetGlobal(key, value);
}

void EngineLegacyOutputSink::SetStorageIntList(const std::string& key,
                                              std::size_t locationIndex, int value) {
    if (storageWriter_) {
        storageWriter_(key, locationIndex, value);
    }
}

void EngineLegacyOutputSink::SetLegacyBridgeGlobal(const StableFormKey& key, int value) {
    SetGlobal(key, value);
}

RE::TESQuest* LookupLegacyQuest(const StableFormKey& key) {
    return engine::LookupLoadedRecord<RE::TESQuest>(RE::TESDataHandler::GetSingleton(), key);
}

compat::PapyrusInstance PapyrusInstanceForQuest(const RE::TESQuest* quest,
                                              std::int32_t aliasIndex) {
    return {engine::LoadedRecordKey(quest), aliasIndex};
}

}
