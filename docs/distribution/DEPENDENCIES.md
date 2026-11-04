# Dependencies and bundled assets

Required game runtime: Skyrim Special Edition 1.6.1170 and SKSE 2.2.6 (x64),
Address Library for that runtime, SKSE Menu Framework, SkyUI, SexLab P+, PapyrusUtil
(StorageUtil and ModEvent), and Devious Devices NG with API version 2. Install
these independently; their DLLs and scripts are not redistributed here.
SkyUI supplies the retained `SKI_ConfigBase` type; settings use the SKSE menu.

The MSVC build requires the Microsoft Visual C++ x64 runtime used by the local
VS 2022 toolchain. `manifest.json` records the CommonLibSSE-NG submodule revision.
CommonLib is linked into the NG DLL; its license and copying terms are included
alongside the project's GPL-3.0 license, template exception and attribution.

Optional appearance sources: RaceMenu/NiOverride, JContainers and SlaveTats.
Optional business providers: Fill Her Up, Public Whore, SLACS, supported pregnancy
mods, Advanced Nudity Detection, SLSF Fame Comments and Bimbos of Skyrim.
Dependent rules wait for unknown facts instead of treating missing VM responses
as known zero. Installing an optional mod requires that mod's own dependencies.

The SLSF Reloaded ESP and Interface files are preserved from the local original
SLSF Reloaded asset directory; this package does not claim their authorship or
change their records. Other external mod sources serve as read-only references.
Public Whore compiler signatures follow the frozen SLSF call sites because the
original source is unavailable locally; the compiler declarations do not prove
that every installed version exposes the same runtime interface.
