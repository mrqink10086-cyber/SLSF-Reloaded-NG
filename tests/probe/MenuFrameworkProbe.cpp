#include "slsf/ui/Menu.h"

#include <windows.h>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr wchar_t kFrameworkModule[] = L"SKSEMenuFramework";

const char* ExportNameAt(std::size_t index) {
    static const char* kExports[] = {
        "RegisterEventPriority",   "UnregisterEvent",           "AddWindow",
        "AddWindowWithView",       "GetMainWindow",             "AddSectionItem",
        "RenameSection",           "DeleteSection",             "SetHotkeyEnabled",
        "IsHotkeyEnabled",         "RegisterInpoutEvent",       "UnregisterInputEvent",
        "RegisterHudElement",      "UnregisterHudElement",      "IsAnyBlockingWindowOpened",
        "GetMenuFrameworkVersion", "GetMenuFrameworkAPIVersion", "LoadTexture",
        "DisposeTexture",          "PushFont",
    };
    return kExports[index];
}

std::size_t ExportNameCount() {
    return 20;
}

}

int main() {
    const HMODULE module = GetModuleHandleW(kFrameworkModule);
    if (module == nullptr) {
        std::cout << "module_loaded=0\n"
                  << "source_facts=menu framework 3.8f self-reported version, API version 1\n"
                  << "runtime_note=SKSEMenuFramework.dll is not loaded in this process; "
                     "probe validates the plugin-side guard path only\n";
        slsf::ui::Menu::ResetForTest();
        const bool installed = slsf::ui::Menu::Instance().Install({});
        std::cout << "install_without_module=" << (installed ? "1" : "0") << "\n";
        if (installed) {
            std::cout << "EXPECT_FAIL install must refuse without the module\n";
            return 99;
        }
        std::cout << "MENU_FRAMEWORK_PROBE=PASS\n";
        return 0;
    }

    std::cout << "module_loaded=1\n";
    std::size_t found = 0;
    for (std::size_t index = 0; index < ExportNameCount(); ++index) {
        const void* address = reinterpret_cast<const void*>(
            GetProcAddress(module, ExportNameAt(index)));
        std::cout << "export " << ExportNameAt(index) << '=' << (address != nullptr ? "1" : "0")
                  << '\n';
        if (address != nullptr) {
            ++found;
        }
    }
    std::cout << "exports_found=" << found << '/' << ExportNameCount() << '\n';

    bool probePass = found == ExportNameCount();
    if (const auto apiVersion = reinterpret_cast<std::uint32_t (*)()>(
            GetProcAddress(module, "GetMenuFrameworkAPIVersion"))) {
        const std::uint32_t version = apiVersion();
        std::cout << "api_version=" << version << " expected=1\n";
        probePass = probePass && version == 1;
    } else {
        probePass = false;
    }
    if (const auto menuVersion = reinterpret_cast<float (*)()>(
            GetProcAddress(module, "GetMenuFrameworkVersion"))) {
        std::cout << "menu_version=" << menuVersion() << " expected=3.8\n";
    }

    slsf::ui::Menu::ResetForTest();
    const bool installed = slsf::ui::Menu::Instance().Install({});
    std::cout << "install_with_module=" << (installed ? "1" : "0") << " expected=1\n";
    probePass = probePass && installed;
    slsf::ui::Menu::Instance().Unregister();
    std::cout << "unregister_done=1\n";
    std::cout << (probePass ? "MENU_FRAMEWORK_PROBE=PASS" : "MENU_FRAMEWORK_PROBE=FAIL")
              << '\n';
    return probePass ? 0 : 99;
}
