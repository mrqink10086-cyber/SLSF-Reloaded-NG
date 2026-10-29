#include "slsf/core/NumericRow.h"

#include <algorithm>

namespace slsf::numeric {

void ClearRow(NumericRow& row) noexcept {
    row.values.fill(0.0);
}

void CopyRow(NumericRow& destination, const NumericRow& source) noexcept {
    destination.values = source.values;
}

double MaxSlot(const NumericRow& row) noexcept {
    double maximum = row.values.front();
    for (std::size_t i = 1; i < kFameCategoryCount; ++i) {
        maximum = std::max(maximum, row.values[i]);
    }
    return maximum;
}

} // namespace slsf::numeric
