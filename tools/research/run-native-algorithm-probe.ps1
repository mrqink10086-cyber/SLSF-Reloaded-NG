param(
    [ValidateSet('native-algorithm', 'permission-mask')]
    [string]$Probe = 'native-algorithm'
)
$ErrorActionPreference = 'Stop'
$repoPath = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$probeOut = Join-Path $repoPath "build/research/$Probe"
New-Item -ItemType Directory -Path $probeOut -Force | Out-Null
$vsWherePath = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vsInstallPath = & $vsWherePath -products '*' -latest -property installationPath
$vcVarsPath = Join-Path $vsInstallPath 'VC/Auxiliary/Build/vcvars64.bat'
$exePath = Join-Path $probeOut 'probe.exe'
$buildLogPath = Join-Path $probeOut 'build.log'
$resultPath = Join-Path $probeOut 'results.csv'
$batchPath = Join-Path $probeOut 'build.cmd'
$sourcePath = Join-Path $PSScriptRoot "$Probe-probe.cpp"
$locationPath = Join-Path $repoPath 'src/core/location/LocationIndex.cpp'
$reputationPath = Join-Path $repoPath 'src/core/reputation/ReputationState.cpp'
$providerPath = Join-Path $repoPath 'src/core/reputation/ProviderRegistry.cpp'
$includePath = Join-Path $repoPath 'include'
$batchText = @"
@echo off
call "$vcVarsPath" > nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++latest /O2 /EHsc /fp:precise /arch:AVX2 /W4 /FAs /I"$includePath" "$sourcePath" "$locationPath" "$reputationPath" "$providerPath" /Fe:"$exePath"
"@
[IO.File]::WriteAllText($batchPath, $batchText, [Text.Encoding]::ASCII)
Push-Location $probeOut
try {
    & cmd.exe /d /c $batchPath 2>&1 | Tee-Object -FilePath $buildLogPath
    if ($LASTEXITCODE -ne 0) { throw "Probe build failed: $LASTEXITCODE" }
    $probeProcess = [Diagnostics.Process]::GetCurrentProcess()
    $originalAffinity = $probeProcess.ProcessorAffinity
    try {
        $probeProcess.ProcessorAffinity = [IntPtr]1
        'affinity,logical-processor-0' | Tee-Object -FilePath $resultPath
        & $exePath | Tee-Object -FilePath $resultPath -Append
        if ($LASTEXITCODE -ne 0) { throw "Probe failed: $LASTEXITCODE" }
    } finally { $probeProcess.ProcessorAffinity = $originalAffinity }
} finally { Pop-Location }
