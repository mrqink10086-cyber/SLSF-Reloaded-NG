param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path,
    [string]$VsEnvironment = 'E:\BuildTools\VC\Auxiliary\Build\vcvars64.bat',
    [string]$OutputDirectory = (Join-Path $env:TEMP ('slsf-audit-replay-' + [guid]::NewGuid().ToString('N')))
)

$ErrorActionPreference = 'Stop'
$auditLibrary = Join-Path $RepositoryRoot 'build/windows/x64/releasedbg/SLSFCore.lib'
$auditInclude = Join-Path $RepositoryRoot 'include'
if (!(Test-Path -LiteralPath $auditLibrary)) { throw "Build SLSFCore first: $auditLibrary" }
if (!(Test-Path -LiteralPath $VsEnvironment)) { throw "Missing MSVC environment: $VsEnvironment" }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$auditNames = @('stable-key-probe', 'coroutine-abort-probe', 'capability-pending-probe', 'capability-reentry-probe', 'wait-retire-probe')
$auditBuild = @('@echo off', ('call "{0}" >nul' -f $VsEnvironment), 'if errorlevel 1 exit /b %errorlevel%')
foreach ($auditName in $auditNames) {
    $auditSource = Join-Path $PSScriptRoot ($auditName + '.cpp')
    $auditObject = Join-Path $OutputDirectory ($auditName + '.obj')
    $auditExe = Join-Path $OutputDirectory ($auditName + '.exe')
    $auditBuild += 'cl /nologo /std:c++latest /EHsc /MD /O2 /utf-8 /I "{0}" "{1}" /Fo"{2}" /Fe"{3}" /link "{4}"' -f $auditInclude, $auditSource, $auditObject, $auditExe, $auditLibrary
    $auditBuild += 'if errorlevel 1 exit /b %errorlevel%'
}
$auditBuildPath = Join-Path $OutputDirectory 'build-probes.cmd'
[IO.File]::WriteAllLines($auditBuildPath, $auditBuild, [Text.Encoding]::Default)
& $auditBuildPath
if ($LASTEXITCODE -ne 0) { throw "Probe compilation failed: $LASTEXITCODE" }

$auditResults = @()
foreach ($auditName in $auditNames) {
    $auditExe = Join-Path $OutputDirectory ($auditName + '.exe')
    $auditStdout = Join-Path $OutputDirectory ($auditName + '.stdout.log')
    $auditStderr = Join-Path $OutputDirectory ($auditName + '.stderr.log')
    $auditProcess = Start-Process -FilePath $auditExe -WindowStyle Hidden -PassThru -RedirectStandardOutput $auditStdout -RedirectStandardError $auditStderr
    $auditTimeout = if ($auditName -eq 'capability-reentry-probe') { 2000 } else { 30000 }
    $auditTimedOut = !$auditProcess.WaitForExit($auditTimeout)
    if ($auditTimedOut) { $auditProcess.Kill(); $auditProcess.WaitForExit() }
    $auditProcess.Refresh()
    $auditResults += [pscustomobject]@{ name = $auditName; timed_out = $auditTimedOut; exit_code = $auditProcess.ExitCode }
}
$auditResults | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDirectory 'results.json') -Encoding UTF8
$auditResults | Format-Table -AutoSize
Write-Output "PROBE_OUTPUT=$OutputDirectory"
