-- Boundary tests exercise only the public ports; no game headers are included.
target("SLSFBoundaryTests")
    set_kind("binary")
    add_files("**.cpp")
    add_includedirs("$(projectdir)/include")
    add_packages("catch2", { components = { "main", "lib" } })
