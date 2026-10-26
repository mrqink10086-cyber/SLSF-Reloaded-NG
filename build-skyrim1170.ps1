param(
    [switch]$ConfigureOnly
)

$ErrorActionPreference = 'Stop'
if (-not (Get-Command xmake -ErrorAction SilentlyContinue)) {
    throw 'Xmake 3.0.0 or newer is required. See README-Skyrim1170.md.'
}

# Keep the template's automatic install step inside this project.
$previousModsPath = $env:XSE_TES5_MODS_PATH
$previousGamePath = $env:XSE_TES5_GAME_PATH
Push-Location $PSScriptRoot
try {
    $env:XSE_TES5_MODS_PATH = Join-Path $PSScriptRoot 'build\staging'
    $env:XSE_TES5_GAME_PATH = $null

    $configArgs = @('f', '-y', '-p', 'windows', '-a', 'x64', '-m', 'releasedbg', '--toolchain=msvc', '--skyrim_se=n', '--skyrim_ae=y', '--skyrim_vr=n')
    $downloadsPath = Join-Path $PSScriptRoot 'build\downloads'
    if (Test-Path -LiteralPath $downloadsPath -PathType Container) {
        $configArgs += "--pkg_searchdirs=$downloadsPath"
    }
    & xmake @configArgs
    if ($LASTEXITCODE -ne 0) { throw "Xmake configuration failed (exit $LASTEXITCODE)." }

    if (-not $ConfigureOnly) {
        & xmake build -y SLSFReloadedNG
        if ($LASTEXITCODE -ne 0) { throw "Xmake build failed (exit $LASTEXITCODE)." }
    }
}
finally {
    $env:XSE_TES5_MODS_PATH = $previousModsPath
    $env:XSE_TES5_GAME_PATH = $previousGamePath
    Pop-Location
}
