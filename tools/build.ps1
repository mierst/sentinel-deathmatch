<#
.SYNOPSIS
  Build the Sentinel Deathmatch mod folder.

.DESCRIPTION
  Output layout:
    build/
      @SentinelDeathmatch/
        addons/sentinel_dm.pbo
        keys/sentinel_dm.bikey          (when signing is enabled)

  Requirements: Mikero DePboTools (MakePbo.exe). Signing additionally needs
  DayZ Tools (DSUtils) from Steam.

.EXAMPLE
  .\tools\build.ps1
#>
[CmdletBinding()]
param(
    [switch]$KeepStaging
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

# -----------------------------------------------------------------------------
# Workshop description length gate. Steam caps descriptions at 8,000 chars and
# silently rejects oversized pastes; fail the build long before a publish run.
# -----------------------------------------------------------------------------
$DescriptionCap = 7900
if (Test-Path (Join-Path $RepoRoot "workshop")) {
    Get-ChildItem (Join-Path $RepoRoot "workshop") -Recurse -Filter "description.bbcode" | ForEach-Object {
        $len = ((Get-Content $_.FullName -Raw) -replace "`r`n", "`n").Length
        if ($len -gt $DescriptionCap) {
            throw "$($_.FullName) is $len chars - exceeds the $DescriptionCap-char budget (Steam hard cap: 8000)."
        }
    }
}

$Targets = @(
    @{
        Name      = "sentinel_dm"
        SourceDir = "."
        ModFolder = "@SentinelDeathmatch"
        PboPrefix = "SentinelDM"
        Modules   = @("3_Game", "4_World", "5_Mission")
    }
    @{
        # Consumer-side Sentinel Enforcer bridge. SEPARATE mod folder /
        # Workshop item, -serverMod= only: bundling it into the core would
        # push a hard enforcer dependency to clients (see its config.cpp).
        Name      = "dm_sentinel"
        SourceDir = "addons\dm_sentinel"
        ModFolder = "@SentinelDeathmatch-Sentinel"
        PboPrefix = "SentinelDM_Sentinel"
        Modules   = @("4_World", "5_Mission")
    }
)

# -----------------------------------------------------------------------------
# Locate Mikero tools.
# -----------------------------------------------------------------------------
$MikeroBin = "C:\Program Files (x86)\Mikero\DePboTools\bin"
if (-not (Test-Path $MikeroBin)) {
    $MikeroBin = "C:\Program Files\Mikero\DePboTools\bin"
}
if (-not (Test-Path $MikeroBin)) {
    throw "Mikero DePboTools not found. Install via mikero.bytex.digital."
}
$MakePbo = Join-Path $MikeroBin "MakePbo.exe"
if (-not (Test-Path $MakePbo)) { throw "MakePbo.exe not found at $MakePbo" }
Write-Host "[build.ps1] MakePbo: $MakePbo"

$BuildDir  = Join-Path $RepoRoot "build"
$StageRoot = Join-Path $BuildDir "staging"

if (Test-Path $StageRoot) { Remove-Item -Recurse -Force $StageRoot }
New-Item -ItemType Directory -Force -Path $StageRoot | Out-Null

foreach ($t in $Targets) {
    $dest = Join-Path $BuildDir $t.ModFolder
    if (Test-Path $dest) { Remove-Item -Recurse -Force $dest }
}

$stdoutLog = Join-Path $BuildDir "packer.out.log"
$stderrLog = Join-Path $BuildDir "packer.err.log"

# -----------------------------------------------------------------------------
# Mikero CLI helper. Mikero tools write the output file then block on a
# "Press ANY key" prompt that ignores stdin redirection - poll for the output
# and kill the process once it appears.
# -----------------------------------------------------------------------------
function Invoke-MikeroTool {
    param(
        [string]$Exe,
        [string[]]$ToolArgs,
        [string]$ExpectedOutput,
        [int]$TimeoutSec = 120
    )
    if (Test-Path $ExpectedOutput) { Remove-Item -Force $ExpectedOutput }
    $proc = Start-Process -FilePath $Exe -ArgumentList $ToolArgs `
        -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog
    $start = Get-Date
    while (-not $proc.HasExited) {
        if (Test-Path $ExpectedOutput) {
            Start-Sleep -Milliseconds 500
            if (-not $proc.HasExited) { $proc.Kill() }
            break
        }
        if (((Get-Date) - $start).TotalSeconds -gt $TimeoutSec) {
            if (-not $proc.HasExited) { $proc.Kill() }
            throw "$([System.IO.Path]::GetFileName($Exe)) timed out after ${TimeoutSec}s without producing $ExpectedOutput"
        }
        Start-Sleep -Milliseconds 200
    }

    if (Test-Path $stderrLog) {
        $errText = (Get-Content $stderrLog -Raw)
        if ($errText -and $errText.Trim().Length -gt 0) {
            $errText.Split("`n") | Where-Object { $_ -match '\S' } | ForEach-Object {
                Write-Host ("    " + $_.Trim())
            }
        }
    }
}

function Build-Target {
    param([hashtable]$T)

    $name      = $T.Name
    $sourceDir = Join-Path $RepoRoot $T.SourceDir
    $pboPrefix = $T.PboPrefix
    $modFolder = Join-Path $BuildDir $T.ModFolder
    $modules   = $T.Modules

    Write-Host ""
    Write-Host ("[build.ps1] === " + $T.ModFolder + " (" + $name + ") ===")

    $stageDir = Join-Path $StageRoot $name
    New-Item -ItemType Directory -Force -Path $stageDir | Out-Null

    $cfgSrc = Join-Path $sourceDir "config.cpp"
    if (-not (Test-Path $cfgSrc)) { throw "config.cpp missing at $cfgSrc" }
    Copy-Item -Force $cfgSrc -Destination $stageDir

    $scriptsOut = Join-Path $stageDir "scripts"
    New-Item -ItemType Directory -Force -Path $scriptsOut | Out-Null
    foreach ($m in $modules) {
        $msrc = Join-Path $sourceDir ("scripts\" + $m)
        if (Test-Path $msrc) {
            Copy-Item $msrc -Destination $scriptsOut -Recurse
        }
    }

    # Data dirs that ship inside the PBO alongside scripts (layouts now;
    # imagesets later). Referenced in-engine as "<PboPrefix>/layouts/...".
    foreach ($dataDir in @("layouts", "imagesets")) {
        $dsrc = Join-Path $sourceDir $dataDir
        if (Test-Path $dsrc) {
            Copy-Item $dsrc -Destination $stageDir -Recurse
        }
    }

    $prefixFile = Join-Path $stageDir '$PBOPREFIX$'
    Set-Content -Path $prefixFile -Value $pboPrefix -Encoding ASCII -NoNewline

    Write-Host ("    staged -> " + $stageDir)

    $modAddons = Join-Path $modFolder "addons"
    New-Item -ItemType Directory -Force -Path $modAddons | Out-Null

    $expectedPbo = Join-Path $modAddons ($name + ".pbo")
    Write-Host "    MakePbo..."
    Invoke-MikeroTool `
        -Exe $MakePbo `
        -ToolArgs @(("`"" + $stageDir + "`""), ("`"" + $expectedPbo + "`"")) `
        -ExpectedOutput $expectedPbo `
        -TimeoutSec 60

    if (-not (Test-Path $expectedPbo) -or (Get-Item $expectedPbo).Length -eq 0) {
        throw "MakePbo finished but $expectedPbo is missing or empty (target: $name)"
    }

    $workshopDir = Join-Path $RepoRoot ("workshop\" + $name)
    $metaSrc = Join-Path $workshopDir "meta.cpp"
    if (Test-Path $metaSrc) {
        Copy-Item -Force $metaSrc -Destination (Join-Path $modFolder "meta.cpp")
        Write-Host ("    bundled meta.cpp")
    }
    else {
        Write-Warning ("    workshop/$name/meta.cpp missing - Workshop upload will need a manual meta.cpp before publishing")
    }

    $previewSrc = Join-Path $RepoRoot "workshop\preview.png"
    if (Test-Path $previewSrc) {
        Copy-Item -Force $previewSrc -Destination (Join-Path $modFolder "preview.png")
    }

    return @{
        Pbo       = $expectedPbo
        ModFolder = $modFolder
    }
}

$produced = @()
foreach ($t in $Targets) {
    $r = Build-Target -T $t
    $produced += $r
}

# -----------------------------------------------------------------------------
# Signing. The client-downloaded PBO must be signed for verifySignatures=2
# servers; each mod folder carries its own keys/<keyname>.bikey copy. This mod
# uses its OWN keypair - never reuse another mod's key (separate revocation
# blast radius).
# -----------------------------------------------------------------------------
$KeyName = if ($env:SIGNING_KEY_NAME) { $env:SIGNING_KEY_NAME } else { "sentinel_dm" }
$KeyDir  = Join-Path $RepoRoot "tools\keys"
$PrivKey = Join-Path $KeyDir ($KeyName + ".biprivatekey")
$PubKey  = Join-Path $KeyDir ($KeyName + ".bikey")
$ShouldSign = ($env:USE_SIGN -eq "1") -or (Test-Path $PrivKey)

if ($ShouldSign) {
    $DSDir = $env:DSUTILS_DIR
    if (-not $DSDir) {
        $DSDir = "Q:\SteamLibrary\steamapps\common\DayZ Tools\Bin\DSUtils"
        if (-not (Test-Path $DSDir)) {
            $DSDir = "C:\Program Files (x86)\Steam\steamapps\common\DayZ Tools\Bin\DSUtils"
        }
    }
    $DSCreateKey = Join-Path $DSDir "DSCreateKey.exe"
    $DSSignFile  = Join-Path $DSDir "DSSignFile.exe"
    if (-not (Test-Path $DSCreateKey) -or -not (Test-Path $DSSignFile)) {
        throw "DSUtils not found at $DSDir. Install DayZ Tools from Steam, or set `$env:DSUTILS_DIR."
    }

    if (-not (Test-Path $PrivKey)) {
        New-Item -ItemType Directory -Force -Path $KeyDir | Out-Null
        Write-Host ("[build.ps1] Generating new signing key " + $KeyName + " in " + $KeyDir)
        Push-Location $KeyDir
        try {
            $proc = Start-Process -FilePath $DSCreateKey -ArgumentList @($KeyName) `
                -WindowStyle Hidden -PassThru `
                -RedirectStandardOutput (Join-Path $BuildDir "dscreatekey.out.log") `
                -RedirectStandardError  (Join-Path $BuildDir "dscreatekey.err.log")
            $proc | Wait-Process -Timeout 30 -ErrorAction SilentlyContinue
            if (-not $proc.HasExited) { $proc.Kill() }
        } finally { Pop-Location }
        if (-not (Test-Path $PrivKey)) {
            throw "DSCreateKey did not produce $PrivKey"
        }
    }

    foreach ($p in $produced) {
        $pbo = $p.Pbo
        Write-Host ("[build.ps1] Signing " + (Split-Path $pbo -Leaf) + "...")
        $proc = Start-Process -FilePath $DSSignFile `
            -ArgumentList @(("`"" + $PrivKey + "`""), ("`"" + $pbo + "`"")) `
            -WindowStyle Hidden -PassThru `
            -RedirectStandardOutput (Join-Path $BuildDir "dssignfile.out.log") `
            -RedirectStandardError  (Join-Path $BuildDir "dssignfile.err.log")
        $proc | Wait-Process -Timeout 30 -ErrorAction SilentlyContinue
        if (-not $proc.HasExited) { $proc.Kill() }
        $bisign = $pbo + "." + $KeyName + ".bisign"
        if (-not (Test-Path $bisign)) {
            throw "DSSignFile did not produce $bisign"
        }

        $modKeys = Join-Path $p.ModFolder "keys"
        New-Item -ItemType Directory -Force -Path $modKeys | Out-Null
        Copy-Item -Force $PubKey $modKeys
    }
    Write-Host ("[build.ps1] Bundled " + (Split-Path $PubKey -Leaf) + " into the mod folder's keys\")
}
else {
    Write-Host "[build.ps1] (Unsigned PBO. Set USE_SIGN=1 or place a key at tools\keys\$KeyName.biprivatekey to sign.)"
}

if (-not $KeepStaging) {
    Remove-Item -Recurse -Force $StageRoot
}

Write-Host ""
Write-Host "[build.ps1] Mod folders:"
foreach ($p in $produced) {
    $folder = $p.ModFolder
    $pboFile = Get-Item $p.Pbo
    $sizeKb = [math]::Round($pboFile.Length / 1024, 1)
    Write-Host ("  " + (Split-Path $folder -Leaf).PadRight(36) + " " + $sizeKb + " KB")
}
