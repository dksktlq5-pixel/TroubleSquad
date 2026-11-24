param([string]$BuildDirectory,[string]$MapName="L_CoastalResearch_Rebuilt")
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (!$BuildDirectory) { $BuildDirectory = Join-Path $projectRoot 'Saved/Builds/Development' }
$gameExe = Join-Path $BuildDirectory 'Windows/TroubleSquad/Binaries/Win64/TroubleSquad.exe'
if (!(Test-Path -LiteralPath $gameExe)) { throw "Packaged executable missing: $gameExe" }
$logDirectory = Join-Path $projectRoot ('Saved/Logs/PackagedCoop_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
$hostLog = Join-Path $logDirectory 'Host.log'
$hostArguments = "/Game/TroubleSquad/Maps/${MapName}?listen?Players=4?Character=0 -port=7789 -nullrhi -nosteam -unattended -nosplash -seconds=30 -abslog=`"$hostLog`" -ExecCmds=`"t.MaxFPS 60`""
$hostProcess = Start-Process -FilePath $gameExe -ArgumentList $hostArguments -WorkingDirectory (Split-Path $gameExe) -WindowStyle Hidden -PassThru
$deadline = (Get-Date).AddSeconds(20)
$ready = $false
while ((Get-Date) -lt $deadline -and !$hostProcess.HasExited) {
    if (Test-Path -LiteralPath $hostLog) {
        $hostText = Get-Content -LiteralPath $hostLog -Raw
        if ($hostText -match 'listening on port 7789') { $ready = $true; break }
    }
    Start-Sleep -Milliseconds 200
}
if (!$ready) { throw "Host readiness not proven. PID=$($hostProcess.Id); log=$hostLog" }
$clients = @()
foreach ($characterIndex in @(1,2,3)) {
    $clientLog = Join-Path $logDirectory "Client$characterIndex.log"
    $clientArguments = "127.0.0.1:7789?Character=$characterIndex -nullrhi -nosteam -unattended -nosplash -seconds=12 -abslog=`"$clientLog`" -ExecCmds=`"t.MaxFPS 60`""
    $clientProcess = Start-Process -FilePath $gameExe -ArgumentList $clientArguments -WorkingDirectory (Split-Path $gameExe) -WindowStyle Hidden -PassThru
    $clients += @{Process=$clientProcess;Log=$clientLog;Character=$characterIndex}
}
foreach ($client in $clients) {
    if (!$client.Process.WaitForExit(45000)) { throw "Client still running: PID=$($client.Process.Id); inspect before retrying." }
    $clientText = Get-Content -LiteralPath $client.Log -Raw
    if ($client.Process.ExitCode -ne 0 -or $clientText -match 'Fatal error:|Assertion failed:|NetworkFailure:') { throw "Client failed: $($client.Log)" }
    if ($clientText -notmatch 'Welcomed by server') { throw "Client join not proven: $($client.Log)" }
    Write-Output "PASS client $($client.Character) joined"
}
if (!$hostProcess.WaitForExit(45000)) { throw "Host still running: PID=$($hostProcess.Id); inspect before retrying." }
$hostText = Get-Content -LiteralPath $hostLog -Raw
if ($hostProcess.ExitCode -ne 0 -or $hostText -match 'Fatal error:|Assertion failed:') { throw "Host failed: $hostLog" }
if ($hostText -notmatch 'District player joined \(Players=4,') { throw "Four-player host state not proven: $hostLog" }
foreach ($characterIndex in @(1,2,3)) {
    if ($hostText -notmatch "District player joined \(Players=\d+, Character=$characterIndex\)") { throw "Character $characterIndex not selected on host: $hostLog" }
}
Write-Output "PASS host + 3 clients, distinct character selections. Logs: $logDirectory"
Write-Output 'Scope: local IP startup and character selection; Steam transport, movement, skills, and full runs need separate verification.'
