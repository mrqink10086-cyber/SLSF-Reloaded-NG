#include "slsf/compat/LegacyOutputs.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace slsf::compat {

namespace {

constexpr std::array<std::uint32_t, kFameCategoryCount> kCurrentFameGlobalLocalIds = {
    0x0000080Du,
    0x0000080Cu,
    0x0000080Eu,
    0x0000080Fu,
    0x00000810u,
    0x00000811u,
    0x00000812u,
    0x00000813u,
    0x00000814u,
    0x00000815u,
    0x00000816u,
    0x00000817u,
    0x00000818u,
    0x00000819u,
    0x0000081Au,
    0x0000081Bu,
    0x0000081Cu,
    0x0000081Du,
    0x0000081Eu,
    0x0000081Fu,
    0x00000820u,
    0x00000821u,
    0x00000822u,
    0x00000823u,
    0x0000082Fu,
};

constexpr std::array<const char*, kFameCategoryCount> kCurrentFameGlobalEditorIds = {
    "CurrentWhoreFame",     "CurrentSlutFame",       "CurrentExhibitionistFame",
    "CurrentOralFame",      "CurrentAnalFame",       "CurrentNastyFame",
    "CurrentPregnantFame",  "CurrentDominantFame",   "CurrentSubmissiveFame",
    "CurrentSadistFame",    "CurrentMasochistFame",  "CurrentGentleFame",
    "CurrentMenFame",       "CurrentWomenFame",      "CurrentOrcFame",
    "CurrentKhajiitFame",   "CurrentArgonianFame",   "CurrentBestialityFame",
    "CurrentGroupFame",     "CurrentBoundFame",      "CurrentTattooFame",
    "CurrentCumDumpFame",   "CurrentCheatFame",      "CurrentCuckFame",
    "CurrentAirheadFame",
};

struct LegacyFameSlot {
    const char* keySuffix;
    std::optional<FameCategory> category;
};

// NOTE: elements use explicit `LegacyFameSlot{...}` initializers.  MSVC 14.44
// fails constant evaluation of a constexpr std::array whose aggregate
// elements are brace-initialized implicitly (C2131), so the explicit type is
constexpr std::array<LegacyFameSlot, LegacyOutputs::kLegacyFameSlotCount> kLegacyFameSlots = {
    LegacyFameSlot{"Anal", FameCategory::Anal},
    LegacyFameSlot{"Argonian", FameCategory::LikesArgonian},
    LegacyFameSlot{"Beastiality", FameCategory::Bestiality},
    LegacyFameSlot{"Dominant/Master", FameCategory::Dominant},
    LegacyFameSlot{"Exhibitionist/Exposed", FameCategory::Exhibitionist},
    LegacyFameSlot{"GentleLover", FameCategory::Gentle},
    LegacyFameSlot{"Group", FameCategory::Group},
    LegacyFameSlot{"Khajiit", FameCategory::LikesKhajiit},
    LegacyFameSlot{"LikeMan", FameCategory::LikesMen},
    LegacyFameSlot{"LikeWoman", FameCategory::LikesWomen},
    LegacyFameSlot{"Masochist", FameCategory::Masochist},
    LegacyFameSlot{"Nasty", FameCategory::Nasty},
    LegacyFameSlot{"Oral", FameCategory::Oral},
    LegacyFameSlot{"Orc", FameCategory::LikesOrc},
    LegacyFameSlot{"Pregnant", FameCategory::Pregnant},
    LegacyFameSlot{"Sadic", FameCategory::Sadist},
    LegacyFameSlot{"SkoomaUser", std::nullopt},
    LegacyFameSlot{"Slut", FameCategory::Slut},
    LegacyFameSlot{"Submissive/Slave", FameCategory::Submissive},
    LegacyFameSlot{"Whore", FameCategory::Whore},
};

constexpr std::array<const char*, LegacyOutputs::kLegacyLocationIndexCount> kLegacyLocationNames = {
    "Dawnstar",     "Falkreath",   "Markarth",    "Morthal",
    "Riften",       "Solitude",    "Whiterun",    "Windhelm",
    "Winterhold",   "Dragon Bridge", "Ivarstead", "Karthwasten",
    "Riverwood",    "Rorikstead",  "Shor's Stone",
    "",
    "Dushnikh Yal", "Largashbur",  "Mor Khazgur", "Narzulbur",
    "",
    "",
    "Skaal Village", "Raven Rock",
};

constexpr std::uint32_t kLegacyBridgeFirstLocalId = 0x00024255u;

}
StableFormKey LegacyOutputs::CurrentFameGlobalKey(FameCategory category) {
    const auto index = FameCategoryIndex(category);
    return index < kCurrentFameGlobalLocalIds.size()
               ? StableFormKey{"SLSF Reloaded.esp", kCurrentFameGlobalLocalIds[index]}
               : StableFormKey{};
}
const char* LegacyOutputs::CurrentFameGlobalEditorId(FameCategory category) noexcept {
    const auto index = FameCategoryIndex(category);
    return index < kCurrentFameGlobalEditorIds.size() ? kCurrentFameGlobalEditorIds[index]
                                                      : "";
}
std::string LegacyOutputs::StorageFameKey(std::size_t fameIndex) noexcept {
    if (fameIndex >= kLegacyFameSlots.size()) {
        return {};
    }
    return "SLSF.LocationsFame.PC." + std::string(kLegacyFameSlots[fameIndex].keySuffix);
}
std::optional<FameCategory> LegacyOutputs::StorageFameCategory(std::size_t fameIndex) noexcept {
    return fameIndex < kLegacyFameSlots.size() ? kLegacyFameSlots[fameIndex].category
                                               : std::nullopt;
}
std::optional<std::string> LegacyOutputs::LegacyLocationName(
    std::size_t locationIndex) noexcept {
    if (locationIndex >= kLegacyLocationNames.size()) {
        return std::nullopt;
    }
    const auto* name = kLegacyLocationNames[locationIndex];
    return name[0] != '\0' ? std::optional<std::string>(name) : std::nullopt;
}
StableFormKey LegacyOutputs::LegacyBridgeKey(std::size_t fameIndex) {
    return fameIndex < kLegacyFameSlots.size()
               ? StableFormKey{"SexLab - Sexual Fame [SLSF].esm",
                               kLegacyBridgeFirstLocalId + static_cast<std::uint32_t>(fameIndex)}
               : StableFormKey{};
}
std::optional<FameCategory> LegacyOutputs::LegacyBridgeCategory(std::size_t fameIndex) noexcept {
    return StorageFameCategory(fameIndex);
}
std::size_t LegacyOutputs::PublishCurrentFameGlobals(
    LegacyOutputSink& sink, const FameValueResolver& resolve,
    const std::string& currentLocationName) const {
    std::size_t written = 0;
    for (FameCategory category = FameCategory::Whore; category != FameCategory::Count;
         category = static_cast<FameCategory>(FameCategoryIndex(category) + 1)) {
        const int value =
            resolve ? resolve(currentLocationName, category) : 0;
        sink.SetCurrentFameGlobal(CurrentFameGlobalKey(category), value);
        ++written;
    }
    return written;
}
std::size_t LegacyOutputs::PublishStorageUtilLists(LegacyOutputSink& sink,
                                                   const FameValueResolver& resolve) const {
    std::size_t written = 0;
    for (std::size_t locationIndex = 0;
         locationIndex < kLegacyLocationNames.size(); ++locationIndex) {
        const auto locationName = LegacyLocationName(locationIndex);
        for (std::size_t fameIndex = 0; fameIndex < kLegacyFameSlots.size();
             ++fameIndex) {
            int value = 0;
            if (const auto category = StorageFameCategory(fameIndex)) {
                if (const auto& name = locationName) {
                    value = resolve ? resolve(*name, *category) : 0;
                }
            }
            sink.SetStorageIntList(StorageFameKey(fameIndex), locationIndex, value);
            ++written;
        }
    }
    return written;
}
std::size_t LegacyOutputs::PublishLegacyBridge(LegacyOutputSink& sink,
                                               const FameValueResolver& resolve,
                                               const std::string& currentLocationName) const {
    std::size_t written = 0;
    for (std::size_t fameIndex = 0; fameIndex < kLegacyFameSlots.size();
         ++fameIndex) {
        int value = 0;
        if (const auto category = LegacyBridgeCategory(fameIndex)) {
            value = resolve ? resolve(currentLocationName, *category) : 0;
        }
        sink.SetLegacyBridgeGlobal(LegacyBridgeKey(fameIndex), value);
        ++written;
    }
    return written;
}
}
