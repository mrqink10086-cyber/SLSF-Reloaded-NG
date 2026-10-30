#include "slsf/core/Contracts.h"

namespace slsf {

std::string_view BuildId() noexcept {
    return "SLSFCore/0.1.0";
}
static_assert(kLegacyInputEventCount == 52);
static_assert(kLegacyReplyEventCount == 7);
static_assert(kLegacyFunctionCount == 124);
static_assert(kLegacyCategoryCount == 25);
static_assert(kFameCategoryCount == 25);

}