#include "slsf/integrations/SlaveTats.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <utility>

namespace slsf::integrations {

namespace {

static_assert(sizeof(void*) == 8);
static_assert(sizeof(JcString) == 8);
static_assert(offsetof(JcString, opaque) == 0);
static_assert(static_cast<std::size_t>(gameplay::TattooArea::Body) ==
                  static_cast<std::size_t>(SlaveTatsArea::Body) &&
              static_cast<std::size_t>(gameplay::TattooArea::Face) ==
                  static_cast<std::size_t>(SlaveTatsArea::Face) &&
              static_cast<std::size_t>(gameplay::TattooArea::Hands) ==
                  static_cast<std::size_t>(SlaveTatsArea::Hands) &&
              static_cast<std::size_t>(gameplay::TattooArea::Feet) ==
                  static_cast<std::size_t>(SlaveTatsArea::Feet));

struct AddressesTable {
    std::uint32_t current_version;
    void* pointers[kSlaveTatsKnownPrefixPointers];
};

static_assert(offsetof(AddressesTable, current_version) == 0);
static_assert(offsetof(AddressesTable, pointers) == 8);
static_assert(offsetof(AddressesTable, pointers) + 13 * sizeof(void*) == 112);
static_assert(offsetof(AddressesTable, pointers) + 14 * sizeof(void*) == 120);

inline constexpr std::size_t kGetAppliedTattooInSlotIndex = 13;
inline constexpr std::size_t kGetAppliedTattoosByAreaIndex = 14;

using GetAppliedTattoosByAreaFn = bool (*)(void* target, std::int32_t, std::int32_t, std::int32_t,
                                           std::int32_t);
std::int32_t SafeCount(const JcTable& table, std::int32_t handle) noexcept {
    return table.arrayCount ? table.arrayCount(table.domain, handle) : 0;
}

std::int32_t SafeGetObject(const JcTable& table, std::int32_t arrayHandle,
                           std::int32_t index) noexcept {
    return table.arrayGetObj ? table.arrayGetObj(table.domain, arrayHandle, index, 0) : 0;
}

JcString SafeGetString(const JcTable& table, std::int32_t object, JcString key,
                       JcString fallback) noexcept {
    return table.mapGetStr ? table.mapGetStr(table.domain, object, key, fallback) : fallback;
}

std::int32_t SafeGetInt(const JcTable& table, std::int32_t object, JcString key,
                        std::int32_t fallback) noexcept {
    return table.mapGetInt ? table.mapGetInt(table.domain, object, key, fallback) : fallback;
}

std::int32_t SafeCreate(const JcTable& table) noexcept {
    return table.arrayObject ? table.arrayObject(table.domain) : 0;
}

}

const char* SlaveTatsAreaName(SlaveTatsArea value) noexcept {
    switch (value) {
    case SlaveTatsArea::Body: return "Body";
    case SlaveTatsArea::Face: return "Face";
    case SlaveTatsArea::Hands: return "Hands";
    case SlaveTatsArea::Feet: return "Feet";
    }
    return "Unknown";
}

bool SlaveTatsAreaRecords::HasRecordAtSlot(std::int32_t slot) const noexcept {
    return std::any_of(placements.begin(), placements.end(),
                       [&](const SlaveTatsPlacement& item) { return item.slot == slot; });
}

const SlaveTatsAreaRecords& SlaveTatsSnapshot::Area(SlaveTatsArea area) const {
    return areas.at(static_cast<std::size_t>(area));
}

std::vector<gameplay::TattooSlotFacts> SlaveTatsSlotFacts(const SlaveTatsAreaRecords& records,
                                                          std::size_t capacity) {
    std::vector<gameplay::TattooSlotFacts> slots{
        capacity, gameplay::TattooSlotFacts{records.truncated || records.rejectedEntries != 0
                                              ? Fact{}
                                              : Fact::Known(false),
                                            gameplay::TattooSubcategory::Unknown}};
    for (const SlaveTatsPlacement& placement : records.placements) {
        if (placement.slot < 0 || static_cast<std::size_t>(placement.slot) >= capacity) {
            continue;
        }
        slots.at(static_cast<std::size_t>(placement.slot)).applied = Fact::Known(true);
    }
    return slots;
}

JcHandle::JcHandle(const JcTable* table, std::int32_t handle) noexcept
    : table_{table}, handle_{handle} {}

JcHandle::~JcHandle() {
    Release();
}

JcHandle::JcHandle(JcHandle&& other) noexcept
    : table_{std::exchange(other.table_, nullptr)}, handle_{std::exchange(other.handle_, 0)} {}

JcHandle& JcHandle::operator=(JcHandle&& other) noexcept {
    if (this != &other) {
        Release();
        table_ = std::exchange(other.table_, nullptr);
        handle_ = std::exchange(other.handle_, 0);
    }
    return *this;
}

std::int32_t JcHandle::get() const noexcept {
    return handle_;
}

bool JcHandle::Owns() const noexcept {
    return handle_ != 0 && table_ != nullptr;
}

void JcHandle::Release() noexcept {
    if (!Owns()) {
        return;
    }
    const JcTable* table = std::exchange(table_, nullptr);
    const std::int32_t handle = std::exchange(handle_, 0);
    if (table->release) {
        std::ignore = table->release(table->domain, handle);
    }
}

struct SlaveTats::Impl {
    SlaveTatsPort& port_;
    SlaveTatsOptions options_;
    mutable std::mutex mutex_;
    SlaveTatsCapability capability_{};
    SlaveTatsSnapshot snapshot_{};
    const void* addresses_{};
    ExternalGate gate_{ExternalGate::Waiting};
    Epoch epoch_{};
    bool epochSeen_{};
    bool dirty_{true};
    bool shuttingDown_{false};
    std::uint32_t attempts_{};
    std::string lastError_{};
    bool stringsReady_{};
    JcString keyName_{};
    JcString keySlot_{};
    JcString emptyString_{};
    std::array<JcString, kSlaveTatsAreaCount> areaNames_{};

    explicit Impl(SlaveTatsPort& port, SlaveTatsOptions options)
        : port_{port}, options_{options} {}

    void MakeStringsLocked() {
        if (stringsReady_) {
            return;
        }
        emptyString_ = port_.Intern("");
        keyName_ = port_.Intern("name");
        keySlot_ = port_.Intern("slot");
        for (std::size_t index = 0; index < kSlaveTatsAreaCount; ++index) {
            areaNames_.at(index) =
                port_.Intern(SlaveTatsAreaName(static_cast<SlaveTatsArea>(index)));
        }
        stringsReady_ = true;
    }

    bool ConsumeLocked(std::int64_t messageType, const void* data, std::size_t dataLen) {
        if (data == nullptr || dataLen != sizeof(void*)) {
            capability_.rejections += 1;
            capability_.reason =
                data == nullptr ? "interface message data is null"
                                : "interface message data length does not match sizeof(void*)";
            return false;
        }
        if (messageType != kSlaveTatsInterfaceMessageType) {
            capability_.rejections += 1;
            capability_.reason = "message type is not the SlaveTats interface broadcast";
            return false;
        }
        const auto* table = static_cast<const AddressesTable*>(data);
        if (table->current_version != kSlaveTatsInterfaceVersion) {
            capability_.state = CapabilityState::Incompatible;
            capability_.version = table->current_version;
            capability_.reason = "interface table version is outside the verified v1 prefix";
            if (gate_ == ExternalGate::Changed || gate_ == ExternalGate::Incompatible ||
                gate_ == ExternalGate::Ready) {
                gate_ = ExternalGate::Waiting;
            }
            return true;
        }
        addresses_ = data;
        const bool recovered = capability_.state == CapabilityState::Incompatible;
        capability_.state = CapabilityState::Available;
        capability_.version = table->current_version;
        capability_.confirmations += 1;
        capability_.reason.clear();
        if (recovered || gate_ == ExternalGate::Changed || gate_ == ExternalGate::Incompatible) {
            gate_ = ExternalGate::Waiting;
        }
        return true;
    }

    [[nodiscard]] bool ReadyForCollectLocked(std::string& reason) const {
        if (capability_.state != CapabilityState::Available) {
            reason = "SlaveTats interface broadcast has not been confirmed";
            return false;
        }
        const JcTable* table = port_.Table();
        if (table == nullptr || table->domain == nullptr) {
            reason = "JContainers default domain is unavailable";
            return false;
        }
        if (table->arrayCount == nullptr || table->arrayObject == nullptr ||
            table->arrayGetObj == nullptr) {
            reason = "JContainers container bindings are unavailable";
            return false;
        }
        const auto* addresses = static_cast<const AddressesTable*>(addresses_);
        if (addresses == nullptr) {
            reason = "SlaveTats addresses table is unavailable";
            return false;
        }
        if (addresses->pointers[kGetAppliedTattoosByAreaIndex] == nullptr) {
            reason = "batch tattoo query pointer is unavailable";
            return false;
        }
        return true;
    }

    bool ReadAreaLocked(const JcTable& table, std::int32_t arrayHandle, SlaveTatsAreaRecords& out) {
        out = SlaveTatsAreaRecords{};
        const std::int32_t count = SafeCount(table, arrayHandle);
        if (count < 0) {
            out.rejectedEntries += 1;
            return false;
        }
        for (std::int32_t index = 0; index < count; ++index) {
            const std::int32_t entry = SafeGetObject(table, arrayHandle, index);
            if (entry == 0) {
                out.rejectedEntries += 1;
                continue;
            }
            const std::int32_t slot = SafeGetInt(table, entry, keySlot_, -1);
            if (slot < 0) {
                out.rejectedEntries += 1;
                continue;
            }
            std::string name = port_.Text(SafeGetString(table, entry, keyName_, emptyString_));
            if (name.size() > kMaxSlaveTatsNameBytes) {
                out.rejectedEntries += 1;
                continue;
            }
            if (out.placements.size() < static_cast<std::size_t>(kMaxSlaveTatsPlacements)) {
                out.placements.push_back(SlaveTatsPlacement{std::move(name), slot});
            } else {
                out.rejectedEntries += 1;
            }
        }
        if (static_cast<int64_t>(count) > static_cast<int64_t>(kMaxSlaveTatsPlacements)) {
            out.truncated = true;
        }
        std::stable_sort(out.placements.begin(), out.placements.end(),
                         [](const SlaveTatsPlacement& left, const SlaveTatsPlacement& right) {
                             return left.slot < right.slot;
                         });
        return true;
    }

    void RecordFailureLocked(const std::string& reason) {
        attempts_ += 1;
        lastError_ = reason;
        gate_ = attempts_ >= options_.timing.maxAttempts ? ExternalGate::Failed
                                                         : ExternalGate::Waiting;
    }

    SlaveTatsCollectResult CollectLocked(std::uint32_t actorFormId, const Epoch& epoch) {
        if (shuttingDown_) {
            return {ExternalGate::Closed, false};
        }
        if (epoch != epoch_) {
            if (epochSeen_) {
                return {ExternalGate::Stale, false};
            }
            epochSeen_ = true;
            epoch_ = epoch;
        }
        std::string reason;
        if (!ReadyForCollectLocked(reason)) {
            RecordFailureLocked(reason);
            return {gate_, false};
        }
        void* actor = port_.ResolveActor(actorFormId);
        if (actor == nullptr) {
            RecordFailureLocked("actor could not be resolved for collection");
            return {gate_, false};
        }
        MakeStringsLocked();
        const JcTable* table = port_.Table();
        if (table->mapGetStr == nullptr || table->mapGetInt == nullptr ||
            table->release == nullptr) {
            RecordFailureLocked("JContainers field bindings are unavailable");
            return {gate_, false};
        }
        JcHandle body{table, SafeCreate(*table)};
        JcHandle face{table, SafeCreate(*table)};
        JcHandle hands{table, SafeCreate(*table)};
        JcHandle feet{table, SafeCreate(*table)};
        if (!body.Owns() || !face.Owns() || !hands.Owns() || !feet.Owns()) {
            RecordFailureLocked("JContainers temporary arrays could not be created");
            return {gate_, false};
        }
        const auto* addresses = static_cast<const AddressesTable*>(addresses_);
        const auto batch = reinterpret_cast<GetAppliedTattoosByAreaFn>(
            addresses->pointers[kGetAppliedTattoosByAreaIndex]);
        const bool failed =
            batch(actor, static_cast<std::int32_t>(body.get()), static_cast<std::int32_t>(face.get()),
                  static_cast<std::int32_t>(hands.get()), static_cast<std::int32_t>(feet.get()));
        if (failed) {
            RecordFailureLocked("batch tattoo query reported failure");
            return {gate_, false};
        }
        SlaveTatsSnapshot next;
        next.availability = ExternalAvailability::Available;
        next.interfaceVersion = capability_.version;
        next.epoch = epoch_;
        const std::array<std::int32_t, kSlaveTatsAreaCount> arrays{
            body.get(), face.get(), hands.get(), feet.get()};
        for (std::size_t index = 0; index < kSlaveTatsAreaCount; ++index) {
            SlaveTatsAreaRecords area;
            if (!ReadAreaLocked(*table, arrays.at(index), area)) {
                RecordFailureLocked("batch result array count is negative");
                return {gate_, false};
            }
            next.areas.at(index) = std::move(area);
        }
        bool changed = next.availability != snapshot_.availability ||
                       next.interfaceVersion != snapshot_.interfaceVersion;
        for (std::size_t index = 0; index < kSlaveTatsAreaCount && !changed; ++index) {
            changed = next.areas.at(index) != snapshot_.areas.at(index);
        }
        snapshot_ = std::move(next);
        dirty_ = false;
        attempts_ = 0;
        gate_ = ExternalGate::Ready;
        lastError_.clear();
        return {gate_, changed};
    }

    [[nodiscard]] ExternalGate GateLocked() const noexcept {
        if (shuttingDown_) {
            return ExternalGate::Closed;
        }
        if (capability_.state == CapabilityState::Unqueried) {
            return ExternalGate::Waiting;
        }
        return gate_;
    }
};

SlaveTats::SlaveTats(SlaveTatsPort& port, SlaveTatsOptions options)
    : impl_{std::make_shared<Impl>(port, std::move(options))} {}

SlaveTats::~SlaveTats() = default;

bool SlaveTats::ConsumeInterfaceMessage(std::int64_t messageType, const void* data,
                                        std::size_t dataLen) noexcept {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    return state->ConsumeLocked(messageType, data, dataLen);
}

SlaveTatsCapability SlaveTats::Capability() const {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    return state->capability_;
}

SlaveTatsCollectResult SlaveTats::Collect(std::uint32_t actorFormId, const Epoch& epoch) {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    return state->CollectLocked(actorFormId, epoch);
}

SlaveTatsSnapshot SlaveTats::Snapshot() const {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    return state->snapshot_;
}

ExternalGate SlaveTats::Gate() const {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    return state->GateLocked();
}

std::uint32_t SlaveTats::Attempts() const {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    return state->attempts_;
}

bool SlaveTats::Dirty() const {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    return state->dirty_;
}

std::string SlaveTats::LastError() const {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    return state->lastError_;
}

void SlaveTats::MarkDirty(const Epoch& eventEpoch) {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    if (eventEpoch != state->epoch_) {
        return;
    }
    state->dirty_ = true;
    state->attempts_ = 0;
    state->gate_ = ExternalGate::Changed;
}

void SlaveTats::OnSessionChanged(const Epoch& epoch) {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    state->epoch_ = epoch;
    state->epochSeen_ = true;
    state->snapshot_ = SlaveTatsSnapshot{};
    state->snapshot_.epoch = epoch;
    state->dirty_ = true;
    state->attempts_ = 0;
    state->gate_ = ExternalGate::Changed;
    state->lastError_.clear();
}

void SlaveTats::Cancel() {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    state->shuttingDown_ = true;
    state->gate_ = ExternalGate::Closed;
}

void SlaveTats::Reset() {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    state->snapshot_ = SlaveTatsSnapshot{};
    state->shuttingDown_ = false;
    state->gate_ = ExternalGate::Waiting;
    state->dirty_ = true;
    state->attempts_ = 0;
    state->lastError_.clear();
}

}
