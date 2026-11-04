param(
    [Parameter(Mandatory = $true)][string]$DataDirectory,
    [string]$PackageDirectory = $PSScriptRoot
)
$ErrorActionPreference = "Stop"
$packageRoot = [IO.Path]::GetFullPath($PackageDirectory)
$dataRoot = [IO.Path]::GetFullPath($DataDirectory)
$packagePrefix = $packageRoot.TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
$dataPrefix = $dataRoot.TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
$manifest = Get-Content -LiteralPath (Join-Path $packageRoot "manifest.json") -Raw | ConvertFrom-Json
foreach ($file in $manifest.files) {
    $source = [IO.Path]::GetFullPath((Join-Path $packageRoot $file.path))
    if (-not $source.StartsWith($packagePrefix, [StringComparison]::OrdinalIgnoreCase)) { throw "Invalid package path: $($file.path)" }
    if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $file.sha256) { throw "Package hash mismatch: $($file.path)" }
}
$copied = 0
$preserved = 0
foreach ($file in $manifest.files) {
    if (-not $file.path.StartsWith("Data/")) { continue }
    $relative = $file.path.Substring(5)
    $destination = [IO.Path]::GetFullPath((Join-Path $dataRoot $relative))
    if (-not $destination.StartsWith($dataPrefix, [StringComparison]::OrdinalIgnoreCase)) { throw "Invalid destination: $relative" }
    if ($relative -eq "SKSE/Plugins/SLSFReloadedNG.json" -and (Test-Path -LiteralPath $destination)) { $preserved++; continue }
    New-Item -ItemType Directory -Force -Path (Split-Path $destination -Parent) | Out-Null
    Copy-Item -LiteralPath (Join-Path $packageRoot $file.path) -Destination $destination -Force
    $copied++
}
Write-Output "INSTALLED=$copied PRESERVED_CONFIG=$preserved DATA=$dataRoot"
