local fixture_dir = os.projectdir():gsub("\\", "/") .. "/tests/fixtures"

target("SLSFCoreTests")
    set_kind("binary")
    add_files("**.cpp")
    add_files("../reference/*.cpp")
    add_includedirs("../reference")
    add_defines('SLSF_FIXTURE_DIR="' .. fixture_dir .. '"')
    add_deps("SLSFCore", "SLSFEngine")
    add_packages("catch2", { components = { "main", "lib" } })
    add_cxflags("/utf-8")