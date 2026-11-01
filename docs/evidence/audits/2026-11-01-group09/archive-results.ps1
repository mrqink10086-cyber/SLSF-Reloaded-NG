$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$sourceRoot = Join-Path $repoRoot 'build/test-results'
$archiveRoot = Join-Path $PSScriptRoot 'test-results'
[void](New-Item -ItemType Directory -Path $archiveRoot -Force)
$names = @(
    'core.xml', 'core.xml.tests.xml', 'core-asan.xml', 'core-asan.xml.tests.xml',
    'boundary.xml', 'boundary.xml.tests.xml', 'boundary-asan.xml', 'boundary-asan.xml.tests.xml',
    'differential.xml', 'differential.xml.tests.xml',
    'appearance-probe.txt', 'bench.txt', 'dispatcher-offline-probe.txt'
)
$artifacts = foreach ($name in $names) {
    $source = Join-Path $sourceRoot $name
    $destination = Join-Path $archiveRoot $name
    Copy-Item -LiteralPath $source -Destination $destination
    $item = Get-Item -LiteralPath $source
    [pscustomobject]@{
        name = $name
        sourceModifiedUtc = $item.LastWriteTimeUtc.ToString('o')
        bytes = $item.Length
        sha256 = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
    }
}
$suites = foreach ($name in @('core', 'core-asan', 'boundary', 'boundary-asan', 'differential')) {
    [xml]$junit = [System.IO.File]::ReadAllText((Join-Path $archiveRoot ($name + '.xml')))
    [xml]$listing = [System.IO.File]::ReadAllText((Join-Path $archiveRoot ($name + '.xml.tests.xml')))
    $nodes = @($junit.testsuites.testsuite)
    [pscustomobject]@{
        name = $name
        cases = @($listing.MatchingTests.TestCase).Count
        assertions = ($nodes | ForEach-Object { [long]$_.tests } | Measure-Object -Sum).Sum
        failures = ($nodes | ForEach-Object { [long]$_.failures } | Measure-Object -Sum).Sum
        errors = ($nodes | ForEach-Object { [long]$_.errors } | Measure-Object -Sum).Sum
    }
}
$manifest = [pscustomobject]@{
    auditedHead = '5529ed98332ade1f8167f9cc2dac210ec0c393e5'
    baseline = '5062105aeec71df206c491d241892e847a4cfd60'
    run = 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\run-native-tests.ps1 -Mode all -Jobs 4 -Seed 1170'
    transcriptLimitation = 'fresh-native-all.log contains wrapper metadata, not native child stdout; temporary fuzz stdout was not preserved.'
    suites = @($suites)
    artifacts = @($artifacts)
}
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'test-artifact-manifest.json') -Encoding UTF8
$suites | ConvertTo-Json -Depth 4
