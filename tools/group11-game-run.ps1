param(
    [int]$TimeoutSeconds = 200,
    [switch]$SkipInstall,
    [switch]$FullPackage,
    [switch]$MinimalPlugins
)
$ErrorActionPreference = "Stop"
$root = "E:/st/steamapps/common/Skyrim Special Edition"
$staging = "E:/source/repos/SLSF Reloaded NG/build/staging/SLSFReloadedNG"
$dllSource = "E:/source/repos/SLSF Reloaded NG/build/windows/x64/releasedbg/SLSFReloadedNG.dll"
$probePex = "E:/source/repos/SLSF Reloaded NG/build/papyrus/probe/SLSFR_FrozenProbe.pex"
$logDir = "$env:USERPROFILE/Documents/My Games/Skyrim Special Edition/SKSE"
$pluginLog = "$logDir/SLSF Reloaded NG.log"
$skseLog = "$logDir/skse64.log"
$scriptLog = "E:/st/steamapps/common/Skyrim Special Edition/Data/Papyrus/Logs/Script Profiler"
$outDir = "E:/source/repos/SLSF Reloaded NG/docs/evidence/task-groups/logs"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$stamp = Get-Date -Format "yyyy-MM-dd-HHmm"
$pluginOut = "$outDir/$stamp-group11-game-plugin.log"
$skseOut = "$outDir/$stamp-group11-game-skse64.log"
if ($MinimalPlugins) { $suffix = "minimal" } elseif ($FullPackage) { $suffix = "full" } else { $suffix = "std" }
$pluginOut = "$outDir/$stamp-group11-game-$suffix-plugin.log"
$skseOut = "$outDir/$stamp-group11-game-$suffix-skse64.log"

$disabled = @()
$pluginDir = "$root/Data/SKSE/Plugins"
$keepPlugins = @('SLSFReloadedNG.dll')

if (-not $SkipInstall) {
    $dst = "$pluginDir/SLSFReloadedNG.dll"
    if (Test-Path $dst) { Remove-Item $dst -Force }
    Copy-Item $dllSource $dst -Force

    Get-ChildItem "$staging/Scripts" -Filter *.pex | ForEach-Object {
        Copy-Item $_.FullName "$root/Data/Scripts/$($_.Name)" -Force
    }
    Copy-Item "$staging/SLSF Reloaded.esp" "$root/Data/SLSF Reloaded.esp" -Force
    if (-not (Test-Path -LiteralPath "$root/Data/SKSE/Plugins/SLSFReloadedNG.json")) {
        Copy-Item -LiteralPath "$staging/SKSE/Plugins/SLSFReloadedNG.json" -Destination "$root/Data/SKSE/Plugins/SLSFReloadedNG.json"
    }

    if (Test-Path $probePex) {
        Copy-Item $probePex "$root/Data/Scripts/SLSFR_FrozenProbe.pex" -Force
    } else {
        Write-Output "WARN probe pex missing: $probePex"
    }
    Write-Output "install: package deployed to $root/Data"

    if ($MinimalPlugins) {
        Get-ChildItem -LiteralPath $pluginDir -Filter *.dll | Where-Object { $_.Name -notin $keepPlugins } | ForEach-Object {
            $target = "$($_.FullName).group11-disabled"
            if (Test-Path $target) { throw "stale disabled plugin $target" }
            Rename-Item -LiteralPath $_.FullName -NewName "$($_.Name).group11-disabled"
            $disabled += $target
        }
        Write-Output "minimal: disabled $($disabled.Count) other plugin dlls"
    }
}

Remove-Item $pluginLog -Force -ErrorAction SilentlyContinue
Remove-Item $skseLog -Force -ErrorAction SilentlyContinue

$before = @(Get-Process SkyrimSE -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id)
$loader = Start-Process -FilePath "$root/skse64_loader.exe" -WorkingDirectory $root -PassThru -WindowStyle Hidden
$deadline = (Get-Date).AddSeconds($TimeoutSeconds)
$markers = $false
$probeCount = 0
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 3
    if (Test-Path $pluginLog) {
        $text = Get-Content $pluginLog -Raw -ErrorAction SilentlyContinue
        if ($text) {
            if ($text -match "compat: registered") { $markers = $true }
            $probeCount = ([regex]::Matches($text, "frozen probe")).Count
            if ($text -match "run all done") { break }
        }
    }
    if ($loader.HasExited -and -not (Get-Process SkyrimSE -ErrorAction SilentlyContinue)) { break }
}
Start-Sleep -Seconds 3

$new = @(Get-Process SkyrimSE -ErrorAction SilentlyContinue | Where-Object { $before -notcontains $_.Id })
Write-Output "GAME_LOAD_MARKERS=$markers probe_hits=$probeCount loader_exit=$($loader.HasExited) game_pids=$($new.Id -join ',')"
if (Test-Path $skseLog) { Copy-Item $skseLog $skseOut -Force }
if (Test-Path $pluginLog) { Copy-Item $pluginLog $pluginOut -Force }
Write-Output "plugin log: $pluginOut"
if ($new.Count -gt 0) { $new | Stop-Process -Force }
if (-not $loader.HasExited) { $loader | Stop-Process -Force }
Start-Sleep -Seconds 2
foreach ($target in $disabled) {
    if (Test-Path $target) {
        Rename-Item -LiteralPath $target -NewName ([IO.Path]::GetFileName($target) -replace '\.group11-disabled$', '')
    }
}
if (-not $markers) { exit 1 }
exit 0
