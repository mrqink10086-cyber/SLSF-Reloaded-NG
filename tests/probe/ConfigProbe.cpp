#include "slsf/config/ConfigurationStore.h"
#include "slsf/io/JsonTransfer.h"

#include <cstdio>
#include <memory>
#include <string>

int main() {
    const auto defaults = slsf::config::ConfigurationStore::LegacyDefaults();
    const auto encoded = slsf::config::ConfigurationStore::Encode(defaults);
    const std::string text = slsf::io::JsonValue::Write(*encoded, 2);
    std::fputs(text.c_str(), stdout);
    std::fputs("\n", stdout);
    return 0;
}
