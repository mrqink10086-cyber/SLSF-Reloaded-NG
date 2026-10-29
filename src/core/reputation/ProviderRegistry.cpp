#include "slsf/core/reputation/ProviderRegistry.h"

#include <algorithm>

namespace slsf {
namespace {

bool ValidPhase(ProviderPhase phase) {
    return static_cast<std::uint8_t>(phase) <=
           static_cast<std::uint8_t>(ProviderPhase::Unregistered);
}

bool IsRemoval(ProviderPhase phase) {
    return phase == ProviderPhase::Disabled || phase == ProviderPhase::Missing ||
           phase == ProviderPhase::Incompatible || phase == ProviderPhase::Unregistered;
}

bool IsInvalidation(ProviderPhase phase) {
    return phase == ProviderPhase::Missing || phase == ProviderPhase::Incompatible;
}

bool IsOptionalCategory(FameCategory category) {
    return category == FameCategory::CumDump || category == FameCategory::Unfaithful ||
           category == FameCategory::Cuck || category == FameCategory::Airhead;
}

CategoryMask CategorySet(const std::vector<FameCategory>& categories, bool& valid) {
    CategoryMask set;
    valid = true;
    for (const auto category : categories) {
        if (!IsValidFameCategory(category)) {
            valid = false;
            return {};
        }
        set.Set(category);
    }
    return set;
}

std::vector<FameCategory> SortedCategories(CategoryMask set) {
    std::vector<FameCategory> out;
    set.ForEach([&](FameCategory category) { out.push_back(category); });
    return out;
}

ProviderContribution* FindContribution(ProviderState& state, std::uint64_t id) {
    const auto it = std::find_if(state.contributions.begin(), state.contributions.end(),
                                 [id](const ProviderContribution& contribution) {
                                     return contribution.registrationId == id;
                                 });
    return it == state.contributions.end() ? nullptr : &*it;
}

bool IsStopped(const ProviderState& state, std::uint64_t id) {
    return std::find(state.stopped.begin(), state.stopped.end(), id) != state.stopped.end();
}

void ClearStopped(ProviderState& state, std::uint64_t id) {
    state.stopped.erase(std::remove(state.stopped.begin(), state.stopped.end(), id),
                        state.stopped.end());
}

}

bool ProviderState::IsActive(FameCategory category) const noexcept {
    if (!IsValidFameCategory(category)) {
        return false;
    }
    return activeCounts[static_cast<std::size_t>(category)] != 0;
}

std::size_t ProviderState::ActiveProviderCount(FameCategory category) const noexcept {
    if (!IsValidFameCategory(category)) {
        return 0;
    }
    return activeCounts[static_cast<std::size_t>(category)];
}

CategoryMask ProviderState::ActiveCategories() const noexcept {
    CategoryMask mask;
    for (std::size_t i = 0; i < kFameCategoryCount; ++i) {
        if (activeCounts[i] != 0) {
            mask.Set(static_cast<FameCategory>(i));
        }
    }
    return mask;
}

void ProviderState::RebuildActiveCounts() noexcept {
    activeCounts.fill(0);
    for (const auto& contribution : contributions) {
        contribution.categories.ForEach([&](FameCategory category) {
            ++activeCounts[static_cast<std::size_t>(category)];
        });
    }
}

ProviderTransition UpdateProvider(ProviderState& state, const ProviderCommand& command) {
    ProviderTransition transition;
    if (command.registrationId == 0 || !ValidPhase(command.phase)) {
        transition.status = Status::Invalid;
        return transition;
    }

    bool validCategories = true;
    auto requested = CategorySet(command.categories, validCategories);
    if (!validCategories) {
        transition.status = Status::Invalid;
        return transition;
    }

    // NotReady and Stale are transient adapter states; they leave saved registrations unchanged.
    if (command.phase == ProviderPhase::NotReady || command.phase == ProviderPhase::Stale) {
        transition.status = command.phase == ProviderPhase::NotReady ? Status::NotReady
                                                                    : Status::Stale;
        return transition;
    }

    if (command.phase == ProviderPhase::Enabled) {
        auto* contribution = FindContribution(state, command.registrationId);
        if (contribution == nullptr) {
            if (state.contributions.size() >= kMaxProviderRegistrations) {
                transition.status = Status::Invalid;
                return transition;
            }
            state.contributions.push_back(ProviderContribution{command.registrationId, {}});
            contribution = &state.contributions.back();
            transition.changed = true;
        }
        const auto added = requested & ~contribution->categories;
        added.ForEach([&](FameCategory category) {
            ++state.activeCounts[static_cast<std::size_t>(category)];
        });
        contribution->categories |= added;
        transition.changed = transition.changed || added.Any();
        ClearStopped(state, command.registrationId);
        transition.status = Status::Ready;
        transition.kind = transition.changed ? ProviderTransitionKind::Enabled
                                             : ProviderTransitionKind::None;
        return transition;
    }

    if (command.phase == ProviderPhase::Unregistered || IsInvalidation(command.phase)) {
        requested = *CategoryMask::FromBits(CategoryMask::kValidBits);
    }

    if (IsRemoval(command.phase)) {
        auto* contribution = FindContribution(state, command.registrationId);
        if (contribution == nullptr) {
            transition.status = Status::Ready;
            return transition;
        }

        if (IsInvalidation(command.phase) && IsStopped(state, command.registrationId)) {
            transition.status = Status::Ready;
            return transition;
        }

        CategoryMask wiped;
        const auto removed = requested & contribution->categories;
        removed.ForEach([&](FameCategory category) {
            const auto i = static_cast<std::size_t>(category);
            --state.activeCounts[i];
            if (state.activeCounts[i] == 0 && IsInvalidation(command.phase) &&
                IsOptionalCategory(category)) {
                wiped.Set(category);
            }
        });
        contribution->categories &= ~removed;
        transition.changed = removed.Any();

        if (contribution->categories.Empty()) {
            state.contributions.erase(state.contributions.begin() +
                                      (contribution - state.contributions.data()));
            transition.changed = true;
        }

        if (IsInvalidation(command.phase)) {
            state.stopped.push_back(command.registrationId);
            transition.kind = ProviderTransitionKind::Invalidated;
        } else if (command.phase == ProviderPhase::Unregistered) {
            state.stopped.push_back(command.registrationId);
            transition.kind = ProviderTransitionKind::Unregistered;
        } else {
            transition.kind = ProviderTransitionKind::Disabled;
        }

        transition.wipeCategories = SortedCategories(wiped);
        transition.status = Status::Ready;
        return transition;
    }

    transition.status = Status::Invalid;
    return transition;
}

}
