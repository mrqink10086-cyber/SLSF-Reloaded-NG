#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace diffref {

inline constexpr std::size_t kSlots = 32;
inline constexpr std::size_t kCategories = 25;

using Row = std::array<double, kSlots>;

struct Edge {
    std::uint32_t source{};
    std::uint32_t target{};
    double coefficient{1.0};
};

// CSR graph keyed by target account.
struct ReferenceGraph {
    std::vector<Edge> operands;
    std::vector<std::uint32_t> offsets;
    std::uint32_t accountCount{};
};

// Uniform int source: one die = [low, high] (inclusive on both ends).
struct IDie {
    virtual ~IDie() = default;
    virtual int UniformInt(int low, int high) = 0;
};

class Die final : public IDie {
public:
    explicit Die(std::uint64_t seed) : state_(seed == 0 ? 0x9E3779B97F4A7C15ULL : seed) {}

    int UniformInt(int low, int high) override {
        if (high < low) {
            std::swap(low, high);
        }
        const std::uint64_t span = static_cast<std::uint64_t>(high - low) + 1;
        return low + static_cast<int>(Next() % span);
    }

    std::uint64_t Next() {
        std::uint64_t x = state_;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        state_ = x;
        return x * 0x2545F4914F9C4F3DULL;
    }

private:
    std::uint64_t state_;
};

struct Parameters {
    double decayPerDay{2.0};
    double relaxationPerDay{1.0};
    double spreadThreshold{30.0};
    double maxFame{150.0};
    double protectionDays{0.5};
};

struct AccountResult {
    Row local{};
    Row remote{};
    std::array<double, kSlots> protection{}; // absolute protection deadline per slot.
    double settledAt{0.0};                   // decay anchor after the batch.
    double remoteSettledAt{0.0};              // relaxation clock after the batch.
};

struct AdvanceResult {
    std::vector<AccountResult> accounts;
    double advancedTo{0.0};
};

struct GainInputs {
    double previousFame{0.0};
    int sample{1};
    double iterationMultiplier{1.0};
    double maxFame{150.0};
    double multiplier{1.0};
    bool nightEnabled{false};
    double hour{0.0};
    double nightStart{22.0};
    double nightEnd{6.0};
    bool foreplay{false};
};

struct GainOutcome {
    double newLocal{};
    int gained{};
    bool halved{};
    bool floored{};
};

// Legacy gain: truncate sample * multiplier (halved for night+foreplay) before
// the iteration multiplier, floor at 1, cap at maxFame.
GainOutcome Gain(const GainInputs& inputs);

struct SpreadInputs {
    double source{};
    double target{};
    int percent;
    double maxFame{150.0};
};

struct SpreadOutcome {
    double newValue{};
    int usedPercent{};
};

// Legacy manual spread: percent clamp [10, 100], then truncate and clamp to
// [0, maxFame].
SpreadOutcome ManualSpread(const SpreadInputs& inputs);
int ManualDecayAmount(int legacyMin, int legacyMax, IDie& die, double maxFame = 150.0);

// Legacy decay roll: when cap(previous) > 0, uniform integer [1, cap] *
// multiplier truncated per owed period; whole-batch total floored at one point.
struct DecayInputs {
    double previous;
    double multiplier;
    int owedPeriods;
};

struct DecayOutcome {
    double newLocal{};
    int perPeriodAmount{};
};

DecayOutcome DecayRoll(const DecayInputs& inputs, IDie& die,
                       double maxFame = 150.0);


// One hop: incoming[t][s] = max over edges(t) of coefficient * max(l, r)[src]
// with the threshold cutoff applied (below cutoff -> excluded; a slot without
// a passing candidate receives 0).
void ReferenceIncoming(std::vector<Row>& incoming,
                       const std::vector<Row>& local,
                       const std::vector<Row>& remote,
                       const ReferenceGraph& graph,
                       double cutoff);

// Remote row advance after one hop:
//   incoming >= remote : remote = incoming
//   incoming <  remote : remote = max(incoming, remote - relaxation * dt)
void ReferenceRelax(Row& remote,
                    const Row& incoming,
                    double elapsedDays,
                    double relaxationPerDay,
                    double maxFame = 150.0);

// Scheduled command kinds represented by the independent settlement harness.
enum class ScheduledKind : std::uint8_t {
    Set,
    Clear,
};

struct ScheduledChange {
    double at;
    ScheduledKind kind{ScheduledKind::Set};
    std::uint32_t account;
    std::size_t category;
    double value;
};

// Advance independently through each due command's business-time boundary,
// applying local decay and one frozen hop before the command, then advance to
// nowDays. Set and Clear reset the target's decay anchor to their boundary.
// `settledAt` carries whole-period decay anchors; `remoteSettledAt` the fully
// anchored relaxation clocks (0.0 sentinel snaps on the first segment).
AdvanceResult AdvanceReference(std::vector<Row> local,
                               std::vector<Row> remote,
                               const std::vector<double>& settledAt,
                               const std::vector<double>& remoteSettledAt,
                               const std::vector<Row>& protection,
                               const ReferenceGraph& graph,
                               const Parameters& params,
                               double nowDays,
                               IDie& decayDie,
                               double decayMultiplier = 1.0,
                               const std::vector<ScheduledChange>& scheduled = {});

// Compares two rows bit for bit.
bool RowEqual(const Row& a, const Row& b);

double PublishedValue(const Row& local, const Row& remote, std::size_t slot);

std::string FormatRow(const Row& row);

}
