#pragma once

#include "slsf/core/Types.h"

namespace slsf::engine {

// The engine owns load-index resolution and record type checking. Call only
// after DataLoaded on the game thread; returned records belong to the engine.
template <class Record, class DataHandler>
Record* LookupLoadedRecord(DataHandler* data, const StableFormKey& key) {
    if (!data || key.plugin.empty() || key.localFormId == 0 ||
        key.localFormId > 0x00FFFFFFu) {
        return nullptr;
    }
    const auto* file = data->LookupModByName(key.plugin);
    if (!file || (file->IsLight() && key.localFormId > 0xFFFu)) {
        return nullptr;
    }
    return data->template LookupForm<Record>(key.localFormId, key.plugin);
}

template <class Record>
StableFormKey LoadedRecordKey(const Record* record) {
    if (const auto* file = record ? record->GetFile(0) : nullptr) {
        return {std::string(file->GetFilename()), record->GetLocalFormID()};
    }
    return {};
}

}
