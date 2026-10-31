#include "slsf/integrations/ExternalState.h"

namespace slsf::integrations {

const char* ExternalAvailabilityName(ExternalAvailability value) noexcept {
    switch (value) {
    case ExternalAvailability::Unknown:
        return "Unknown";
    case ExternalAvailability::Available:
        return "Available";
    case ExternalAvailability::Missing:
        return "Missing";
    case ExternalAvailability::Incompatible:
        return "Incompatible";
    }
    return "Unknown";
}

const char* ExternalGateName(ExternalGate value) noexcept {
    switch (value) {
    case ExternalGate::Ready:
        return "Ready";
    case ExternalGate::Waiting:
        return "Waiting";
    case ExternalGate::Changed:
        return "Changed";
    case ExternalGate::Stale:
        return "Stale";
    case ExternalGate::Missing:
        return "Missing";
    case ExternalGate::Incompatible:
        return "Incompatible";
    case ExternalGate::Failed:
        return "Failed";
    case ExternalGate::Closed:
        return "Closed";
    }
    return "Unknown";
}

bool BlocksNewScoring(ExternalGate value) noexcept {
    return value != ExternalGate::Ready;
}

ExternalGate GateForAvailability(ExternalAvailability value) noexcept {
    switch (value) {
    case ExternalAvailability::Available:
        return ExternalGate::Ready;
    case ExternalAvailability::Missing:
        return ExternalGate::Missing;
    case ExternalAvailability::Incompatible:
        return ExternalGate::Incompatible;
    case ExternalAvailability::Unknown:
        return ExternalGate::Waiting;
    }
    return ExternalGate::Waiting;
}

FactState FactStateForAvailability(ExternalAvailability value) noexcept {
    switch (value) {
    case ExternalAvailability::Available:
        return FactState::Unknown;
    case ExternalAvailability::Missing:
        return FactState::Missing;
    case ExternalAvailability::Incompatible:
        return FactState::Incompatible;
    case ExternalAvailability::Unknown:
        return FactState::Unknown;
    }
    return FactState::Unknown;
}

}
