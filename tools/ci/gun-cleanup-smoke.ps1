# Native entity probes, separate from pure boot fixtures. Requires an installed
# DayZ dedicated server and an already-built development PBO.
[CmdletBinding()]
param(
    [string]$ServerRoot,
    [ValidateSet('server', 'round_end', 'player_death', 'missing', 'invalid')]
    [string[]]$Mode = @('server', 'round_end', 'player_death', 'missing', 'invalid'),
    [int]$Port = 24912,
    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'

$script:CommonAssertions = @(
    'setup',
    'configured mode',
    'combat gunshot death',
    'corpse gun released',
    'looted gun owned',
    'LIVE before deadline',
    'no cleanup before ten seconds',
    'manual gun survives combat',
    'looted gun survives combat',
    'LIVE after deadline',
    'death policy during combat',
    'early round end keeps death deadline',
    'late death setup',
    'countdown preserves gun container',
    'manual gun survives until round end',
    'corpse cleared',
    'round end death gun policy',
    'round end manual gun policy',
    'looted gun survives round end',
    'pickup after queue survives',
    'looted gun survives death deadline',
    'LIVE exit 0 follows mode',
    'LIVE exit 1 follows mode',
    'LIVE exit 2 follows mode'
)

function Get-AssertionNames {
    param([string]$ExpectedMode)

    $names = @($script:CommonAssertions)
    if ($ExpectedMode -eq 'player_death') {
        $names += 'death cleanup within sweep tolerance'
    }
    return $names
}

function Get-ObservedLines {
    param([string[]]$Lines, [string]$Pattern)
    return @($Lines | Where-Object { $_ -match $Pattern })
}

function Get-NormalizedMarkerLines {
    param([string[]]$Lines)

    $markers = @()
    foreach ($line in $Lines) {
        if ($line -match '(\[(?:DM|GUN-PROBE)\].*)$') { $markers += $Matches[1].Trim() }
    }
    return $markers
}

function Assert-NamedPassLines {
    param(
        [string[]]$Lines,
        [string]$Prefix,
        [string[]]$ExpectedNames
    )

    $seen = @{}
    foreach ($name in $ExpectedNames) { $seen[$name] = 0 }
    if ($Prefix -eq '[DM] fixture') {
        $assertionPattern = '^\[DM\] fixture (.+?): expected=.* got=.* (PASS|FAIL)\s*$'
    } else {
        $assertionPattern = '^' + [regex]::Escape($Prefix) + ' (.+?) (PASS|FAIL)\s*$'
    }
    foreach ($line in $Lines) {
        if ($line -match $assertionPattern) {
            $name = $Matches[1]
            if (-not $seen.ContainsKey($name)) {
                throw "Unexpected assertion name: $name"
            }
            $seen[$name]++
            if ($Matches[2] -ne 'PASS') { throw "Assertion failed: $name" }
        }
    }

    $missing = @($ExpectedNames | Where-Object { $seen[$_] -eq 0 })
    $duplicates = @($ExpectedNames | Where-Object { $seen[$_] -gt 1 })
    if ($missing.Count -gt 0) { throw "Missing assertion names: $($missing -join ', ')" }
    if ($duplicates.Count -gt 0) { throw "Duplicate assertion names: $($duplicates -join ', ')" }
}

function Assert-RunEvidence {
    param(
        [string[]]$ScriptLines,
        [string[]]$RptLines,
        [string[]]$ExpectedFixtureNames,
        [string[]]$ExpectedAssertionNames,
        [string]$ExpectedMode,
        [string]$Version
    )

    $allLines = @($ScriptLines) + @($RptLines)
    $markerLines = @(Get-NormalizedMarkerLines -Lines $ScriptLines)
    $errorLines = @(Get-ObservedLines -Lines $allLines -Pattern '(?i)\bFAIL\b|SCRIPT\s*\(E\)|SCRIPT ERROR|Compile error|Cannot compile|Can''t compile|Error while compiling|Unhandled exception|Access violation|Fatal error')
    if ($errorLines.Count -gt 0) {
        throw "Script or runtime error: $($errorLines -join ' | ')"
    }

    $fixtureNames = @($ExpectedFixtureNames)
    if ($fixtureNames.Count -eq 0) { throw 'No registered fixture names were discovered from scripts/*.c' }
    Assert-NamedPassLines -Lines $markerLines -Prefix '[DM] fixture' -ExpectedNames $fixtureNames

    $assertions = @($ExpectedAssertionNames | Where-Object { $_ })
    if ($assertions.Count -eq 0) { $assertions = @(Get-AssertionNames -ExpectedMode $ExpectedMode) }
    Assert-NamedPassLines -Lines $markerLines -Prefix '[GUN-PROBE]' -ExpectedNames $assertions

    $startup = '^\[DM\] round engine started v' + [regex]::Escape($Version) + '\s*$'
    if (@(Get-ObservedLines -Lines $markerLines -Pattern $startup).Count -ne 1) {
        throw "Missing or duplicate version-specific round-engine startup marker for v$Version"
    }
    if (@(Get-ObservedLines -Lines $markerLines -Pattern '^\[DM\] boot fixtures complete\s*$').Count -ne 1) {
        throw 'Missing or duplicate boot-fixtures completion marker'
    }
    $completion = '^\[GUN-PROBE\] complete ' + [regex]::Escape($ExpectedMode) + '\s*$'
    if (@(Get-ObservedLines -Lines $markerLines -Pattern $completion).Count -ne 1) {
        throw "Missing or duplicate completion marker for expected mode $ExpectedMode"
    }
    if (@(Get-ObservedLines -Lines $markerLines -Pattern '^\[GUN-PROBE\] complete\s+').Count -ne 1) {
        throw 'Completion marker missing or emitted more than once'
    }
}

function Invoke-ParserSelfTest {
    $fixtures = @('fixture alpha', 'fixture beta')
    $assertions = @('probe alpha', 'probe beta')
    $scriptLines = @(
        '[DM] fixture fixture alpha: expected=1 got=1 PASS',
        '[DM] fixture fixture beta: expected=1 got=1 PASS',
        '[DM] round engine started v9.8.7',
        '[DM] boot fixtures complete',
        '[GUN-PROBE] probe alpha PASS',
        '[GUN-PROBE] probe beta PASS',
        '[GUN-PROBE] complete round_end'
    )

    # Exercise the parser with a small known fixture/assertion set.
    Assert-NamedPassLines -Lines $scriptLines -Prefix '[DM] fixture' -ExpectedNames $fixtures
    Assert-NamedPassLines -Lines $scriptLines -Prefix '[GUN-PROBE]' -ExpectedNames $assertions
    Assert-RunEvidence -ScriptLines $scriptLines -RptLines @() -ExpectedFixtureNames $fixtures -ExpectedAssertionNames $assertions -ExpectedMode 'round_end' -Version '9.8.7'

    $prefixedLines = @($scriptLines | ForEach-Object { 'SCRIPT       : ' + $_ })
    Assert-RunEvidence -ScriptLines $prefixedLines -RptLines @() -ExpectedFixtureNames $fixtures -ExpectedAssertionNames $assertions -ExpectedMode 'round_end' -Version '9.8.7'

    $defaultLines = @($scriptLines | Where-Object { $_ -notmatch '^\[GUN-PROBE\] probe ' })
    $defaultLines += @(Get-AssertionNames 'round_end' | ForEach-Object { '[GUN-PROBE] ' + $_ + ' PASS' })
    Assert-RunEvidence -ScriptLines $defaultLines -RptLines @() -ExpectedFixtureNames $fixtures -ExpectedMode 'round_end' -Version '9.8.7'

    $duplicate = @($scriptLines) + '[DM] fixture fixture alpha: expected=1 got=1 PASS'
    $duplicateRejected = $false
    try { Assert-NamedPassLines -Lines $duplicate -Prefix '[DM] fixture' -ExpectedNames $fixtures }
    catch { $duplicateRejected = $_.Exception.Message -match 'Duplicate assertion names' }
    if (-not $duplicateRejected) { throw 'Self-test failed: duplicate fixture did not fail closed' }

    $wrongMode = @($scriptLines | Where-Object { $_ -notmatch '^\[GUN-PROBE\] complete ' }) + '[GUN-PROBE] complete server'
    $wrongModeRejected = $false
    try { Assert-RunEvidence -ScriptLines $wrongMode -RptLines @() -ExpectedFixtureNames $fixtures -ExpectedAssertionNames $assertions -ExpectedMode 'round_end' -Version '9.8.7' }
    catch { $wrongModeRejected = $_.Exception.Message -match 'completion marker' }
    if (-not $wrongModeRejected) { throw 'Self-test failed: wrong-mode completion did not fail closed' }

    $missing = @($scriptLines | Where-Object { $_ -notmatch 'fixture beta' })
    $missingRejected = $false
    try { Assert-NamedPassLines -Lines $missing -Prefix '[DM] fixture' -ExpectedNames $fixtures }
    catch { $missingRejected = $_.Exception.Message -match 'Missing assertion names' }
    if (-not $missingRejected) { throw 'Self-test failed: missing fixture did not fail closed' }

    $duplicateAssertion = @($scriptLines) + '[GUN-PROBE] probe alpha PASS'
    $duplicateAssertionRejected = $false
    try { Assert-NamedPassLines -Lines $duplicateAssertion -Prefix '[GUN-PROBE]' -ExpectedNames $assertions }
    catch { $duplicateAssertionRejected = $_.Exception.Message -match 'Duplicate assertion names' }
    if (-not $duplicateAssertionRejected) { throw 'Self-test failed: duplicate probe did not fail closed' }

    $missingAssertion = @($scriptLines | Where-Object { $_ -notmatch '^\[GUN-PROBE\] probe beta ' })
    $missingAssertionRejected = $false
    try { Assert-NamedPassLines -Lines $missingAssertion -Prefix '[GUN-PROBE]' -ExpectedNames $assertions }
    catch { $missingAssertionRejected = $_.Exception.Message -match 'Missing assertion names' }
    if (-not $missingAssertionRejected) { throw 'Self-test failed: missing probe did not fail closed' }

    $rptErrorRejected = $false
    try { Assert-RunEvidence -ScriptLines $scriptLines -RptLines @('SCRIPT (E): synthetic runtime failure') -ExpectedFixtureNames $fixtures -ExpectedAssertionNames $assertions -ExpectedMode 'round_end' -Version '9.8.7' }
    catch { $rptErrorRejected = $_.Exception.Message -match 'Script or runtime error' }
    if (-not $rptErrorRejected) { throw 'Self-test failed: RPT runtime error did not fail closed' }

    Write-Output 'Gun cleanup parser self-test PASS'
}

if ($SelfTest) {
    Invoke-ParserSelfTest
    exit 0
}

if (-not $ServerRoot) { throw 'ServerRoot is required unless -SelfTest is used.' }
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$serverExe = Join-Path $ServerRoot 'DayZServer_x64.exe'
$modPath = Join-Path $repoRoot 'build\@SentinelDeathmatch'
if (-not (Test-Path -LiteralPath $serverExe)) { throw "Missing server: $serverExe" }
if (-not (Test-Path -LiteralPath "$modPath\addons\sentinel_dm.pbo")) { throw 'Build the mod first with tools/build.ps1' }

$versionSource = Get-Content -LiteralPath (Join-Path $repoRoot 'scripts\3_Game\DmVersion.c') -Raw
if ($versionSource -notmatch 'VERSION\s*=\s*"([0-9]+\.[0-9]+\.[0-9]+)"') { throw 'Could not read DmVersion.VERSION' }
$version = $Matches[1]
$manualOnlyFixtureNames = @(
    '<name>' # Comment placeholder in DmRunSelfTests' output format example.
    'DmTheme malformed layer atomic' # In SelfTestInvalidJson, not called by DmRunSelfTests.
    'DmTheme invalid root and field type' # In SelfTestInvalidJson, not called by DmRunSelfTests.
)
$fixtureNames = @(
    Get-ChildItem -LiteralPath (Join-Path $repoRoot 'scripts') -Recurse -File -Filter '*.c' |
        Select-String -Pattern '\[DM\] fixture ([^:"]+): expected=' |
        ForEach-Object { $_.Matches[0].Groups[1].Value } |
        Where-Object { $_ -notin $manualOnlyFixtureNames } |
        Sort-Object -Unique
)
if ($fixtureNames.Count -eq 0) { throw 'No registered fixture names discovered in scripts/*.c' }

function Test-PortAvailable {
    param([int]$CandidatePort)
    $udp = $null
    $tcp = $null
    try {
        $udp = [System.Net.Sockets.UdpClient]::new($CandidatePort)
        $tcp = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $CandidatePort)
        $tcp.Start()
        return $true
    } catch {
        return $false
    } finally {
        if ($tcp) { $tcp.Stop() }
        if ($udp) { $udp.Dispose() }
    }
}

$caseIndex = 0
foreach ($requestedMode in $Mode) {
    $expectedMode = $requestedMode
    if ($requestedMode -in @('missing', 'invalid')) { $expectedMode = 'round_end' }
    $casePort = $Port + ($caseIndex * 10)
    $queryPort = $casePort + 5
    if ($casePort -gt 65530 -or $queryPort -gt 65535) { throw "Port range exceeded at case $requestedMode" }
    foreach ($candidatePort in @($casePort, $queryPort)) {
        if (-not (Test-PortAvailable -CandidatePort $candidatePort)) { throw "Required UDP/TCP port $candidatePort is unavailable before $requestedMode" }
    }

    $runId = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
    $probeRoot = Join-Path $repoRoot "build\gun-cleanup-smoke\$requestedMode-$runId"
    $profilePath = Join-Path $probeRoot 'profile'
    $missionPath = Join-Path $probeRoot 'mission.chernarusplus'
    New-Item -ItemType Directory -Force -Path "$profilePath\SentinelDeathmatch", $missionPath | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'gun-cleanup-smoke\init.c') -Destination "$missionPath\init.c"

    $config = [ordered]@{ MinPlayers = 2; MaxDeletesPerTick = 3; CorpseLifetimeSeconds = 45 }
    if ($requestedMode -notin @('missing', 'invalid')) { $config.GunCleanupMode = $requestedMode }
    if ($requestedMode -eq 'invalid') { $config.GunCleanupMode = 'unknown_smoke_mode' }
    $configPath = Join-Path $profilePath 'SentinelDeathmatch\config.json'
    $config | ConvertTo-Json | Set-Content -LiteralPath $configPath -Encoding ascii
    $expectedPath = Join-Path $profilePath 'gun-cleanup-expected.json'
    @{ Mode = $expectedMode } | ConvertTo-Json | Set-Content -LiteralPath $expectedPath -Encoding ascii
    $expectation = Get-Content -LiteralPath $expectedPath -Raw | ConvertFrom-Json
    if (-not $expectation.Mode -or $expectation.Mode -ne $expectedMode) { throw "Invalid expected-mode file: $expectedPath" }
    $expectedMode = [string]$expectation.Mode
    $configHashBefore = (Get-FileHash -LiteralPath $configPath -Algorithm SHA256).Hash

    $serverCfg = Join-Path $probeRoot 'server.cfg'
    @"
hostname="Local gun cleanup fixtures";
password="local-fixtures";
maxPlayers=1;
verifySignatures=0;
BattlEye=0;
steamQueryPort=$queryPort;
instanceId=914;
class Missions { class DayZ { template="dayzOffline.chernarusplus"; }; };
"@ | Set-Content -LiteralPath $serverCfg -Encoding ascii

    # Local inventory moves make synthetic players independent of client acknowledgements.
    $serverArgs = @("`"-config=$serverCfg`"", '-ip=127.0.0.1', "-port=$casePort", "`"-profiles=$profilePath`"", "`"-mod=$modPath`"", "`"-mission=$missionPath`"", '-dologs', '-noPause', '-limitFPS=60', '-NO_GUI')
    $serverProc = $null
    $scriptLog = $null
    try {
        $serverProc = Start-Process -FilePath $serverExe -WorkingDirectory $ServerRoot -ArgumentList $serverArgs -WindowStyle Hidden -PassThru
        for ($poll = 0; $poll -lt 120; $poll++) {
            Start-Sleep -Seconds 1
            $scriptLog = Get-ChildItem -LiteralPath $profilePath -Filter 'script_*.log' -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
            if ($scriptLog -and (Select-String -LiteralPath $scriptLog.FullName -Pattern '\[GUN-PROBE\] complete\s+' -Quiet)) { break }
            if ($serverProc.HasExited) { throw "Server exited $($serverProc.ExitCode); logs: $profilePath" }
        }
        if (-not $scriptLog) { throw "No script log: $profilePath" }
    } finally {
        if ($serverProc -and -not $serverProc.HasExited) {
            Stop-Process -Id $serverProc.Id -Force
            $serverProc.WaitForExit()
        }
    }

    $configHashAfter = (Get-FileHash -LiteralPath $configPath -Algorithm SHA256).Hash
    if ($configHashAfter -ne $configHashBefore) { throw "Config changed during $requestedMode run; before=$configHashBefore after=$configHashAfter; logs: $profilePath" }
    if (-not $scriptLog) { throw "No script log: $profilePath" }
    $scriptLines = @(Get-Content -LiteralPath $scriptLog.FullName)
    $rptFiles = @(Get-ChildItem -LiteralPath $profilePath -Recurse -File -Filter '*.RPT' -ErrorAction SilentlyContinue)
    if ($rptFiles.Count -eq 0) { throw "No RPT log: $profilePath" }
    $rptLines = @($rptFiles | ForEach-Object { Get-Content -LiteralPath $_.FullName })
    Assert-RunEvidence -ScriptLines $scriptLines -RptLines $rptLines -ExpectedFixtureNames $fixtureNames -ExpectedMode $expectedMode -Version $version
    Write-Output "Mode=$requestedMode expected=$expectedMode fixtures=$($fixtureNames.Count) assertions=$(@(Get-AssertionNames $expectedMode).Count) PASS; log=$($scriptLog.FullName); configHashBefore=$configHashBefore configHashAfter=$configHashAfter"
    $caseIndex++
}
