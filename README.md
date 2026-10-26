# SLSF Reloaded NG

Version **0.1** (build metadata: **0.1.0**).

A C++ SKSE plugin project for SkyrimSE.exe 1.6.1170, based on
[CommonLibSSE-NG Plugin Template](https://github.com/libxse/commonlibsse-ng-template).
The build target and DLL basename are `SLSFReloadedNG`.

Template authorship and licensing are recorded in [ATTRIBUTION.md](ATTRIBUTION.md).

See [the local development guide](README-Skyrim1170.md) for configuration,
build output, and runtime requirements. Plugin functionality is currently the
original minimal SKSE initialization entry point.

### Requirements
* [XMake](https://xmake.io) [3.0.0+]
* C++23 Compiler (MSVC, Clang-CL)

## Getting Started
```powershell
Set-Location 'E:\source\repos\SLSF Reloaded NG'
.\build-skyrim1170.ps1 -ConfigureOnly
```

### Build
To build the project, run the following command:
```powershell
.\build-skyrim1170.ps1
```

> ***Note:*** *This will generate a `build/windows/` directory in the **project's root directory** with the build output.*


### Build Output (Optional)
If you want to redirect the build output, set one of the following environment variables:

- Path to a Mod Manager mods folder: `XSE_TES5_MODS_PATH`

  or

- Path to a Skyrim install folder: `XSE_TES5_GAME_PATH`

**Alternatively**, use the [set_installdir](https://xmake.io/api/description/project-target.html#set-installdir) api to set a specific install path instead, either globally or per target. By default, your plugin `.dll` and `.pdb` are included, but you can *add* more files to be installed by using the [add_installfiles](https://xmake.io/api/description/project-target.html#add-installfiles) api.

### Project Generation (Optional)
If you use Visual Studio, run the following command:
```bat
xmake project -k vsxmake
```

> ***Note:*** *This will generate a `vsxmakeXXXX/` directory in the **project's root directory** using the latest version of Visual Studio installed on the system.*

**Alternatively**, if you do not use Visual Studio, you can generate a `compile_commands.json` file for use with a laguage server like clangd in any code editor that supports it, like vscode:
```bat
xmake project -k compile_commands
```

> ***Note:*** *You must have a language server extension installed to make use of this file. I recommend `clangd`. Do not have more than one installed at a time as they will conflict with each other. I also recommend installing the `xmake` extension if available to make building the project easier.*

## Documentation
Please refer to the [upstream template Wiki](https://github.com/libxse/commonlibsse-ng-template/wiki) for more advanced topics.
