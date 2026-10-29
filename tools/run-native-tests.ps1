[CmdletBinding()]
param(
    [Alias('Suite')]
    [ValidateSet('core', 'boundary', 'asan', 'differential', 'fuzz', 'bench', 'all')]
    [string]$Mode = 'all',
    [int]$Seed = 1170,
    [Alias('Runs')]
    [int]$FuzzIterations = 100000,
    [string]$Target = '',
    [ValidateRange(1, 1024)]
    [int]$Jobs = [Environment]::ProcessorCount
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$resultsRoot = Join-Path $repoRoot 'build/test-results'
$asanRuntime = 'C:/Program Files/LLVM/lib/clang/20/lib/windows'
New-Item -ItemType Directory -Force -Path $resultsRoot | Out-Null
Push-Location -LiteralPath $repoRoot

function Invoke-Xmake {
    param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Arguments)
    & xmake @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "xmake $($Arguments -join ' ') failed with exit code $LASTEXITCODE"
    }
}

function Invoke-XmakeCapture {
    param([string[]]$Arguments)
    $xmake = (Get-Command xmake).Source
    $stdout = [System.IO.Path]::GetTempFileName()
    $stderr = [System.IO.Path]::GetTempFileName()
    try {
        $process = Start-Process -FilePath $xmake -ArgumentList $Arguments -NoNewWindow -Wait -PassThru `
            -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        $text = ((Get-Content -LiteralPath $stdout -Raw -ErrorAction SilentlyContinue) + "`n" +
                 (Get-Content -LiteralPath $stderr -Raw -ErrorAction SilentlyContinue))
        return [pscustomobject]@{ ExitCode = $process.ExitCode; Text = $text }
    }
    finally {
        Remove-Item -LiteralPath $stdout, $stderr -Force -ErrorAction SilentlyContinue
    }
}

function Invoke-Catch2Target {
    param(
        [string]$Target,
        [string]$ResultName,
        [string]$TestSpec = ''
    )

    Invoke-Xmake build -y $Target
    $resolve = "import('core.project.config'); import('core.project.project'); config.load(); print(path.absolute(project.target('$Target'):targetfile()))"
    $executable = (& xmake lua -c $resolve | Select-Object -Last 1)
    if ($LASTEXITCODE -ne 0) {
        throw "could not resolve executable for $Target"
    }
    $executable = $executable.Trim()

    $junit = Join-Path $resultsRoot $ResultName
    $listing = "$junit.tests.xml"
    if ($TestSpec) {
        & $executable --list-tests --reporter xml --out $listing $TestSpec
    }
    else {
        & $executable --list-tests --reporter xml --out $listing
    }
    if ($LASTEXITCODE -ne 0) {
        throw "$Target test discovery failed with exit code $LASTEXITCODE"
    }
    $discovered = New-Object System.Xml.XmlDocument
    $discovered.Load($listing)
    $testCount = $discovered.SelectNodes('/MatchingTests/TestCase').Count
    if ($testCount -eq 0) {
        throw "$Target has no test cases"
    }
    $shardCount = [Math]::Min($Jobs, $testCount)
    $shardRoot = Join-Path $resultsRoot ([System.IO.Path]::GetFileNameWithoutExtension($ResultName) + '-shards')
    New-Item -ItemType Directory -Force -Path $shardRoot | Out-Null
    $shards = @()
    $timer = [Diagnostics.Stopwatch]::StartNew()
    Write-Host ("RUN {0} cases={1} jobs={2} seed={3}" -f $Target, $testCount, $shardCount, $Seed)
    try {
        for ($i = 0; $i -lt $shardCount; ++$i) {
            $prefix = Join-Path $shardRoot ("shard-{0:D2}" -f $i)
            $arguments = @('--rng-seed', $Seed, '--order', 'rand', '--shard-count', $shardCount, '--shard-index', $i,
                '--reporter', 'console', '--reporter', ('"junit::out={0}.xml"' -f $prefix),
                '--colour-mode', 'none')
            if ($TestSpec) {
                $arguments = @($TestSpec) + $arguments
            }
            $process = Start-Process -FilePath $executable -ArgumentList $arguments -WindowStyle Hidden `
                -WorkingDirectory $repoRoot -PassThru -RedirectStandardOutput "$prefix.log" -RedirectStandardError "$prefix.stderr.log"
            # Windows PowerShell needs the handle retained to read ExitCode after waiting.
            $null = $process.Handle
            $shards += [pscustomobject]@{ Process = $process; Prefix = $prefix; Index = $i }
        }
        foreach ($shard in $shards) {
            $shard.Process.WaitForExit()
        }
        $merged = New-Object System.Xml.XmlDocument
        $root = $merged.AppendChild($merged.CreateElement('testsuites'))
        $failed = @()
        foreach ($shard in $shards) {
            Write-Host ("SHARD {0}/{1}" -f ($shard.Index + 1), $shardCount)
            Write-Host ([System.IO.File]::ReadAllText("$($shard.Prefix).log").TrimEnd())
            $stderrText = [System.IO.File]::ReadAllText("$($shard.Prefix).stderr.log").TrimEnd()
            if ($stderrText) { Write-Host $stderrText }
            if ($shard.Process.ExitCode -ne 0) {
                $failed += "$($shard.Index):exit=$($shard.Process.ExitCode)"
            }
            $report = New-Object System.Xml.XmlDocument
            $report.Load("$($shard.Prefix).xml")
            foreach ($suite in $report.SelectNodes('/testsuites/testsuite')) {
                $root.AppendChild($merged.ImportNode($suite, $true)) | Out-Null
            }
        }
        $merged.Save($junit)
        Write-Host ("JUNIT {0}; elapsed={1:F3}s" -f $junit, $timer.Elapsed.TotalSeconds)
        if ($failed.Count -gt 0) {
            throw "$Target failed shards: $($failed -join ', '); logs: $shardRoot"
        }
    }
    finally {
        foreach ($shard in $shards) {
            if (-not $shard.Process.HasExited) { $shard.Process.Kill() }
            $shard.Process.Dispose()
        }
    }
}

function Invoke-Core {
    Invoke-Catch2Target -Target 'SLSFCoreTests' -ResultName 'core.xml'
}

function Invoke-Boundary {
    Invoke-Catch2Target -Target 'SLSFBoundaryTests' -ResultName 'boundary.xml'
}

function Invoke-Differential {
    Invoke-Catch2Target -Target 'SLSFCoreTests' -ResultName 'differential.xml' -TestSpec '[differential]'
}

function Invoke-Bench {
    Invoke-Xmake build -y SLSFBench
    $resolve = "import('core.project.config'); import('core.project.project'); config.load(); print(path.absolute(project.target('SLSFBench'):targetfile()))"
    $executable = (& xmake lua -c $resolve | Select-Object -Last 1)
    if ($LASTEXITCODE -ne 0) {
        throw 'could not resolve executable for SLSFBench'
    }
    $executable = $executable.Trim()
    $out = Join-Path $resultsRoot 'bench.txt'
    Write-Host "RUN SLSFBench -> $out"
    & $executable | Tee-Object -FilePath $out
    if ($LASTEXITCODE -ne 0) {
        throw "SLSFBench failed with exit code $LASTEXITCODE"
    }
    Write-Host ("BENCH report: {0}" -f $out)
}

function Invoke-OfflineWakeProbe {
    Invoke-Xmake build -y SLSFDispatcherOfflineProbe
    $resolve = "import('core.project.config'); import('core.project.project'); config.load(); print(path.absolute(project.target('SLSFDispatcherOfflineProbe'):targetfile()))"
    $executable = (& xmake lua -c $resolve | Select-Object -Last 1)
    if ($LASTEXITCODE -ne 0) {
        throw 'could not resolve executable for SLSFDispatcherOfflineProbe'
    }
    Write-Host 'RUN SLSFDispatcherOfflineProbe'
    & $executable.Trim() | Tee-Object -FilePath (Join-Path $resultsRoot 'dispatcher-offline-probe.txt')
    if ($LASTEXITCODE -ne 0) {
        throw "SLSFDispatcherOfflineProbe failed with exit code $LASTEXITCODE"
    }
}

function Invoke-Asan {
    if (-not (Test-Path -LiteralPath $asanRuntime)) {
        throw "ASan runtime directory not found: $asanRuntime"
    }

    Invoke-Xmake build -y SLSFCoreTestsAsan SLSFBoundaryTestsAsan SLSFAsanProbe
    $oldPath = $env:PATH
    try {
        $env:PATH = "$asanRuntime;$oldPath"

        Write-Host "RUN SLSFAsanProbe (index 7, expected exit 0)"
        $positive = Invoke-XmakeCapture @('run', 'SLSFAsanProbe')
        Write-Host $positive.Text.TrimEnd()
        if ($positive.ExitCode -ne 0 -or $positive.Text -notmatch 'probe=42 index=7') {
            throw "ASan positive probe failed (exit $($positive.ExitCode))"
        }

        Write-Host "RUN SLSFAsanProbe oob (index 8, expected heap-buffer-overflow)"
        $negative = Invoke-XmakeCapture @('run', 'SLSFAsanProbe', 'oob')
        Write-Host (($negative.Text -split "`r?`n" | Select-Object -First 8) -join "`n")
        if ($negative.ExitCode -eq 0) {
            throw "ASan negative probe unexpectedly exited 0"
        }
        if ($negative.Text -notmatch 'heap-buffer-overflow') {
            throw "ASan negative probe did not report heap-buffer-overflow"
        }

        Invoke-Catch2Target -Target 'SLSFCoreTestsAsan' -ResultName 'core-asan.xml'
        Invoke-Catch2Target -Target 'SLSFBoundaryTestsAsan' -ResultName 'boundary-asan.xml'
    }
    finally {
        $env:PATH = $oldPath
    }
}

function Invoke-Fuzz {
    $fuzzTarget = if ($Target) { $Target } else { 'StateCodecFuzz' }
    if ($fuzzTarget -notin @('StateCodecFuzz', 'SerializationFuzz')) {
        throw "unsupported fuzz target '$fuzzTarget'"
    }
    Invoke-Xmake build -y $fuzzTarget
    $corpus = Join-Path $repoRoot "build/fuzz/$fuzzTarget-corpus"
    New-Item -ItemType Directory -Force -Path $corpus | Out-Null
    $artifacts = Join-Path $repoRoot "build/fuzz/$fuzzTarget-artifacts"
    New-Item -ItemType Directory -Force -Path $artifacts | Out-Null
    $fixtureDir = if ($fuzzTarget -eq 'StateCodecFuzz') { 'v1' } else { 'envelope' }
    Copy-Item -Path (Join-Path $repoRoot "tests/fixtures/serialization/$fixtureDir/*.bin") -Destination $corpus -Force
    $fuzzer = Join-Path $repoRoot "build/fuzz/$fuzzTarget.exe"
    Write-Host ("RUN {0} -runs={1} -seed={2}" -f $fuzzTarget, $FuzzIterations, $Seed)
    $oldPath = $env:PATH
    $stdout = [System.IO.Path]::GetTempFileName()
    $stderr = [System.IO.Path]::GetTempFileName()
    try {
        $env:PATH = "$asanRuntime;$oldPath"
        # libFuzzer writes its banner to stderr; Windows PowerShell 5.1 turns
        # native stderr into terminating errors under ErrorActionPreference=Stop.
        $process = Start-Process -FilePath $fuzzer -NoNewWindow -Wait -PassThru `
            -ArgumentList @(
                ("-runs={0}" -f $FuzzIterations),
                ("-seed={0}" -f $Seed),
                '-max_len=65536',
                ("`"-artifact_prefix={0}/`"" -f $artifacts),
                ("`"{0}`"" -f $corpus)
            ) -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        $outText = Get-Content -LiteralPath $stdout -Raw -ErrorAction SilentlyContinue
        $errText = Get-Content -LiteralPath $stderr -Raw -ErrorAction SilentlyContinue
        if ($outText) { Write-Host $outText.TrimEnd() }
        if ($errText) { Write-Host $errText.TrimEnd() }
        if ($process.ExitCode -ne 0) {
            throw "$fuzzTarget failed with exit code $($process.ExitCode) (seed $Seed)"
        }
    }
    finally {
        $env:PATH = $oldPath
        Remove-Item -LiteralPath $stdout, $stderr -Force -ErrorAction SilentlyContinue
    }
}

function Show-SkippedTarget {
    param([string]$Name, [string]$Reason)
    Write-Host ("SKIP {0}: {1}" -f $Name, $Reason)
    $script:skipped.Add($Name) | Out-Null
}

$script:skipped = New-Object System.Collections.Generic.List[string]

try {
    switch ($Mode) {
        'core' { Invoke-Core }
        'boundary' { Invoke-Boundary }
        'asan' { Invoke-Asan }
        'differential' {
            Invoke-Differential
        }
        'fuzz' {
            Invoke-Fuzz
        }
        'bench' {
            Invoke-Bench
        }
        'all' {
            Invoke-Core
            Invoke-Boundary
            Invoke-Asan
            Invoke-Fuzz
            Invoke-Differential
            Invoke-Bench
            Invoke-OfflineWakeProbe
        }
    }
    if ($script:skipped.Count -gt 0) {
        Write-Host ("PARTIAL native tests: mode={0} seed={1}; skipped={2}" -f $Mode, $Seed, ($script:skipped -join ","))
        exit 2
    }
    Write-Host ("PASS native tests: mode={0} seed={1}" -f $Mode, $Seed)
    exit 0
}
catch {
    Write-Error $_.Exception.Message
    exit 1
}
finally {
    Pop-Location
}
