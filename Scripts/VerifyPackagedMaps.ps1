param([string]$BuildDirectory)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (!$BuildDirectory) { $BuildDirectory = Join-Path $projectRoot 'Saved/Builds/Development' }
$gameExe = Join-Path $BuildDirectory 'Windows/TroubleSquad/Binaries/Win64/TroubleSquad.exe'
if (!(Test-Path -LiteralPath $gameExe)) { throw "Packaged executable missing: $gameExe" }
$logDirectory = Join-Path $projectRoot 'Saved/Logs/PackagedMapVerification'
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
foreach ($mapName in @('L_SoloMenu','L_CoastalResearch','L_CoastalResearch_Rebuilt','L_Outskirts','L_Outskirts_Rebuilt','L_SubterraneanLab','L_SubterraneanLab_Rebuilt','L_Beachhead')) {
    $logPath = Join-Path $logDirectory "$mapName.log"
    $arguments = "/Game/TroubleSquad/Maps/$mapName -nullrhi -nosteam -unattended -nosplash -seconds=8 -abslog=`"$logPath`""
    $process = Start-Process -FilePath $gameExe -ArgumentList $arguments -WorkingDirectory (Split-Path $gameExe) -WindowStyle Hidden -PassThru
    if (!$process.WaitForExit(60000)) { throw "Map verification is still running: $mapName PID=$($process.Id). Inspect this process before retrying." }
    if (!(Test-Path -LiteralPath $logPath)) { throw "No runtime log: $mapName" }
    $runtimeLog = Get-Content -LiteralPath $logPath -Raw
    if ($process.ExitCode -ne 0 -or $runtimeLog -match 'Fatal error:|Assertion failed:') { throw "Runtime failure in $mapName; see $logPath" }
    if ($runtimeLog -notmatch ('Bringing World .*' + [regex]::Escape($mapName) + '.*up for play')) { throw "Map startup was not proven: $mapName; see $logPath" }
    Write-Output "PASS $mapName (startup only; combat and visual fidelity require separate verification)"
}
