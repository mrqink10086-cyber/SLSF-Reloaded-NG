#include <catch2/catch_test_macros.hpp>

#include "slsf/core/Types.h"
#include "slsf/core/reputation/ProviderRegistry.h"

#include <cstdint>
#include <vector>

namespace {

slsf::ProviderCommand MakeProvider(std::uint64_t id, slsf::ProviderPhase phase,
                                   std::vector<slsf::FameCategory> categories) {
    slsf::ProviderCommand command;
    command.registrationId = id;
    command.phase = phase;
    command.categories = std::move(categories);
    return command;
}

}

TEST_CASE("two providers contribute to the same category", "[providers]") {
    slsf::ProviderState state;
    const auto category = slsf::FameCategory::Whore;

    auto a = slsf::UpdateProvider(state, MakeProvider(1, slsf::ProviderPhase::Enabled, {category}));
    auto b = slsf::UpdateProvider(state, MakeProvider(2, slsf::ProviderPhase::Enabled, {category}));
    REQUIRE(a.status == slsf::Status::Ready);
    REQUIRE(b.status == slsf::Status::Ready);
    REQUIRE(state.ActiveProviderCount(category) == 2);
    REQUIRE(state.IsActive(category));

    const auto disabled =
        slsf::UpdateProvider(state, MakeProvider(1, slsf::ProviderPhase::Disabled, {category}));
    REQUIRE(disabled.status == slsf::Status::Ready);
    REQUIRE(disabled.kind == slsf::ProviderTransitionKind::Disabled);
    REQUIRE(disabled.wipeCategories.empty());
    REQUIRE(state.ActiveProviderCount(category) == 1);
    REQUIRE(state.IsActive(category));
    REQUIRE(state.ActiveProviderCount(slsf::FameCategory::Cuck) == 0);
}

TEST_CASE("unregister removes only its own contributions", "[providers]") {
    slsf::ProviderState state;
    const auto whore = slsf::FameCategory::Whore;
    const auto slut = slsf::FameCategory::Slut;
    REQUIRE(slsf::UpdateProvider(state, MakeProvider(1, slsf::ProviderPhase::Enabled,
                                                     {whore, slut}))
                .status == slsf::Status::Ready);
    REQUIRE(slsf::UpdateProvider(state, MakeProvider(2, slsf::ProviderPhase::Enabled, {whore}))
                .status == slsf::Status::Ready);

    const auto removed = slsf::UpdateProvider(
        state, MakeProvider(1, slsf::ProviderPhase::Unregistered, {whore, slut}));
    REQUIRE(removed.status == slsf::Status::Ready);
    REQUIRE(removed.kind == slsf::ProviderTransitionKind::Unregistered);
    REQUIRE(removed.wipeCategories.empty());
    REQUIRE(state.ActiveProviderCount(whore) == 1);
    REQUIRE(state.ActiveProviderCount(slut) == 0);
    REQUIRE(state.IsActive(whore));
}

TEST_CASE("temporary provider unavailability preserves registered contributions", "[providers]") {
    for (const auto phase : {slsf::ProviderPhase::NotReady, slsf::ProviderPhase::Stale}) {
        slsf::ProviderState state;
        const auto optional = slsf::FameCategory::CumDump;
        const auto regular = slsf::FameCategory::Nasty;
        REQUIRE(slsf::UpdateProvider(state, MakeProvider(10, slsf::ProviderPhase::Enabled,
                                                        {optional, regular}))
                    .status == slsf::Status::Ready);

        for (int attempt = 0; attempt < 2; ++attempt) {
            const auto unavailable = slsf::UpdateProvider(state, MakeProvider(10, phase, {}));
            REQUIRE(state.ActiveProviderCount(optional) == 1);
            REQUIRE(state.ActiveProviderCount(regular) == 1);
            REQUIRE(unavailable.wipeCategories.empty());
            REQUIRE(unavailable.status == (phase == slsf::ProviderPhase::NotReady
                                              ? slsf::Status::NotReady : slsf::Status::Stale));
            REQUIRE(unavailable.kind == slsf::ProviderTransitionKind::None);
            REQUIRE_FALSE(unavailable.changed);
            REQUIRE(state.IsActive(optional));
            REQUIRE(state.IsActive(regular));
            REQUIRE(state.stopped.empty());
        }

        const auto recovered = slsf::UpdateProvider(
            state, MakeProvider(10, slsf::ProviderPhase::Enabled, {optional, regular}));
        REQUIRE_FALSE(recovered.changed);
        REQUIRE(state.ActiveProviderCount(optional) == 1);
        const auto missing = slsf::UpdateProvider(
            state, MakeProvider(10, slsf::ProviderPhase::Missing, {}));
        REQUIRE(missing.wipeCategories == std::vector<slsf::FameCategory>{optional});
        REQUIRE_FALSE(state.IsActive(regular));
    }
}

TEST_CASE("unavailable unregistered providers do not create persistent state", "[providers]") {
    slsf::ProviderState state;
    for (const auto phase : {slsf::ProviderPhase::NotReady, slsf::ProviderPhase::Stale}) {
        const auto unavailable = slsf::UpdateProvider(state, MakeProvider(99, phase, {}));
        REQUIRE_FALSE(unavailable.changed);
        REQUIRE(unavailable.wipeCategories.empty());
        REQUIRE(state.contributions.empty());
        REQUIRE(state.stopped.empty());
    }
}

TEST_CASE("confirmed invalidations wipe optional categories once per generation", "[providers]") {
    const std::vector<slsf::ProviderPhase> phases = {
        slsf::ProviderPhase::Missing,
        slsf::ProviderPhase::Incompatible,
    };
    const auto category = slsf::FameCategory::CumDump;

    for (const auto phase : phases) {
        slsf::ProviderState state;
        REQUIRE(slsf::UpdateProvider(state, MakeProvider(10, slsf::ProviderPhase::Enabled,
                                                         {category}))
                    .status == slsf::Status::Ready);
        REQUIRE(slsf::UpdateProvider(state, MakeProvider(11, slsf::ProviderPhase::Enabled,
                                                         {category}))
                    .status == slsf::Status::Ready);

        const auto first =
            slsf::UpdateProvider(state, MakeProvider(10, phase, {category}));
        REQUIRE(first.status == slsf::Status::Ready);
        REQUIRE(first.kind == slsf::ProviderTransitionKind::Invalidated);
        REQUIRE(first.wipeCategories.empty());
        REQUIRE(state.IsActive(category));

        const auto second =
            slsf::UpdateProvider(state, MakeProvider(11, phase, {category}));
        REQUIRE(second.status == slsf::Status::Ready);
        REQUIRE(second.kind == slsf::ProviderTransitionKind::Invalidated);
        REQUIRE(second.wipeCategories == std::vector<slsf::FameCategory>{category});
        REQUIRE_FALSE(state.IsActive(category));

        const auto repeated =
            slsf::UpdateProvider(state, MakeProvider(11, phase, {category}));
        REQUIRE(repeated.status == slsf::Status::Ready);
        REQUIRE(repeated.wipeCategories.empty());

        REQUIRE(slsf::UpdateProvider(state, MakeProvider(11, slsf::ProviderPhase::Enabled,
                                                         {category}))
                    .status == slsf::Status::Ready);
        REQUIRE(state.IsActive(category));
        const auto again =
            slsf::UpdateProvider(state, MakeProvider(11, phase, {category}));
        REQUIRE(again.wipeCategories == std::vector<slsf::FameCategory>{category});
    }
}

TEST_CASE("automatic invalidation wipes only the four optional categories", "[providers]") {
    for (const auto phase : {slsf::ProviderPhase::Missing, slsf::ProviderPhase::Incompatible}) {
        slsf::ProviderState state;
        const std::vector<slsf::FameCategory> all(slsf::kEspFameCategoryOrder.begin(),
                                                  slsf::kEspFameCategoryOrder.end());
        REQUIRE(slsf::UpdateProvider(state, MakeProvider(1, slsf::ProviderPhase::Enabled, all))
                    .status == slsf::Status::Ready);
        const auto removed = slsf::UpdateProvider(state, MakeProvider(1, phase, {}));
        REQUIRE(removed.wipeCategories == std::vector<slsf::FameCategory>{
            slsf::FameCategory::CumDump, slsf::FameCategory::Unfaithful,
            slsf::FameCategory::Cuck, slsf::FameCategory::Airhead});
        REQUIRE(state.contributions.empty());
    }
}

TEST_CASE("provider categories stay isolated", "[providers]") {
    slsf::ProviderState state;
    const auto cumDump = slsf::FameCategory::CumDump;
    const auto cuck = slsf::FameCategory::Cuck;
    REQUIRE(slsf::UpdateProvider(state, MakeProvider(7, slsf::ProviderPhase::Enabled,
                                                     {cumDump}))
                .status == slsf::Status::Ready);
    REQUIRE(state.IsActive(cumDump));
    REQUIRE_FALSE(state.IsActive(cuck));

    const auto invalidated =
        slsf::UpdateProvider(state, MakeProvider(7, slsf::ProviderPhase::Missing, {cumDump}));
    REQUIRE(invalidated.wipeCategories == std::vector<slsf::FameCategory>{cumDump});
    REQUIRE_FALSE(state.IsActive(cumDump));
    REQUIRE_FALSE(state.IsActive(cuck));
}

TEST_CASE("invalidating a provider revokes all of its categories", "[providers]") {
    slsf::ProviderState state;
    const auto cumDump = slsf::FameCategory::CumDump;
    const auto cuck = slsf::FameCategory::Cuck;
    REQUIRE(slsf::UpdateProvider(state, MakeProvider(20, slsf::ProviderPhase::Enabled,
                                                     {cumDump, cuck}))
                .status == slsf::Status::Ready);
    REQUIRE(state.ActiveProviderCount(cumDump) == 1);
    REQUIRE(state.ActiveProviderCount(cuck) == 1);

    const auto invalidated =
        slsf::UpdateProvider(state, MakeProvider(20, slsf::ProviderPhase::Incompatible, {}));
    REQUIRE(invalidated.status == slsf::Status::Ready);
    REQUIRE(invalidated.kind == slsf::ProviderTransitionKind::Invalidated);
    REQUIRE(invalidated.wipeCategories ==
            std::vector<slsf::FameCategory>{cumDump, cuck});
    REQUIRE_FALSE(state.IsActive(cumDump));
    REQUIRE_FALSE(state.IsActive(cuck));
    REQUIRE(state.contributions.empty());
}

TEST_CASE("duplicate categories count once and capacity is bounded", "[providers]") {
    slsf::ProviderState state;
    const auto category = slsf::FameCategory::Gentle;
    REQUIRE(slsf::UpdateProvider(state, MakeProvider(1, slsf::ProviderPhase::Enabled,
                                                     {category, category, category}))
                .status == slsf::Status::Ready);
    REQUIRE(state.ActiveProviderCount(category) == 1);

    for (std::uint64_t id = 2; id <= slsf::kMaxProviderRegistrations; ++id) {
        REQUIRE(slsf::UpdateProvider(state, MakeProvider(id, slsf::ProviderPhase::Enabled,
                                                         {category}))
                    .status == slsf::Status::Ready);
    }
    REQUIRE(state.contributions.size() == slsf::kMaxProviderRegistrations);
    REQUIRE(slsf::UpdateProvider(state, MakeProvider(
                                      slsf::kMaxProviderRegistrations + 1,
                                      slsf::ProviderPhase::Enabled, {category}))
                .status == slsf::Status::Invalid);
}

TEST_CASE("empty registration removal reports the durable change", "[providers][regression]") {
    slsf::ProviderState state;
    REQUIRE(slsf::UpdateProvider(state, MakeProvider(1, slsf::ProviderPhase::Enabled, {})).changed);
    REQUIRE(slsf::UpdateProvider(state, MakeProvider(1, slsf::ProviderPhase::Unregistered, {}))
                .changed);
    REQUIRE(state.contributions.empty());
    REQUIRE_FALSE(slsf::UpdateProvider(state, MakeProvider(1, slsf::ProviderPhase::Unregistered, {}))
                      .changed);
}

TEST_CASE("provider commands reject invalid input", "[providers]") {
    slsf::ProviderState state;
    const auto category = slsf::FameCategory::Whore;
    REQUIRE(slsf::UpdateProvider(state, MakeProvider(0, slsf::ProviderPhase::Enabled, {category}))
                .status == slsf::Status::Invalid);
    REQUIRE(slsf::UpdateProvider(state, MakeProvider(1, slsf::ProviderPhase::Enabled,
                                                     {slsf::FameCategory::Count}))
                .status == slsf::Status::Invalid);
    REQUIRE(slsf::UpdateProvider(state, MakeProvider(1, slsf::ProviderPhase::Enabled, {category}))
                .status == slsf::Status::Ready);
    const auto invalidPhase =
        slsf::UpdateProvider(state, MakeProvider(1, static_cast<slsf::ProviderPhase>(200),
                                                 {category}));
    REQUIRE(invalidPhase.status == slsf::Status::Invalid);
    REQUIRE(state.IsActive(category));
}
