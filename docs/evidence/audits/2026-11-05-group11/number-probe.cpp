#include "slsf/io/JsonTransfer.h"
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    std::string error;
    const auto root = slsf::io::JsonValue::Parse("{\"int\":{\"modtotal\":1e20,\"mod.whore\":1e20},\"string\":{\"modindex[0]\":\"mod\"}}", 4096, &error);
    if (!root) { std::cout << "parse_error=" << error << '\n'; return 1; }
    const auto total = root->MemberAt("int")->MemberAt("modtotal");
    std::cout << "parsed_number=" << total->NumberAt(0) << '\n';
    std::cout << "integer=" << total->IntAt(7) << '\n';
    const auto imported = slsf::io::ExtractLegacyImport(root, std::vector<std::string>{"whore"});
    std::cout << "import_success=" << imported.has_value() << " mods=" << (imported ? imported->mods.size() : 0) << '\n';
}
