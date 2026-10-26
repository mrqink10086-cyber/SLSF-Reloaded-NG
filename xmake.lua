-- include subprojects
includes("lib/commonlibsse-ng")

-- Resolve library dependencies through Xmake instead of probing vcpkg/conan.
add_requireconfs("directxmath", { system = false })
add_requireconfs("directxtk", { system = false })
add_requireconfs("spdlog", { system = false })

-- set project constants
set_project("SLSF Reloaded NG")
set_version("0.1.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- define targets
target("SLSFReloadedNG")
    add_rules("commonlibsse-ng.plugin", {
        name = "SLSF Reloaded NG",
        description = "SLSF Reloaded NG SKSE plugin for Skyrim 1.6.1170"
    })

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
