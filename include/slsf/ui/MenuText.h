#pragma once

#include "slsf/core/Types.h"

#include <string>
#include <string_view>

namespace slsf::ui {

std::string_view MenuStatusText(Status status) noexcept;
std::string MenuMessageText(std::string_view message);

}
