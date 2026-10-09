# Native entity probes, separate from the pure boot fixtures. Requires an
# installed DayZ dedicated server and an already-built development PBO.
param(
    [Parameter(Mandatory = $true)][string]$ServerRoot,
    [ValidateSet('server', 'round_end', 'player_death')][string]$Mode = 'round_end',
    [int]$Port = 24912
)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$serverExe = Join-Path $ServerRoot 'DayZServer_x64.exe'
$modPath = Join-Path $repoRoot 'build\@SentinelDeathmatch'
if (-not (Test-Path -LiteralPath $serverExe)) { throw "Missing server: $serverExe" }
if (-not (Test-Path -LiteralPath "$modPath\addons\sentinel_dm.pbo")) { throw 'Build the mod first with tools/build.ps1' }
$runId = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$probeRoot = Join-Path $repoRoot "build\gun-cleanup-smoke\$Mode-$runId"
$profilePath = Join-Path $probeRoot 'profile'
$missionPath = Join-Path $probeRoot 'mission.chernarusplus'
New-Item -ItemType Directory -Force -Path "$profilePath\SentinelDeathmatch", $missionPath | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'gun-cleanup-smoke\init.c') -Destination "$missionPath\init.c"
@{ GunCleanupMode = $Mode; MinPlayers = 2; MaxDeletesPerTick = 100; CorpseLifetimeSeconds = 45 } | ConvertTo-Json | Set-Content -LiteralPath "$profilePath\SentinelDeathmatch\config.json" -Encoding ascii
$serverCfg = Join-Path $probeRoot 'server.cfg'
$queryPort = $Port + 5
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
# Local inventory moves in the mission make the synthetic players independent
# of client acknowledgements. No client or production integration is loaded.
$serverArgs = @("`"-config=$serverCfg`"", "-port=$Port", "`"-profiles=$profilePath`"", "`"-mod=$modPath`"", "`"-mission=$missionPath`"", '-dologs', '-noPause', '-limitFPS=60', '-NO_GUI')
$serverProc = Start-Process -FilePath $serverExe -WorkingDirectory $ServerRoot -ArgumentList $serverArgs -WindowStyle Hidden -PassThru
try {
    $scriptLog = $null
    for ($poll = 0; $poll -lt 120; $poll++) {
        Start-Sleep -Seconds 1
        $scriptLog = Get-ChildItem "$profilePath\script_*.log" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if ($scriptLog -and (Select-String -LiteralPath $scriptLog.FullName -Pattern '\[GUN-PROBE\] complete' -Quiet)) { break }
        if ($serverProc.HasExited) { throw "Server exited $($serverProc.ExitCode); logs: $profilePath" }
    }
    if (-not $scriptLog) { throw "No script log: $profilePath" }
    $logLines = Get-Content -LiteralPath $scriptLog.FullName
    $errors = @($logLines | Select-String '\bFAIL\b|SCRIPT.*\(E\)|Compile error')
    $fixtures = @($logLines | Select-String '\[DM\] fixture.* PASS$')
    $probes = @($logLines | Select-String '\[GUN-PROBE\].* PASS$')
    $probes | Write-Output
    if ($errors.Count -gt 0) { $errors | Write-Output; throw "Fixture or script error; logs: $profilePath" }
    # Main v0.1.32 plus gun cleanup has 65 boot fixtures. Permit additional
    # fixtures, but never silently pass a run missing this baseline coverage.
    $expectedProbes = 16
    if ($Mode -eq 'round_end') { $expectedProbes = 15 }
    if ($fixtures.Count -lt 65 -or $probes.Count -ne $expectedProbes -or -not ($logLines | Select-String "\[GUN-PROBE\] complete $Mode$")) { throw "Incomplete probe: fixtures=$($fixtures.Count), probes=$($probes.Count); logs: $profilePath" }
    Write-Output "Mode=$Mode fixtures=$($fixtures.Count) probes=$($probes.Count) PASS; log=$($scriptLog.FullName)"
} finally {
    if (-not $serverProc.HasExited) { Stop-Process -Id $serverProc.Id }
}
