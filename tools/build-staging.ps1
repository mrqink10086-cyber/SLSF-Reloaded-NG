param(
    [string]$StagingDir = "build/staging/SLSFReloadedNG"
)
$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $repoRoot
$resolvedStage = [IO.Path]::GetFullPath((Join-Path $repoRoot $StagingDir))
$stageRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot "build/staging"))
if (-not $resolvedStage.StartsWith($stageRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw "Staging must stay within build/staging" }
$compileManifest = Get-Content -LiteralPath "build/papyrus/out/compile-manifest.json" -Raw | ConvertFrom-Json
if ($compileManifest.testOnly -ne $false -or $compileManifest.compiled -le 0) { throw "Production Papyrus build required" }
$legacy = Join-Path (Split-Path $repoRoot -Parent) "SLSF Reloaded"
$dll = Join-Path $repoRoot "build/windows/x64/releasedbg/SLSFReloadedNG.dll"
$defaultsTool = Join-Path $repoRoot "build/windows/x64/releasedbg/SLSFConfigProbe.exe"
foreach ($required in @($dll, $defaultsTool, (Join-Path $legacy "SLSF Reloaded.esp"))) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing package input: $required" }
}
if (Test-Path -LiteralPath $resolvedStage) { Remove-Item -LiteralPath $resolvedStage -Recurse -Force }
foreach ($directory in @("Data/SKSE/Plugins", "Data/Scripts", "Data/Source/Scripts", "licenses")) {
    New-Item -ItemType Directory -Force -Path (Join-Path $resolvedStage $directory) | Out-Null
}
Copy-Item -LiteralPath (Join-Path $legacy "SLSF Reloaded.esp") -Destination (Join-Path $resolvedStage "Data/SLSF Reloaded.esp")
Copy-Item -LiteralPath (Join-Path $legacy "Interface") -Destination (Join-Path $resolvedStage "Data/Interface") -Recurse
Copy-Item -LiteralPath $dll -Destination (Join-Path $resolvedStage "Data/SKSE/Plugins/SLSFReloadedNG.dll")
foreach ($script in $compileManifest.scripts) {
    $pex = [IO.Path]::ChangeExtension($script, ".pex")
    Copy-Item -LiteralPath (Join-Path $repoRoot "build/papyrus/out/$pex") -Destination (Join-Path $resolvedStage "Data/Scripts/$pex")
    $source = Join-Path $repoRoot "src/compat/papyrus/$script"
    if (-not (Test-Path -LiteralPath $source)) { $source = Join-Path $repoRoot "Scripts/Source/$script" }
    Copy-Item -LiteralPath $source -Destination (Join-Path $resolvedStage "Data/Source/Scripts/$script")
}
$defaults = & $defaultsTool
if ($LASTEXITCODE -ne 0) { throw "Default configuration generation failed" }
$null = ($defaults -join "`n") | ConvertFrom-Json
[IO.File]::WriteAllText((Join-Path $resolvedStage "Data/SKSE/Plugins/SLSFReloadedNG.json"), ($defaults -join "`n"), [Text.UTF8Encoding]::new($false))
foreach ($name in @("LICENSE", "EXCEPTIONS", "ATTRIBUTION.md")) {
    Copy-Item -LiteralPath (Join-Path $repoRoot $name) -Destination (Join-Path $resolvedStage "licenses/$name")
}
foreach ($name in @("LICENSE", "COPYING")) {
    Copy-Item -LiteralPath (Join-Path $repoRoot "lib/commonlibsse-ng/$name") -Destination (Join-Path $resolvedStage "licenses/CommonLibSSE-NG-$name")
}
foreach ($pair in @(@("tools/install-package.ps1", "Install.ps1"), @("docs/distribution/INSTALL.md", "INSTALL.md"), @("docs/distribution/DEPENDENCIES.md", "DEPENDENCIES.md"))) {
    Copy-Item -LiteralPath (Join-Path $repoRoot $pair[0]) -Destination (Join-Path $resolvedStage $pair[1])
}
$files = @(Get-ChildItem -LiteralPath $resolvedStage -Recurse -File | Sort-Object FullName | ForEach-Object {
    [ordered]@{ path = $_.FullName.Substring($resolvedStage.Length + 1).Replace('\', '/'); bytes = $_.Length; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
})
$sourceCommit = (& git rev-parse HEAD).Trim()
$commonLibCommit = (& git -C lib/commonlibsse-ng rev-parse HEAD).Trim()
$manifest = [ordered]@{
    package = "SLSFReloadedNG"; version = "0.1.0"; runtime = "1.6.1170"; skse = "2.2.6";
    sourceCommit = $sourceCommit; sourceDirty = [bool](& git status --porcelain); commonLibCommit = $commonLibCommit;
    build = "xmake mode.releasedbg windows x64 MSVC C++23";
    papyrus = $compileManifest; fileCount = $files.Count; files = $files
}
[IO.File]::WriteAllText((Join-Path $resolvedStage "manifest.json"), ($manifest | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))
$files | ForEach-Object { "$($_.sha256)  $($_.path)" } | Set-Content -LiteralPath (Join-Path $resolvedStage "SHA256SUMS.txt") -Encoding UTF8
$archive = Join-Path $stageRoot "SLSFReloadedNG-0.1.0-game-test.zip"
Compress-Archive -LiteralPath (Join-Path $resolvedStage "Data"), (Join-Path $resolvedStage "licenses"), (Join-Path $resolvedStage "Install.ps1"), (Join-Path $resolvedStage "INSTALL.md"), (Join-Path $resolvedStage "DEPENDENCIES.md"), (Join-Path $resolvedStage "manifest.json"), (Join-Path $resolvedStage "SHA256SUMS.txt") -DestinationPath $archive -Force
Write-Output "STAGED_COUNT=$($files.Count) PEX=$($compileManifest.compiled) ARCHIVE=$archive"
Write-Output "ARCHIVE_SHA256=$((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant())"
