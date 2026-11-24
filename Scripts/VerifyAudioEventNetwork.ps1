param([string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$exe=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$logs=Join-Path $root ('Saved/Logs/AudioEventNetwork_'+(Get-Date -Format 'yyyyMMdd_HHmmss'))
New-Item -ItemType Directory -Path $logs -Force | Out-Null
$hostLog=Join-Path $logs 'Host.log';$clientLog=Join-Path $logs 'Client.log'
$arguments="`"$root/TroubleSquad.uproject`" /Game/TroubleSquad/Maps/L_CoastalResearch?listen?Players=2 -game -nullrhi -nosteam -nosound -unattended -port=7798 -SwarmAudioEventNetwork -SwarmGrowthNetworkReview -seconds=30 -abslog=`"$hostLog`""
$server=Start-Process $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
$deadline=(Get-Date).AddSeconds(40)
while((Get-Date) -lt $deadline){
 if($server.HasExited){throw 'Audio server exited during startup'}
 if((Test-Path $hostLog) -and (Select-String $hostLog -Pattern 'listening on port 7798' -Quiet)){break}
 Start-Sleep -Milliseconds 200
}
if(!(Test-Path $hostLog) -or !(Select-String $hostLog -Pattern 'listening on port 7798' -Quiet)){throw 'Audio server not ready'}
$arguments="`"$root/TroubleSquad.uproject`" 127.0.0.1:7798?Character=1 -game -nullrhi -nosteam -nosound -unattended -SwarmAudioEventNetwork -SwarmGrowthNetworkReview -seconds=14 -abslog=`"$clientLog`""
$client=Start-Process $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
foreach($p in @($client,$server)){if(!$p.WaitForExit(60000)){throw "Audio process still running: $($p.Id)"};if($p.ExitCode -ne 0){throw 'Audio network runtime failed'}}
$h=Get-Content $hostLog -Raw;$c=Get-Content $clientLog -Raw
if($c -notmatch 'General audio network: remote=1 impact=0 evolved=1 cue=General/Launcher_WeaponEvolved' -or $c -notmatch 'General audio network: remote=1 impact=1 evolved=1 cue=General/Launcher_ImpactEvolved'){throw "Audio event delivery failed: $logs"}
if(($h+$c) -match 'Fatal error:|Assertion failed:|Ensure condition failed:|NetworkFailure:'){throw "Network error: $logs"}
Write-Output "PASS remote evolved launch and impact cues delivered separately. Local transport only. Logs: $logs"
