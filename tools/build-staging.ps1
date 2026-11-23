param(
    [string]$StagingDir = "build/staging/SLSFReloadedNG",
    [string]$ArchiveName = "SLSF声望重制NG-0.1.5-中文测试版-MO2.zip",
    [string[]]$DependencySource = @("../sexlab P+/Source/Scripts"),
    [string]$VrikSource,
    [switch]$AllowVrikStub,
    [string]$SkyrimRoot = "E:/st/steamapps/common/Skyrim Special Edition"
)
$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $repoRoot
$resolvedStage = [IO.Path]::GetFullPath((Join-Path $repoRoot $StagingDir))
$stageRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot "build/staging"))
if (-not $resolvedStage.StartsWith($stageRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw "Staging must stay within build/staging" }
if ([IO.Path]::GetFileName($ArchiveName) -ne $ArchiveName -or [IO.Path]::GetExtension($ArchiveName) -ne '.zip') { throw "ArchiveName must be a .zip filename" }
$nativeSources = @('src', 'include', 'xmake.lua', 'tests/probe/ConfigProbe.cpp')
function Get-NativeSourceHashes {
    $paths = @(Get-ChildItem -LiteralPath $nativeSources -Recurse -Force -File | Sort-Object FullName)
    $paths | ForEach-Object {
        "$($_.FullName.Substring($repoRoot.Length + 1).Replace('\', '/')) $((Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant())"
    }
}
$sourceCommit = (& git rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Could not identify the source commit' }
$commonLibCommit = (& git -C lib/commonlibsse-ng rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or (& git -C lib/commonlibsse-ng status --porcelain)) { throw 'Packaging requires an unmodified CommonLib checkout' }
$nativeInputs = @(Get-NativeSourceHashes)
$ignoredNative = @(& git -c core.quotepath=false ls-files --others --ignored --exclude-standard -- $nativeSources)
if ($LASTEXITCODE -ne 0) { throw 'Could not identify ignored native inputs' }
$legacy = Join-Path (Split-Path $repoRoot -Parent) "SLSF Reloaded"
$esp = Join-Path $legacy "SLSF Reloaded.esp"
$espHash = (Get-FileHash -LiteralPath $esp).Hash.ToLowerInvariant()

$powerShell = (Get-Process -Id $PID).Path
& $powerShell -NoProfile -NonInteractive -File tools/verify-contracts.ps1 -EspPath $esp
if ($LASTEXITCODE -ne 0) { throw 'Legacy ESP contract verification failed' }

$papyrusDir = "build/papyrus/staging-$([Guid]::NewGuid().ToString('N'))"
$compileArgs = @{ OutDir = $papyrusDir; DependencySource = $DependencySource; VrikSource = $VrikSource; AllowVrikStub = $AllowVrikStub; SkyrimRoot = $SkyrimRoot }
& "$PSScriptRoot/compile-papyrus.ps1" @compileArgs
if ($LASTEXITCODE -ne 0) { throw 'Production Papyrus build failed' }
$compileManifest = Get-Content -LiteralPath "$papyrusDir/compile-manifest.json" -Raw | ConvertFrom-Json
if ($compileManifest.testOnly -ne $false -or $compileManifest.compiled -le 0) { throw 'Production Papyrus build required' }

& xmake f -y -p windows --toolchain=msvc --skyrim_vr=n -m releasedbg -a x64 -o build
if ($LASTEXITCODE -ne 0) { throw 'Native build configuration failed' }
& xmake build -y -r SLSFReloadedNG SLSFConfigProbe
if ($LASTEXITCODE -ne 0) { throw 'Native build failed' }
if ((& git rev-parse HEAD).Trim() -ne $sourceCommit -or ((Get-NativeSourceHashes) -join "`n") -cne ($nativeInputs -join "`n")) { throw 'Native sources changed while packaging; run staging again' }

$dll = Join-Path $repoRoot "build/windows/x64/releasedbg/SLSFReloadedNG.dll"
$defaultsTool = Join-Path $repoRoot "build/windows/x64/releasedbg/SLSFConfigProbe.exe"
foreach ($required in @($dll, $defaultsTool, $esp)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing package input: $required" }
}
if (Test-Path -LiteralPath $resolvedStage) { Remove-Item -LiteralPath $resolvedStage -Recurse -Force }
$documentPath = "Docs/SLSFReloadedNG"
foreach ($directory in @("SKSE/Plugins", "Scripts", "Source/Scripts", "$documentPath/许可证")) {
    New-Item -ItemType Directory -Force -Path (Join-Path $resolvedStage $directory) | Out-Null
}
$stagedEsp = Join-Path $resolvedStage "SLSF Reloaded.esp"
Copy-Item -LiteralPath $esp -Destination $stagedEsp
if ((Get-FileHash -LiteralPath $stagedEsp).Hash.ToLowerInvariant() -cne $espHash) { throw 'ESP changed while packaging' }
Copy-Item -LiteralPath (Join-Path $legacy "Interface") -Destination (Join-Path $resolvedStage "Interface") -Recurse
Copy-Item -LiteralPath $dll -Destination (Join-Path $resolvedStage "SKSE/Plugins/SLSFReloadedNG.dll")
foreach ($script in $compileManifest.files) {
    $pex = Join-Path $resolvedStage "Scripts/$($script.pex)"
    $source = Join-Path $resolvedStage "Source/Scripts/$(Split-Path $script.source -Leaf)"
    Copy-Item -LiteralPath "$papyrusDir/$($script.pex)" -Destination $pex
    Copy-Item -LiteralPath $script.source -Destination $source
    if ((Get-FileHash -LiteralPath $pex).Hash.ToLowerInvariant() -cne $script.pexSha256 -or (Get-FileHash -LiteralPath $source).Hash.ToLowerInvariant() -cne $script.sourceSha256) { throw "Papyrus input changed while packaging: $($script.source)" }
}
$defaults = & $defaultsTool
if ($LASTEXITCODE -ne 0) { throw "Default configuration generation failed" }
$null = ($defaults -join "`n") | ConvertFrom-Json
[IO.File]::WriteAllText((Join-Path $resolvedStage "SKSE/Plugins/SLSFReloadedNG.json"), ($defaults -join "`n"), [Text.UTF8Encoding]::new($false))
foreach ($name in @("LICENSE", "EXCEPTIONS", "ATTRIBUTION.md")) {
    Copy-Item -LiteralPath (Join-Path $repoRoot $name) -Destination (Join-Path $resolvedStage "$documentPath/许可证/$name")
}
foreach ($name in @("LICENSE", "COPYING")) {
    Copy-Item -LiteralPath (Join-Path $repoRoot "lib/commonlibsse-ng/$name") -Destination (Join-Path $resolvedStage "$documentPath/许可证/CommonLibSSE-NG-$name")
}
foreach ($pair in @(@("tools/install-package.ps1", "手动安装.ps1"), @("docs/distribution/安装说明.md", "安装说明.md"), @("docs/distribution/依赖与兼容说明.md", "依赖与兼容说明.md"), @("docs/distribution/鸣谢与来源.md", "鸣谢与来源.md"))) {
    Copy-Item -LiteralPath (Join-Path $repoRoot $pair[0]) -Destination (Join-Path $resolvedStage "$documentPath/$($pair[1])")
}
$files = @(Get-ChildItem -LiteralPath $resolvedStage -Recurse -File | Sort-Object FullName | ForEach-Object {
    [ordered]@{ path = $_.FullName.Substring($resolvedStage.Length + 1).Replace('\', '/'); bytes = $_.Length; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
})
$manifest = [ordered]@{
    package = "SLSFReloadedNG"; version = "0.1.5"; runtime = "1.6.1170"; skse = "2.2.6";
    sourceCommit = $sourceCommit; sourceDirty = [bool]($ignoredNative + @(& git status --porcelain)); commonLibCommit = $commonLibCommit;
    nativeInputs = $nativeInputs;
    build = "xmake mode.releasedbg windows x64 MSVC C++23"; layout = "MO2-Data-root";
    papyrus = $compileManifest; fileCount = $files.Count; files = $files
}
[IO.File]::WriteAllText((Join-Path $resolvedStage "$documentPath/文件清单.json"), ($manifest | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))
$files | ForEach-Object { "$($_.sha256)  $($_.path)" } | Set-Content -LiteralPath (Join-Path $resolvedStage "$documentPath/SHA256校验.txt") -Encoding UTF8
$archive = Join-Path $stageRoot $ArchiveName
$python = (Get-Command python -ErrorAction SilentlyContinue).Source
if (-not $python) { throw "python is required to build the archive with UTF-8 entry names" }
& $python "tools/make-zip.py" $resolvedStage $archive
if ($LASTEXITCODE -ne 0) { throw "Archive creation failed (exit $LASTEXITCODE)" }
Write-Output "STAGED_COUNT=$($files.Count) PEX=$($compileManifest.compiled) ARCHIVE=$archive"
Write-Output "ARCHIVE_SHA256=$((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant())"
