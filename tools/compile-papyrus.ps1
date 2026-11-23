param(
    [string]$OutDir,
    [switch]$Clean,
    [switch]$TestOnly,
    [string[]]$DependencySource = @("../sexlab P+/Source/Scripts"),
    [string]$VrikSource,
    [switch]$AllowVrikStub,
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
$interfaceRoot = Join-Path $repoRoot "tools/papyrus-imports"
$fixtureRoot = Join-Path $repoRoot "tests/fixtures/papyrus"
$imports = @($thin, $probes)
$dependencyFiles = [ordered]@{}
$interfaceImports = @()
$manifestPath = Join-Path $resolvedOutput "compile-manifest.json"
if (Test-Path -LiteralPath $manifestPath) { Remove-Item -LiteralPath $manifestPath -Force }
if ($TestOnly) {
    $imports += @($interfaceRoot, $fixtureRoot, $vanilla)
    $interfaceImports = @(Get-ChildItem -LiteralPath $interfaceRoot -Filter *.psc | ForEach-Object Name)
} else {
    $compileOnlyRoots = @($interfaceRoot, $fixtureRoot) | ForEach-Object { [IO.Path]::GetFullPath($_).TrimEnd('\', '/') }
    $dependencyRoots = $DependencySource
    if ($VrikSource) { $dependencyRoots += $VrikSource }
    $sourceRoots = @($dependencyRoots | ForEach-Object {
        $candidate = if ([IO.Path]::IsPathRooted($_)) { $_ } else { Join-Path $repoRoot $_ }
        $absolute = [IO.Path]::GetFullPath($candidate).TrimEnd('\', '/')
        if (-not (Test-Path -LiteralPath $absolute -PathType Container)) { throw "Dependency source directory missing: $absolute" }
        if ($absolute -in $compileOnlyRoots) { throw "Compile-only interface directory cannot be listed as a production dependency: $absolute" }
        $absolute
    })
    $pPlusScripts = @("SexLabFramework", "SexLabThread", "SexLabUtil", "SexlabRegistry", "sslThreadModel", "sslThreadController")
    foreach ($name in ($pPlusScripts + @("sr_InflateQuest", "SLSF_Configuration", "SLACS_main", "StorageUtil", "PW_TrackerScript", "PW_MainLoopScript", "VRIK"))) {
        $fallbackRoots = @()
        if ($name.StartsWith("PW_")) { $fallbackRoots = @($vanilla, $interfaceRoot) }
        elseif ($name -notin $pPlusScripts) { $fallbackRoots = @($vanilla) }
        $file = @($sourceRoots + $fallbackRoots | ForEach-Object { Join-Path $_ "$name.psc" } | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1)
        if (-not $file -and $name -eq 'VRIK') {
            if (-not $AllowVrikStub) { throw 'Production dependency source missing: VRIK.psc. Supply -VrikSource with the original source directory or opt in with -AllowVrikStub.' }
            $file = @(Join-Path $fixtureRoot 'VRIK.psc')
        }
        if (-not $file) { throw "Production dependency source missing: $name.psc. Supply -DependencySource with the actual mod source directory." }
        if ((Split-Path $file[0] -Parent) -in $compileOnlyRoots) { $interfaceImports += "$name.psc" }
        $dependencyFiles["$name.psc"] = [ordered]@{ path = $file[0]; sha256 = (Get-FileHash -LiteralPath $file[0]).Hash.ToLowerInvariant() }
    }
    $imports += $sourceRoots + @($vanilla)
    if ($interfaceImports) {
        $selectedInterfaces = Join-Path $resolvedOutput "interfaces-$([Guid]::NewGuid().ToString('N'))"
        New-Item -ItemType Directory -Force -Path $selectedInterfaces | Out-Null
        foreach ($name in $interfaceImports) {
            Copy-Item -LiteralPath $dependencyFiles[$name].path -Destination (Join-Path $selectedInterfaces $name)
        }
        $imports += $selectedInterfaces
    }
}
$scripts = @((Get-ChildItem -LiteralPath $thin -Filter *.psc) + (Get-ChildItem -LiteralPath $probes -Filter *.psc) | Sort-Object Name)
$failed = @()
$compiledFiles = @()
foreach ($script in $scripts) {
    $name = $script.Name
    $sourceHash = (Get-FileHash -LiteralPath $script.FullName).Hash.ToLowerInvariant()
    $pexPath = Join-Path $resolvedOutput ($script.BaseName + ".pex")
    if (Test-Path -LiteralPath $pexPath) { Remove-Item -LiteralPath $pexPath -Force }
    & $compiler $script.FullName -f="$flags" -i="$($imports -join ';')" -o="$resolvedOutput"
    if ($LASTEXITCODE -ne 0) {
        Write-Output "FAIL $name exit=$LASTEXITCODE"
        $failed += $name
        continue
    }
    if (-not (Test-Path $pexPath)) {
        Write-Output "FAIL $name no output pex"
        $failed += $name
        continue
    }
    if ((Get-FileHash -LiteralPath $script.FullName).Hash.ToLowerInvariant() -ne $sourceHash) { throw "Papyrus source changed during compilation: $($script.FullName)" }
    $compiledFiles += [ordered]@{ source = $script.FullName.Substring($repoRoot.Length + 1).Replace('\', '/'); sourceSha256 = $sourceHash; pex = (Split-Path $pexPath -Leaf); pexSha256 = (Get-FileHash -LiteralPath $pexPath).Hash.ToLowerInvariant() }
}
Write-Output "COMPILED=$($compiledFiles.Count) FAILED=$($failed.Count)"
if ($failed.Count -gt 0) { $failed | ForEach-Object { Write-Output "FAILED=$_" }; exit 1 }
foreach ($dependency in $dependencyFiles.Values) {
    if ((Get-FileHash -LiteralPath $dependency.path).Hash.ToLowerInvariant() -cne $dependency.sha256) { throw "Papyrus dependency changed during compilation: $($dependency.path)" }
}
@{ testOnly = [bool]$TestOnly; compiled = $compiledFiles.Count; imports = $imports; scripts = @($scripts.Name); files = $compiledFiles; dependencySources = $dependencyFiles; interfaceImports = $interfaceImports } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding utf8
exit 0
