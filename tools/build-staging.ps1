param(
    [string]$StagingDir = "build/staging/SLSFReloadedNG"
)
$ErrorActionPreference = "Stop"
Set-Location "E:/source/repos/SLSF Reloaded NG"

if (Test-Path $StagingDir) { Remove-Item $StagingDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $StagingDir | Out-Null
New-Item -ItemType Directory -Force -Path "$StagingDir/SKSE/Plugins" | Out-Null
New-Item -ItemType Directory -Force -Path "$StagingDir/Scripts" | Out-Null
New-Item -ItemType Directory -Force -Path "$StagingDir/Source/scripts" | Out-Null

$legacy = "E:/source/repos/SLSF Reloaded"

Copy-Item "$legacy/SLSF Reloaded.esp" "$StagingDir/SLSF Reloaded.esp" -Force
Copy-Item "build/windows/x64/releasedbg/SLSFReloadedNG.dll" "$StagingDir/SKSE/Plugins/SLSFReloadedNG.dll" -Force
if (Test-Path "build/windows/x64/releasedbg/SLSFReloadedNG.pdb") {
    Copy-Item "build/windows/x64/releasedbg/SLSFReloadedNG.pdb" "$StagingDir/SKSE/Plugins/SLSFReloadedNG.pdb" -Force
}

Get-ChildItem "build/papyrus/out" -Filter *.pex | ForEach-Object {
    Copy-Item $_.FullName "$StagingDir/Scripts/$($_.Name)" -Force
}

Copy-Item "$legacy/Scripts/SLSF_Reloaded_MCM.pex" "$StagingDir/Scripts/SLSF_Reloaded_MCM.pex" -Force
Copy-Item "$legacy/Scripts/SLSF_Reloaded_NPCScan.pex" "$StagingDir/Scripts/SLSF_Reloaded_NPCScan.pex" -Force

Get-ChildItem "src/compat/papyrus" -Filter *.psc | ForEach-Object {
    Copy-Item $_.FullName "$StagingDir/Source/scripts/$($_.Name)" -Force
}

Write-Output "STAGED_COUNT=$((Get-ChildItem $StagingDir -Recurse -File).Count)"
