#pragma once

#include "slsf/core/Types.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace slsf::engine {

enum class PluginType : unsigned char {
    Unknown = 0,
    Esm,
    Esp,
    Esl,
};

struct LoadedPlugin {
    std::string name;
    PluginType type{PluginType::Unknown};
};

// Queries are only legal from the game's main thread with a loaded game.  The
// adapter is responsible for filling this from SKSE/CommonLib before every
// call; the pure services reject any other context before touching the backend.
struct EngineContext {
    bool mainThread{};
    bool inGame{};
};

[[nodiscard]] inline bool IsLegalContext(const EngineContext& context) noexcept {
    return context.mainThread && context.inGame;
}
inline constexpr std::size_t kEquipmentSlotCount = 61;

struct EquippedItemSnapshot {
    bool present{};
    std::vector<std::string> keywords;
};

struct EquipmentSnapshot {
    std::array<EquippedItemSnapshot, kEquipmentSlotCount> slots{};
};

// A complete copy of the engine facts needed by the gameplay rules.  Fact
// states distinguish a known-false observation (KnownFalse) from data the
// engine could not provide (Unknown/Missing/Incompatible).
//
// Ready means the snapshot is complete.  A non-Ready snapshot is a partial
// observation that remains dirty and is retried by the next refresh.  A
// backend that cannot resolve the actor at all should instead return
// Error::Code::NotFound; that error is terminal for the current game load.
struct ActorSnapshot {
    Status status{Status::Ready};
    StableFormKey actor;
    StableFormKey player;
    std::string name;

    StableFormKey location;
    Fact loaded{FactState::Unknown};
    Fact validLocation{FactState::Unknown};
    Fact dead{FactState::Unknown};
    Fact inCombat{FactState::Unknown};
    Fact sexLabActive{FactState::Unknown};
    Fact lineOfSight{FactState::Unknown};
    Fact playerDetected{FactState::Unknown};

    Fact distanceKnown{FactState::Unknown};
    double distance{};

    Fact relationshipKnown{FactState::Unknown};
    int relationshipRank{};

    Fact detectionKnown{FactState::Unknown};
    int detectionLevel{};

    EquipmentSnapshot equipment;
};

[[nodiscard]] inline bool IsRelationshipRankValid(int rank) noexcept {
    return rank >= -4 && rank <= 4;
}
[[nodiscard]] inline Status StatusFromError(const Error& error) noexcept {
    switch (error.code) {
    case Error::Code::InvalidArgument:
    case Error::Code::OutOfRange:
        return Status::Invalid;
    case Error::Code::NotFound:
        return Status::Missing;
    case Error::Code::InvalidState:
    case Error::Code::Io:
    case Error::Code::Internal:
        return Status::NotReady;
    case Error::Code::Unsupported:
        return Status::Incompatible;
    }
    return Status::Invalid;
}
// The adapter seam.  QueryActor is one call per target so tests can assert the
// exact number of engine queries during a dirty refresh.
class IEngineBackend {
public:
    virtual ~IEngineBackend() = default;

    [[nodiscard]] virtual EngineContext Context() const = 0;
    [[nodiscard]] virtual Result<std::vector<LoadedPlugin>> CollectLoadedPlugins() const = 0;
    [[nodiscard]] virtual Result<ActorSnapshot> QueryActor(const StableFormKey& actor,
                                                           const StableFormKey& player) const = 0;
};

}