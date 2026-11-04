param(
    [string]$OutDir = "build/papyrus/out",
    [switch]$Clean
)
$ErrorActionPreference = "Stop"
Set-Location "E:/source/repos/SLSF Reloaded NG"
if ($Clean -and (Test-Path $OutDir)) { Remove-Item $OutDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$compiler = "E:/st/steamapps/common/Skyrim Special Edition/Papyrus Compiler/PapyrusCompiler.exe"
$flags = "E:/st/steamapps/common/Skyrim Special Edition/Data/Source/Scripts/TESV_Papyrus_Flags.flg"
$vanilla = "E:/st/steamapps/common/Skyrim Special Edition/Data/Source/Scripts"
$stubs = "build/papyrus/stubs"
$thin = "src/compat/papyrus"
if (-not (Test-Path $flags)) { throw "Papyrus flags file missing: $flags" }
if (-not (Test-Path $compiler)) { throw "Papyrus compiler missing: $compiler" }
$scripts = @(Get-ChildItem -LiteralPath $thin -Filter *.psc | Sort-Object Name)
$failed = @()
$compiled = 0
foreach ($script in $scripts) {
    $name = $script.Name
    & $compiler $script.FullName -f="$flags" -i="$stubs;$thin;$vanilla" -o="$OutDir" | Out-Null
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
if ($failed.Count -gt 0) { $failed | ForEach-Object { Write-Output "  readonlyFAILED=$_" } ; exit 1 }
exit 0
