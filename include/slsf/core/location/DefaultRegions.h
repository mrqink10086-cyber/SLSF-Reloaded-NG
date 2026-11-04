#pragma once

#include "slsf/core/Types.h"
#include <array>
#include <string_view>

namespace slsf {
struct DefaultRegion {
    std::string_view name;
    std::string_view plugin;
    std::uint32_t form;
};
inline constexpr std::array<DefaultRegion, 21> kDefaultRegions{{
    {"Whiterun", "Skyrim.esm", 0x16772}, {"Winterhold", "Skyrim.esm", 0x1676B},
    {"Windhelm", "Skyrim.esm", 0x1676A}, {"Solitude", "Skyrim.esm", 0x16770},
    {"Riften", "Skyrim.esm", 0x1676C}, {"Markarth", "Skyrim.esm", 0x16769},
    {"Morthal", "Skyrim.esm", 0x1676E}, {"Dawnstar", "Skyrim.esm", 0x1676D},
    {"Falkreath", "Skyrim.esm", 0x1676F}, {"Raven Rock", "Dragonborn.esm", 0x143B9},
    {"Riverwood", "Skyrim.esm", 0x13163}, {"Rorikstead", "Skyrim.esm", 0x18A47},
    {"Ivarstead", "Skyrim.esm", 0x18A4B}, {"Shor's Stone", "Skyrim.esm", 0x18A4C},
    {"Dragon Bridge", "Skyrim.esm", 0x18A46}, {"Karthwasten", "Skyrim.esm", 0x18A54},
    {"Largashbur", "Skyrim.esm", 0x19263}, {"Dushnikh Yal", "Skyrim.esm", 0x19171},
    {"Mor Khazgur", "Skyrim.esm", 0x1927C}, {"Narzulbur", "Skyrim.esm", 0x19282},
    {"Skaal Village", "Dragonborn.esm", 0x143BB}
}};
}
