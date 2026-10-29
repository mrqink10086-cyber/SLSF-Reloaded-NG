param(
    [switch]$MinimalPlugins,
    [string[]]$KeepPlugins = @(),
    [string]$Label = '',
    [int]$TimeoutSeconds = 120
)
$ErrorActionPreference = "Stop"
$root = "E:/st/steamapps/common/Skyrim Special Edition"
$pluginDir = "$root/Data/SKSE/Plugins"
$dst = "$pluginDir/SLSFReloadedNG.dll"
$src = "E:/source/repos/SLSF Reloaded NG/build/windows/x64/releasedbg/SLSFReloadedNG.dll"
$logDir = "$env:USERPROFILE/Documents/My Games/Skyrim Special Edition/SKSE"
$pluginLog = "$logDir/SLSF Reloaded NG.log"
$skseLog = "$logDir/skse64.log"
$outDir = "E:/source/repos/SLSF Reloaded NG/docs/evidence/task-groups/logs"
$suffix = if ($Label) { $Label } elseif ($MinimalPlugins) { "minimal" } else { "full" }
$out = "$outDir/2026-10-29-group06-game-load-$suffix-skse64.log"
$pluginOut = "$outDir/2026-10-29-group06-game-load-$suffix-plugin.log"

if (Test-Path $dst) { throw "refusing to overwrite existing $dst" }
$disabled = @()
$before = @(Get-Process SkyrimSE -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id)
try {
    if ($MinimalPlugins) {
        Get-ChildItem -LiteralPath $pluginDir -Filter *.dll | Where-Object { $_.Name -ne 'SLSFReloadedNG.dll' -and $_.Name -notin $KeepPlugins } | ForEach-Object {
            $target = "$($_.FullName).group06-disabled"
            if (Test-Path $target) { throw "stale disabled plugin $target" }
            Rename-Item -LiteralPath $_.FullName -NewName "$($_.Name).group06-disabled"
            $disabled += $target
        }
    }
    Copy-Item $src $dst
    # Never let a previous run's markers satisfy this run.
    Remove-Item $pluginLog -Force -ErrorAction SilentlyContinue
    Remove-Item $skseLog -Force -ErrorAction SilentlyContinue
    $loader = Start-Process -FilePath "$root/skse64_loader.exe" -WorkingDirectory $root `
        -PassThru
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $markers = $false
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 2
        if (Test-Path $pluginLog) {
            $text = Get-Content $pluginLog -Raw -ErrorAction SilentlyContinue
            if ($text -match "static table built") {
                $markers = $true
                break
            }
        }
        if ($loader.HasExited -and -not (Get-Process SkyrimSE -ErrorAction SilentlyContinue)) {
            break
        }
    }
    Start-Sleep -Seconds 2
    $new = @(Get-Process SkyrimSE -ErrorAction SilentlyContinue | Where-Object { $before -notcontains $_.Id })
    Write-Output "GAME_LOAD_MARKERS=$markers loader_exit=$($loader.HasExited) game_pids=$($new.Id -join ',') minimal=$MinimalPlugins"
    if (Test-Path $skseLog) { Copy-Item $skseLog $out -Force }
    if (Test-Path $pluginLog) { Copy-Item $pluginLog $pluginOut -Force }
    if ($new.Count -gt 0) { $new | Stop-Process -Force }
    if (-not $loader.HasExited) { $loader | Stop-Process -Force }
    if (-not $markers) { exit 1 }
}
finally {
    Start-Sleep -Seconds 2
    Remove-Item $dst -Force -ErrorAction SilentlyContinue
    foreach ($target in $disabled) {
        if (Test-Path $target) {
            Rename-Item -LiteralPath $target -NewName ([IO.Path]::GetFileName($target) -replace '\.group06-disabled$', '')
        }
    }
}
