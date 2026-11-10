#pragma once

#include "slsf/core/Commands.h"
#include "slsf/core/Snapshots.h"
#include "slsf/core/Types.h"
#include "slsf/core/reputation/GainRules.h"
#include "slsf/compat/LegacyOutputs.h"
#include "slsf/compat/ModEventBridge.h"
#include "slsf/runtime/Dispatcher.h"
#include "slsf/io/JsonTransfer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace slsf::api {

using slsf::CommandResult;
using slsf::runtime::Dispatcher;
using slsf::Epoch;
using slsf::ExplicitCommand;
using slsf::ExplicitCommandKind;
using slsf::FameCategory;
using slsf::GainFameInputs;
using slsf::GameTime;
using slsf::RegionId;
using slsf::Revision;
using slsf::Status;

struct RegionalFameView {
    Epoch epoch;
    Revision revision;
    std::string location;
    std::string currentLocation;
    std::vector<std::string> locations;
    std::optional<std::array<int, kFameCategoryCount>> values;
};

class PublicApi {
public:
    PublicApi(Dispatcher& executor, const slsf::RegionSnapshot& regions);

    [[nodiscard]] Dispatcher& Executor() const noexcept { return executor_; }
    [[nodiscard]] bool BusinessOpen() const { return executor_.Attached(); }
    [[nodiscard]] Epoch CurrentEpoch() const { return executor_.CurrentEpoch(); }
    [[nodiscard]] bool IsEpochCurrent(Epoch epoch) const {
        return BusinessOpen() && epoch == executor_.CurrentEpoch();
    }
    [[nodiscard]] GameTime CurrentGameTime() const { return executor_.Clocks().Game(); }
    [[nodiscard]] slsf::PublishedStatePtr Snapshot() const noexcept {
        return executor_.Snapshot();
    }
    [[nodiscard]] Revision LastPublishedRevision() const;
    [[nodiscard]] RegionalFameView ReadRegionalFame(const std::string& location) const;

    [[nodiscard]] std::optional<RegionId> ResolveRegionByName(const std::string& name) const;
    [[nodiscard]] static std::optional<FameCategory> ResolveCategory(const std::string& name);

    using FormRegionResolver =
        std::function<std::optional<RegionId>(const slsf::StableFormKey&)>;
    void SetFormRegionResolver(FormRegionResolver resolver);
    [[nodiscard]] std::optional<RegionId> ResolveForm(const slsf::StableFormKey& form) const;
    [[nodiscard]] std::optional<RegionId> ResolveParamLocation(const compat::ModEvent& event,
                                                              const std::string& paramName) const;

    [[nodiscard]] CommandResult SetFameValueRegion(RegionId region, FameCategory category,
                                                  double value);
    [[nodiscard]] CommandResult ClearFameRegion(RegionId region, FameCategory category);
    [[nodiscard]] CommandResult ClearAllFameRegion(RegionId region);
    [[nodiscard]] CommandResult SetFameValue(const std::string& locationName,
                                            const std::string& categoryName, int value);
    [[nodiscard]] CommandResult ClearFame(const std::string& locationName,
                                         const std::string& categoryName);
    [[nodiscard]] CommandResult ClearAllFame(const std::string& locationName);
    [[nodiscard]] std::optional<int> GetFameValueRegion(RegionId region,
                                                       FameCategory category) const;
    [[nodiscard]] std::optional<int> GetFameValue(const std::string& locationName,
                                                 const std::string& categoryName) const;

    [[nodiscard]] Dispatcher::GainOutcome GainFame(const std::string& locationName,
                                                  const std::string& categoryName,
                                                  const GainFameInputs& ruleInputs = {});
    [[nodiscard]] Dispatcher::ApplyOutcome ManualSpread(const std::string& fromName,
                                                       const std::string& toName,
                                                       const std::string& categoryName,
                                                       int percent);
    [[nodiscard]] Dispatcher::ApplyOutcome ManualDecay(const std::string& locationName,
                                                      const std::string& categoryName,
                                                      int minimum, int maximum);
    [[nodiscard]] Dispatcher::ApplyOutcome ManualGain(const std::string& locationName,
                                                      const std::string& categoryName,
                                                      int minimum, int maximum);
    [[nodiscard]] CommandResult SpreadPulse(const std::vector<std::string>& sourceNames);

    [[nodiscard]] static const std::vector<std::string>& FlagNames();
    [[nodiscard]] static bool IsKnownFlag(const std::string& name);
    [[nodiscard]] static std::string NormalizeFlag(const std::string& name);
    [[nodiscard]] bool RegisterMod(const std::string& modName);
    [[nodiscard]] bool UnregisterMod(const std::string& modName);
    [[nodiscard]] bool IsModRegistered(const std::string& modName) const;
    [[nodiscard]] std::size_t CountExternalMods() const;
    void SetExternalFlag(const std::string& modName, const std::string& flagName, bool flag);
    [[nodiscard]] bool GetModFlag(const std::string& modName, const std::string& flagName) const;
    [[nodiscard]] bool GetAggregatedFlag(const std::string& flagName) const;
    [[nodiscard]] std::vector<std::string> RegisteredMods() const;
    [[nodiscard]] CommandResult ImportLegacy(const io::LegacyImport& input);
    [[nodiscard]] io::LegacyDocument ExportLegacy() const;
    [[nodiscard]] bool RegisterCustomLocation(const std::string& name, const StableRegionKey& key);
    [[nodiscard]] bool UnregisterCustomLocation(const std::string& name);
    [[nodiscard]] std::vector<CustomRegionState> CustomLocations() const;
    [[nodiscard]] std::string RegionName(RegionId region) const;
    [[nodiscard]] int SpreadChance(const std::string& name) const;
    [[nodiscard]] std::vector<std::string> RegisteredLocations() const;
    using GameplayCall = std::function<bool(const std::string&, const std::vector<compat::ModEventValue>&, compat::ModEventValue&)>;
    void SetGameplayCall(GameplayCall call);
    bool CallGameplay(const std::string& name, const std::vector<compat::ModEventValue>& arguments, compat::ModEventValue& result) const;
    [[nodiscard]] bool SetSpreadChance(const std::string& name, int chance);

    void SetCurrentLocation(const std::string& name);
    [[nodiscard]] std::string CurrentLocation() const;
    using StrictLocationResolver = std::function<std::optional<std::string>()>;
    void SetStrictLocationResolver(StrictLocationResolver resolver);
    [[nodiscard]] compat::FameValueResolver CommittedResolver() const;
    [[nodiscard]] std::size_t PublishCurrentFameGlobals(compat::LegacyOutputSink& sink) const;
    [[nodiscard]] std::size_t PublishStorageUtilLists(compat::LegacyOutputSink& sink) const;
    [[nodiscard]] std::size_t PublishLegacyBridge(compat::LegacyOutputSink& sink) const;
    [[nodiscard]] std::size_t PublishAllOutputs(compat::LegacyOutputSink& sink) const;

    void AttachToBridge(compat::ModEventBridge& bridge);
    void OnEvent(const compat::ModEvent& event);

    using ReplySender =
        std::function<bool(const std::string&, const std::vector<compat::ModEventValue>&)>;
    void SetReplySender(ReplySender sender);

private:
    [[nodiscard]] std::optional<RegionId> ResolveRegionByName(
        const std::string& name, const PublishedStatePtr& snapshot) const;
    bool SendReply(const std::string& eventName,
                  const std::vector<compat::ModEventValue>& values) const;
    static compat::ModEventValue MakeBool(bool v);
    static compat::ModEventValue MakeInt(std::int64_t v);
    static compat::ModEventValue MakeFloat(double v);
    static compat::ModEventValue MakeString(const std::string& v);

    std::string ResolveParamLocationName(const compat::ModEvent& event,
                                         const char* param) const;

    bool Resolve(const std::string& locationName, const std::string& categoryName,
                RegionId& region, FameCategory& category) const;

    void HandleGain(const compat::ModEvent& event);
    void HandleManualGain(const compat::ModEvent& event);
    void HandleDecay(const compat::ModEvent& event);
    void HandleSpread(const compat::ModEvent& event);
    void HandleFlag(const compat::ModEvent& event, const std::string& flagName);
    void HandleEnableFameType(const compat::ModEvent& event);
    void HandleRequestLocation(const compat::ModEvent& event);
    void HandleRequestRecognitionTime(const compat::ModEvent& event);
    void HandleRequestFame(const compat::ModEvent& event);
    void HandleRequestCumVisibility(const compat::ModEvent& event);
    void HandleRequestFlagState(const compat::ModEvent& event);
    void HandleRequestModFlagState(const compat::ModEvent& event);
    void HandleRequestModRegisterState(const compat::ModEvent& event);

    [[nodiscard]] static std::optional<FameCategory> CategoryFromFlagHandler(
        const std::string& handler);

    mutable std::mutex formMutex_;
    FormRegionResolver formResolver_;

    mutable std::mutex locationMutex_;
    std::string currentLocation_;

    mutable std::mutex strictMutex_;
    StrictLocationResolver strictResolver_;

    mutable std::mutex replyMutex_;
    ReplySender replySender_;
    mutable std::mutex gameplayMutex_;
    GameplayCall gameplayCall_;

    Dispatcher& executor_;
    const slsf::RegionSnapshot& regions_;
    compat::LegacyOutputs outputs_;
};

}
