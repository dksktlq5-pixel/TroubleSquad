param([string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$exe=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$logs=Join-Path $root ('Saved/Logs/GrowthNetwork_'+(Get-Date -Format 'yyyyMMdd_HHmmss'))
New-Item -ItemType Directory -Path $logs | Out-Null
$hostLog=Join-Path $logs 'Host.log'
$clientLog=Join-Path $logs 'Client.log'
$arguments="`"$root/TroubleSquad.uproject`" /Game/TroubleSquad/Maps/L_CoastalResearch?listen?Players=2 -game -nullrhi -nosteam -nosound -unattended -port=7792 -SwarmSkillVisual=0 -VfxReviewDelay=8 -SwarmGrowthNetworkHost -SwarmGrowthNetworkReview -seconds=45 -abslog=`"$hostLog`""
$server=Start-Process $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
$deadline=(Get-Date).AddSeconds(40)
while((Get-Date) -lt $deadline)
{
    if($server.HasExited) { throw 'Growth server exited during startup' }
    if((Test-Path $hostLog) -and (Select-String -LiteralPath $hostLog -Pattern 'listening on port 7792' -Quiet)) { break }
    Start-Sleep -Milliseconds 200
}
if(!(Test-Path $hostLog) -or !(Select-String -LiteralPath $hostLog -Pattern 'listening on port 7792' -Quiet)) { throw "Server readiness not proven, PID=$($server.Id)" }
$arguments="`"$root/TroubleSquad.uproject`" 127.0.0.1:7792?Character=1 -game -nullrhi -nosteam -nosound -unattended -SwarmGrowthNetworkReview -seconds=23 -abslog=`"$clientLog`""
$client=Start-Process $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
foreach($process in @($client,$server)) { if(!$process.WaitForExit(60000)) { throw "Growth process still running: $($process.Id)" }; if($process.ExitCode -ne 0) { throw "Growth process failed: $($process.Id)" } }
$hostText=Get-Content $hostLog -Raw; $clientText=Get-Content $clientLog -Raw
if($hostText -notmatch 'Growth network host: rewards=2' -or $clientText -notmatch 'Growth network client: selected=1 replicated=1 xpCollected=1 augment=15') { throw "Growth replication failed; $logs" }
if(($hostText+$clientText) -match 'Fatal error:|Assertion failed:|Ensure condition failed:|NetworkFailure:') { throw "Growth network runtime error; $logs" }
Write-Output "PASS client selected augment through RPC, replicated ownership, collected shared XP. Local transport only. Logs: $logs"
