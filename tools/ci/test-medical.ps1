<#
.SYNOPSIS
Build and boot isolated local DayZ servers for all medical configuration cases.
.DESCRIPTION
Requires a local DayZ dedicated-server installation. Each case gets its own
mission, profile, config, logs, and JSON result below ignored build/medical-tests.
Only the process started for the current case is stopped. No server-installation
files or existing profiles are changed. Cases run sequentially.
.EXAMPLE
.\tools\ci\test-medical.ps1
.EXAMPLE
.\tools\ci\test-medical.ps1 -SkipBuild -TimeoutSeconds 180
#>
[CmdletBinding()]
param(
    [string]$ServerRoot = "",
    [switch]$SkipBuild,
    [switch]$SelfTest,
    [ValidateRange(10, 3600)]
    [int]$TimeoutSeconds = 120,
    [ValidateRange(1024, 65480)]
    [int]$BasePort = 28725
)

$ErrorActionPreference = "Stop"

# Expected values are independent of the implementation's accessors/clamps.
$Cases = @(
    @{ Name = "missing-fields"; Config = @{}; Expected = @{ Mode = "bandages"; IntervalSeconds = 5.0; HealthPerTick = 5.0 } },
    @{ Name = "regen-default"; Config = @{ MedicalMode = "regen" }; Expected = @{ Mode = "regen"; IntervalSeconds = 5.0; HealthPerTick = 5.0 } },
    @{ Name = "regen-tuned"; Config = @{ MedicalMode = "regen"; RegenIntervalSeconds = 2.0; RegenHealthPerTick = 7.0 }; Expected = @{ Mode = "regen"; IntervalSeconds = 2.0; HealthPerTick = 7.0 } },
    @{ Name = "unknown-mode"; Config = @{ MedicalMode = "unknown-medical-mode" }; Expected = @{ Mode = "bandages"; IntervalSeconds = 5.0; HealthPerTick = 5.0 } },
    @{ Name = "low-interval-high-hp"; Config = @{ MedicalMode = "regen"; RegenIntervalSeconds = 0.1; RegenHealthPerTick = 200.0 }; Expected = @{ Mode = "regen"; IntervalSeconds = 0.5; HealthPerTick = 100.0 } },
    @{ Name = "high-interval-low-hp"; Config = @{ MedicalMode = "regen"; RegenIntervalSeconds = 4000.0; RegenHealthPerTick = 0.0 }; Expected = @{ Mode = "regen"; IntervalSeconds = 3600.0; HealthPerTick = 0.1 } }
)

function Write-TestJson {
    param([object]$Value, [string]$Path)
    $Value | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $Path -Encoding ASCII
}

function Assert-PortsAvailable {
    param([int]$Port)
    $Sockets = @()
    try {
        # Game, adjacent engine port, Steam query, and legacy Steam transport.
        foreach ($Candidate in @($Port, ($Port + 1), ($Port + 2), ($Port + 3))) {
            $Socket = New-Object System.Net.Sockets.UdpClient
            $Sockets += $Socket
            $Socket.Client.ExclusiveAddressUse = $true
            $Socket.Client.Bind((New-Object System.Net.IPEndPoint ([System.Net.IPAddress]::Any, $Candidate)))
        }
    }
    catch { throw "UDP ports $Port-$($Port + 3) are unavailable: $($_.Exception.Message)" }
    finally { foreach ($Socket in $Sockets) { $Socket.Dispose() } }
}

function Read-SharedLog {
    param([string]$Path)
    $Stream = $null
    $Reader = $null
    try {
        $Stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
        $Reader = New-Object System.IO.StreamReader($Stream)
        return $Reader.ReadToEnd()
    }
    finally {
        if ($null -ne $Reader) { $Reader.Dispose() }
        elseif ($null -ne $Stream) { $Stream.Dispose() }
    }
}

function ConvertFrom-TestTranscript {
    param([string]$Text, [string]$ExpectedVersion)
    $FixtureLines = @([regex]::Matches($Text, '\[DM\] fixture [^\r\n]+') | ForEach-Object { $_.Value.Trim() })
    $MedicalLines = @([regex]::Matches($Text, '\[DM-MEDICAL\] [^\r\n]+') | ForEach-Object { $_.Value.Trim() })
    $FixturePasses = @($FixtureLines | Where-Object { $_ -match '\sPASS\s*$' } | Select-Object -Unique)
    $FixtureNames = @($FixturePasses | ForEach-Object { if ($_ -match '^\[DM\] fixture ([^:]+):') { $Matches[1] } } | Select-Object -Unique)
    $Assertions = @($MedicalLines | Where-Object { $_ -match '\s(PASS|FAIL)\s*$' } | Select-Object -Unique)
    $Failures = @($FixtureLines + $MedicalLines | Where-Object { $_ -match '\sFAIL\s*$' })
    $MalformedFixtures = @($FixtureLines | Where-Object { $_ -notmatch '^\[DM\] fixture [^:]+:.*\s(PASS|FAIL)\s*$' })
    $Errors = @([regex]::Matches($Text, "(?im)^.*(?:Can't compile|Cannot compile|SCRIPT\s*\(E\)|SCRIPT\s*:\s*(?:\[ERROR\]|ERROR\b)|\b(?:SCRIPT|RUNTIME)\s+ERROR\b|NULL pointer to instance|Undefined function|Multiple declaration|Error loading mission|Cannot open mission|Cannot load mission)[^\r\n]*") | ForEach-Object { $_.Value.Trim() } | Select-Object -Unique)
    return @{
        Logs = @()
        FixturePassCount = $FixtureNames.Count
        FixtureLines = $FixturePasses
        MalformedFixtures = $MalformedFixtures
        Assertions = $Assertions
        Failures = $Failures
        Errors = $Errors
        BootComplete = $Text -match '\[DM\] boot fixtures complete'
        EngineStarted = $Text -match ('\[DM\] round engine started v' + [regex]::Escape($ExpectedVersion) + '(?:\s|$)')
        SmokeComplete = $Text -match '\[DM-MEDICAL\] smoke complete'
    }
}

function Get-RequiredMedicalAssertions {
    param([string]$Mode)
    $Required = @("configured mode, interval and HP", "bleeding precondition", "no early heal", "benchmark 60 bodies")
    if ($Mode -eq "regen") {
        $Required += @("configured HP and bleed clearing", "blood and shock unchanged", "stall heals once", "no catch-up burst", "maximum HP cap", "full HP bleeding precondition", "full HP still clears bleeds", "dead player remains dead")
    }
    else { $Required += "bandages preserves health and bleeding" }
    return $Required
}

function Get-TestTranscriptProblems {
    param([hashtable]$State, [string]$Mode)
    $Problems = @()
    if ($State.Errors.Count -gt 0) { $Problems += "Compile/runtime error: $($State.Errors[0])" }
    if ($State.Failures.Count -gt 0) { $Problems += "Assertion failed: $($State.Failures[0])" }
    if ($State.MalformedFixtures.Count -gt 0) { $Problems += "Malformed fixture result: $($State.MalformedFixtures[0])" }
    if (-not $State.BootComplete -or $State.FixturePassCount -lt 66) { $Problems += "Expected >=66 distinct boot fixtures and completion; got $($State.FixturePassCount)" }
    if (-not $State.EngineStarted) { $Problems += "Round engine startup marker for expected version missing" }
    if (-not $State.SmokeComplete) { $Problems += "Medical smoke completion marker missing" }
    foreach ($Required in @(Get-RequiredMedicalAssertions -Mode $Mode)) {
        if ($State.Assertions -notcontains ("[DM-MEDICAL] " + $Required + " PASS")) { $Problems += "Medical assertion missing: $Required" }
    }
    return $Problems
}

function Get-TestLogState {
    param([string]$CaseRoot, [string]$ExpectedVersion)
    $Logs = @(Get-ChildItem -LiteralPath $CaseRoot -Recurse -File | Where-Object { $_.Extension -in @(".log", ".RPT") })
    $Text = ($Logs | ForEach-Object { Read-SharedLog -Path $_.FullName }) -join "`n"
    $State = ConvertFrom-TestTranscript -Text $Text -ExpectedVersion $ExpectedVersion
    $State.Logs = @($Logs.FullName)
    return $State
}

if ($SelfTest) {
    # Pure transcript tests also run on Linux CI without DayZ or a built PBO.
    $Version = "1.2.3"
    $FixtureTranscript = (1..66 | ForEach-Object { "SCRIPT : [DM] fixture synthetic${_}: expected=1 got=1 PASS" }) -join "`n"
    $Prefix = $FixtureTranscript + "`n[DM] boot fixtures complete`n[DM] round engine started v$Version`n"
    $RegenTranscript = $Prefix + ((Get-RequiredMedicalAssertions -Mode "regen" | ForEach-Object { "[DM-MEDICAL] $_ PASS" }) -join "`n") + "`n[DM-MEDICAL] smoke complete"
    $BandagesTranscript = $Prefix + ((Get-RequiredMedicalAssertions -Mode "bandages" | ForEach-Object { "[DM-MEDICAL] $_ PASS" }) -join "`n") + "`n[DM-MEDICAL] smoke complete"
    $Checks = @(
        @{ Name = "complete regen"; Text = $RegenTranscript; Mode = "regen"; Pass = $true },
        @{ Name = "complete bandages"; Text = $BandagesTranscript; Mode = "bandages"; Pass = $true },
        @{ Name = "missing completion"; Text = $RegenTranscript.Replace("[DM-MEDICAL] smoke complete", ""); Mode = "regen"; Pass = $false },
        @{ Name = "failed medical assertion"; Text = $RegenTranscript.Replace("no early heal PASS", "no early heal FAIL"); Mode = "regen"; Pass = $false },
        @{ Name = "failed boot fixture"; Text = $RegenTranscript.Replace("synthetic1: expected=1 got=1 PASS", "synthetic1: expected=1 got=0 FAIL"); Mode = "regen"; Pass = $false },
        @{ Name = "compile error"; Text = $RegenTranscript + "`nCan't compile World script module"; Mode = "regen"; Pass = $false },
        @{ Name = "runtime error"; Text = $RegenTranscript + "`nSCRIPT (E): NULL pointer to instance"; Mode = "regen"; Pass = $false },
        @{ Name = "script error format"; Text = $RegenTranscript + "`nSCRIPT : [ERROR] runtime failure"; Mode = "regen"; Pass = $false },
        @{ Name = "too few fixtures"; Text = $RegenTranscript.Replace("[DM] fixture synthetic1: expected=1 got=1 PASS", ""); Mode = "regen"; Pass = $false },
        @{ Name = "duplicate fixture cannot fill count"; Text = $RegenTranscript.Replace("synthetic1:", "synthetic2:"); Mode = "regen"; Pass = $false },
        @{ Name = "missing boot completion"; Text = $RegenTranscript.Replace("[DM] boot fixtures complete", ""); Mode = "regen"; Pass = $false },
        @{ Name = "stale PBO version"; Text = $RegenTranscript.Replace("started v1.2.3", "started v1.2.2"); Mode = "regen"; Pass = $false },
        @{ Name = "version prefix is insufficient"; Text = $RegenTranscript.Replace("started v1.2.3", "started v1.2.30"); Mode = "regen"; Pass = $false },
        @{ Name = "missing config expectations assertion"; Text = $RegenTranscript.Replace("[DM-MEDICAL] configured mode, interval and HP PASS", ""); Mode = "regen"; Pass = $false },
        @{ Name = "missing case-specific assertion"; Text = $RegenTranscript.Replace("[DM-MEDICAL] dead player remains dead PASS", ""); Mode = "regen"; Pass = $false },
        @{ Name = "bandages cannot stand in for regen"; Text = $BandagesTranscript; Mode = "regen"; Pass = $false },
        @{ Name = "malformed fixture"; Text = $RegenTranscript + "`n[DM] fixture unexpected: no verdict"; Mode = "regen"; Pass = $false }
    )
    foreach ($Check in $Checks) {
        $State = ConvertFrom-TestTranscript -Text $Check.Text -ExpectedVersion $Version
        $Problems = @(Get-TestTranscriptProblems -State $State -Mode $Check.Mode)
        if (($Problems.Count -eq 0) -ne $Check.Pass) { throw "Parser self-test failed: $($Check.Name); problems=$($Problems -join '; ')" }
        Write-Host "[medical-tests] parser $($Check.Name): PASS"
    }
    Write-Host "[medical-tests] PASS: $($Checks.Count) parser self-tests"
    return
}

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
if (-not $ServerRoot) { $ServerRoot = Join-Path ([Environment]::GetFolderPath('UserProfile')) "dev/dayz-server-win/steamcmd/steamapps/common/DayZServer" }
$ServerRoot = (Resolve-Path -LiteralPath $ServerRoot).Path
$ServerExe = Join-Path $ServerRoot "DayZServer_x64.exe"
$MissionSource = Join-Path $PSScriptRoot "medical-smoke\init.c"
$ModRoot = Join-Path $RepoRoot "build\@SentinelDeathmatch"
if (-not (Test-Path -LiteralPath $ServerExe -PathType Leaf)) { throw "DayZ server executable missing: $ServerExe" }
if (-not (Test-Path -LiteralPath $MissionSource -PathType Leaf)) { throw "Medical mission missing: $MissionSource" }
$VersionSource = Get-Content -LiteralPath (Join-Path $RepoRoot "scripts\3_Game\DmVersion.c") -Raw
if ($VersionSource -notmatch 'VERSION\s*=\s*"([^"]+)"') { throw "Cannot read expected DmVersion.VERSION" }
$ExpectedVersion = $Matches[1]

$RunId = (Get-Date -Format "yyyyMMdd-HHmmss") + "-" + [guid]::NewGuid().ToString("N").Substring(0, 8)
$RunRoot = Join-Path $RepoRoot ("build\medical-tests\" + $RunId)
New-Item -ItemType Directory -Path $RunRoot -Force | Out-Null
$Suite = [ordered]@{ RunId = $RunId; StartedUtc = [DateTime]::UtcNow.ToString("o"); ServerRoot = $ServerRoot; Status = "Running"; Cases = @(); Error = $null }
Write-Host "[medical-tests] Evidence: $RunRoot"

try {
    if (-not $SkipBuild) { & (Join-Path $RepoRoot "tools\build.ps1") }
    $Pbo = Join-Path $ModRoot "addons\sentinel_dm.pbo"
    if (-not (Test-Path -LiteralPath $Pbo -PathType Leaf) -or (Get-Item -LiteralPath $Pbo).Length -eq 0) { throw "Built mod missing or empty: $Pbo" }

    for ($CaseIndex = 0; $CaseIndex -lt $Cases.Count; $CaseIndex++) {
        $Case = $Cases[$CaseIndex]
        $CaseRoot = Join-Path $RunRoot $Case.Name
        $Profile = Join-Path $CaseRoot "profile"
        $ConfigDir = Join-Path $Profile "SentinelDeathmatch"
        $Mission = Join-Path $CaseRoot "medical.chernarusplus"
        $ServerConfig = Join-Path $CaseRoot "serverDZ.cfg"
        $Port = $BasePort + $CaseIndex * 10
        $Process = $null
        $CaseResult = [ordered]@{ Name = $Case.Name; Status = "Running"; Expected = $Case.Expected; InputConfig = $Case.Config; ConfigHashBefore = $null; ConfigHashAfter = $null; Port = $Port; ProcessId = $null; Seconds = 0; Error = $null; LogState = $null }
        $Watch = [System.Diagnostics.Stopwatch]::StartNew()
        try {
            New-Item -ItemType Directory -Path $ConfigDir, $Mission -Force | Out-Null
            Write-TestJson -Value $Case.Config -Path (Join-Path $ConfigDir "config.json")
            $CaseResult.ConfigHashBefore = (Get-FileHash -LiteralPath (Join-Path $ConfigDir "config.json") -Algorithm SHA256).Hash
            Write-TestJson -Value $Case.Expected -Path (Join-Path $Profile "medical-expectations.json")
            Copy-Item -LiteralPath $MissionSource -Destination (Join-Path $Mission "init.c")
            $Password = [guid]::NewGuid().ToString("N")
            @"
hostname = "Sentinel medical local test $($Case.Name)";
instanceId = $Port;
password = "$Password";
maxPlayers = 1;
verifySignatures = 0;
forceSameBuild = 0;
BattlEye = 0;
steamQueryPort = $($Port + 2);
steamPort = $($Port + 3);
disableVoN = 1;
serverTime = "2026/06/21/12/00";
serverTimeAcceleration = 0;
serverNightTimeAcceleration = 1;
serverTimePersistent = 0;
class Missions { class DayZ { template = "medical.chernarusplus"; }; };
"@ | Set-Content -LiteralPath $ServerConfig -Encoding ASCII
            Assert-PortsAvailable -Port $Port
            $LaunchArgs = @("-config=$ServerConfig", "-profiles=$Profile", "-mission=$Mission", "-mod=$ModRoot", "-port=$Port", "-ip=127.0.0.1", "-cpuCount=2", "-limitFPS=30", "-doLogs", "-adminLog", "-freezecheck")
            # Start-Process joins arrays without quoting; quote whole switches to
            # keep their absolute paths intact on both Windows PowerShell and 7.
            $ArgumentLine = ($LaunchArgs | ForEach-Object { '"' + $_ + '"' }) -join " "
            Write-Host "[medical-tests] $($Case.Name): starting on 127.0.0.1:$Port"
            $Process = Start-Process -FilePath $ServerExe -WorkingDirectory $ServerRoot -ArgumentList $ArgumentLine -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $CaseRoot "console.out.log") -RedirectStandardError (Join-Path $CaseRoot "console.err.log")
            $CaseResult.ProcessId = $Process.Id
            while ($true) {
                $State = Get-TestLogState -CaseRoot $CaseRoot -ExpectedVersion $ExpectedVersion
                $CaseResult.LogState = $State
                if ($State.Errors.Count -gt 0) { throw "Compile/runtime error: $($State.Errors[0])" }
                if ($State.Failures.Count -gt 0) { throw "Assertion failed: $($State.Failures[0])" }
                if ($State.SmokeComplete) {
                    $Problems = @(Get-TestTranscriptProblems -State $State -Mode $Case.Expected.Mode)
                    if ($Problems.Count -gt 0) { throw ($Problems -join "; ") }
                    $CaseResult.Status = "Passed"
                    break
                }
                $Process.Refresh()
                if ($Process.HasExited) { throw "Server exited before smoke completion (exit $($Process.ExitCode))" }
                if ($Watch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw "Server smoke timed out after ${TimeoutSeconds}s" }
                Start-Sleep -Milliseconds 250
            }
        }
        catch {
            $CaseResult.Status = "Failed"
            $CaseResult.Error = $_.Exception.Message
        }
        finally {
            if ($null -ne $Process) {
                $Process.Refresh()
                if (-not $Process.HasExited) {
                    $Process.Kill()
                    if (-not $Process.WaitForExit(10000)) { throw "Owned server PID $($Process.Id) did not stop; refusing to launch another case" }
                }
                $Process.Dispose()
            }
            $Watch.Stop()
            $CaseResult.Seconds = [math]::Round($Watch.Elapsed.TotalSeconds, 2)
            # Re-read after shutdown to preserve final diagnostics and late writes.
            $CaseResult.LogState = Get-TestLogState -CaseRoot $CaseRoot -ExpectedVersion $ExpectedVersion
            $ConfigPath = Join-Path $ConfigDir "config.json"
            if (Test-Path -LiteralPath $ConfigPath) { $CaseResult.ConfigHashAfter = (Get-FileHash -LiteralPath $ConfigPath -Algorithm SHA256).Hash }
            if ($CaseResult.ConfigHashBefore -and $CaseResult.ConfigHashBefore -ne $CaseResult.ConfigHashAfter) {
                $CaseResult.Status = "Failed"
                $CaseResult.Error = "Existing operator config.json changed during load"
            }
            if ($CaseResult.LogState.Errors.Count -gt 0 -or $CaseResult.LogState.Failures.Count -gt 0) {
                $CaseResult.Status = "Failed"
                if (-not $CaseResult.Error) { $CaseResult.Error = "Failure appeared in final log snapshot" }
            }
            Write-TestJson -Value $CaseResult -Path (Join-Path $CaseRoot "result.json")
            $Suite.Cases += $CaseResult
        }
        Write-Host "[medical-tests] $($Case.Name): $($CaseResult.Status) ($($CaseResult.LogState.FixturePassCount) fixtures, $($CaseResult.LogState.Assertions.Count) assertions, $($CaseResult.Seconds)s)"
        if ($CaseResult.Error) { Write-Host "[medical-tests]   $($CaseResult.Error)" }
    }
    $FailedCases = @($Suite.Cases | Where-Object { $_.Status -ne "Passed" })
    if ($FailedCases.Count -gt 0) { throw "$($FailedCases.Count)/$($Cases.Count) medical cases failed; see $RunRoot" }
    $Suite.Status = "Passed"
    Write-Host "[medical-tests] PASS: all $($Cases.Count) cases"
}
catch {
    $Suite.Status = "Failed"
    $Suite.Error = $_.Exception.Message
    throw
}
finally {
    $Suite.CompletedUtc = [DateTime]::UtcNow.ToString("o")
    Write-TestJson -Value $Suite -Path (Join-Path $RunRoot "result.json")
}
