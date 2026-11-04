param(
    [string]$OutDir,
    [switch]$Clean,
    [switch]$TestOnly,
    [string[]]$DependencySource = @("../sexlab P+/Source/Scripts"),
    [string]$SkyrimRoot = "E:/st/steamapps/common/Skyrim Special Edition"
)
$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent
Set-Location $repoRoot
if (-not $OutDir) { $OutDir = if ($TestOnly) { "build/papyrus/test-out" } else { "build/papyrus/out" } }
$resolvedOutput = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $OutDir))
if (-not $resolvedOutput.StartsWith($repoRoot + [System.IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw "Output must stay within repository" }
if ($TestOnly -and $resolvedOutput -eq [System.IO.Path]::GetFullPath((Join-Path $repoRoot "build/papyrus/out"))) { throw "Test output must not use the production output directory" }
if ($Clean -and (Test-Path -LiteralPath $resolvedOutput)) { Remove-Item -LiteralPath $resolvedOutput -Recurse -Force }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$compiler = Join-Path $SkyrimRoot "Papyrus Compiler/PapyrusCompiler.exe"
$vanilla = Join-Path $SkyrimRoot "Data/Source/Scripts"
$flags = Join-Path $vanilla "TESV_Papyrus_Flags.flg"
$thin = "src/compat/papyrus"
if (-not (Test-Path $flags)) { throw "Papyrus flags file missing: $flags" }
if (-not (Test-Path $compiler)) { throw "Papyrus compiler missing: $compiler" }
$probes = "Scripts/Source"
$imports = @($thin, $probes, "tools/papyrus-imports")
if (-not $TestOnly) {
    foreach ($source in $DependencySource) {
        $resolvedSource = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $source))
        if (-not (Test-Path -LiteralPath $resolvedSource -PathType Container)) { throw "Dependency source directory missing: $resolvedSource" }
        $imports += $resolvedSource
    }
}
$imports += $vanilla
if (-not $TestOnly) {
    $missing = @("SexLabFramework", "SexLabThread", "SexLabUtil", "SexlabRegistry", "sslThreadModel", "sslThreadController", "PW_TrackerScript", "PW_MainLoopScript", "sr_InflateQuest", "SLSF_Configuration", "SLACS_main") | Where-Object {
        $dependencyName = $_ + ".psc"
        -not ($imports | Where-Object { Test-Path -LiteralPath (Join-Path $_ $dependencyName) -PathType Leaf })
    }
    if ($missing) { throw "Dependency sources missing: $($missing -join ', '). Supply -DependencySource." }
}
$manifestPath = Join-Path $resolvedOutput "compile-manifest.json"
if (Test-Path -LiteralPath $manifestPath) { Remove-Item -LiteralPath $manifestPath -Force }
$scripts = @((Get-ChildItem -LiteralPath $thin -Filter *.psc) + (Get-ChildItem -LiteralPath $probes -Filter *.psc) | Sort-Object Name)
$failed = @()
$compiled = 0
foreach ($script in $scripts) {
    $name = $script.Name
    & $compiler $script.FullName -f="$flags" -i="$($imports -join ';')" -o="$resolvedOutput"
    if ($LASTEXITCODE -ne 0) {
        Write-Output "FAIL $name exit=$LASTEXITCODE"
        $failed += $name
        continue
    }
    $pexPath = Join-Path $OutDir ($script.BaseName + ".pex")
    if (-not (Test-Path $pexPath)) {
        Write-Output "FAIL $name no output pex"
        $failed += $name
        continue
    }
    $compiled++
}
Write-Output "COMPILED=$compiled FAILED=$($failed.Count)"
if ($failed.Count -gt 0) { $failed | ForEach-Object { Write-Output "FAILED=$_" }; exit 1 }
@{ testOnly = [bool]$TestOnly; compiled = $compiled; imports = $imports; scripts = @($scripts.Name); interfaceImports = @(Get-ChildItem -LiteralPath "tools/papyrus-imports" -Filter *.psc | ForEach-Object Name) } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $manifestPath -Encoding utf8
exit 0
