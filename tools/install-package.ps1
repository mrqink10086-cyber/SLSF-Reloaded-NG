param(
    [Parameter(Mandatory = $true)][string]$DataDirectory,
    [string]$PackageDirectory
)
$ErrorActionPreference = "Stop"
if (-not $PackageDirectory) { $PackageDirectory = Join-Path $PSScriptRoot "../.." }
$packageRoot = [IO.Path]::GetFullPath($PackageDirectory)
$dataRoot = [IO.Path]::GetFullPath($DataDirectory)
$packagePrefix = $packageRoot.TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
$dataPrefix = $dataRoot.TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
$manifest = Get-Content -LiteralPath (Join-Path $packageRoot "Docs/SLSFReloadedNG/文件清单.json") -Raw -Encoding UTF8 | ConvertFrom-Json
foreach ($file in $manifest.files) {
    $source = [IO.Path]::GetFullPath((Join-Path $packageRoot $file.path))
    if (-not $source.StartsWith($packagePrefix, [StringComparison]::OrdinalIgnoreCase)) { throw "安装包路径无效：$($file.path)" }
    if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $file.sha256) { throw "文件校验失败：$($file.path)" }
}
$copied = 0
$preserved = 0
foreach ($file in $manifest.files) {
    if ($file.path.StartsWith("Docs/")) { continue }
    $relative = $file.path
    $destination = [IO.Path]::GetFullPath((Join-Path $dataRoot $relative))
    if (-not $destination.StartsWith($dataPrefix, [StringComparison]::OrdinalIgnoreCase)) { throw "安装目标路径无效：$relative" }
    if ($relative -eq "SKSE/Plugins/SLSFReloadedNG.json" -and (Test-Path -LiteralPath $destination)) { $preserved++; continue }
    New-Item -ItemType Directory -Force -Path (Split-Path $destination -Parent) | Out-Null
    Copy-Item -LiteralPath (Join-Path $packageRoot $file.path) -Destination $destination -Force
    $copied++
}
Write-Output "INSTALLED=$copied PRESERVED_CONFIG=$preserved DATA=$dataRoot"
