$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$exe='C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$logs=Join-Path $root ('Saved/Logs/PickupNetwork_'+(Get-Date -Format yyyyMMdd_HHmmss));New-Item -ItemType Directory -Path $logs | Out-Null
$before=@(Get-ChildItem "$root/Saved/SaveGames" -File -ErrorAction SilentlyContinue | ForEach-Object {$_.Name+':'+(Get-FileHash $_.FullName).Hash}|Sort-Object)
$hostLog=Join-Path $logs 'Host.log';$clientLog=Join-Path $logs 'Client.log'
$hostArgs="`"$root/TroubleSquad.uproject`" /Game/TroubleSquad/Maps/L_CoastalResearch_Rebuilt?listen?Players=2 -game -nullrhi -nosteam -nosound -unattended -port=7793 -SwarmSkillVisual=0 -VfxReviewDelay=8 -SwarmPickupNetworkHost -SwarmPickupNetworkReview -SwarmGrowthNetworkReview -seconds=26 -abslog=`"$hostLog`""
$hostProcess=Start-Process $exe -ArgumentList $hostArgs -WindowStyle Hidden -PassThru
$deadline=(Get-Date).AddSeconds(30)
while((Get-Date) -lt $deadline -and !$hostProcess.HasExited){if((Test-Path $hostLog) -and (Select-String $hostLog -Pattern 'listening on port 7793' -Quiet)){break};Start-Sleep -Milliseconds 200}
if(!(Test-Path $hostLog) -or !(Select-String $hostLog -Pattern 'listening on port 7793' -Quiet)){throw "Host not ready: PID=$($hostProcess.Id)"}
$clientArgs="`"$root/TroubleSquad.uproject`" 127.0.0.1:7793?Character=1 -game -nullrhi -nosteam -nosound -unattended -SwarmPickupNetworkReview -SwarmGrowthNetworkReview -seconds=20 -abslog=`"$clientLog`""
$clientProcess=Start-Process $exe -ArgumentList $clientArgs -WindowStyle Hidden -PassThru
foreach($p in @($clientProcess,$hostProcess)){if(!$p.WaitForExit(60000)){throw "Still running PID=$($p.Id); inspect before retry"};if($p.ExitCode -ne 0){throw "Process failed: PID=$($p.Id)"}}
$h=Get-Content $hostLog -Raw;$c=Get-Content $clientLog -Raw
if($h -notmatch 'Pickup network host: players=2 started=1' -or $h -notmatch 'Pickup network host: sharedPlayers=2 protected=1' -or $c -notmatch 'Pickup network client: vacuum=1 xp=1 gold=1 heal=1' -or $c -notmatch 'Pickup network client: expired=1'){throw "Pickup replication failed: $logs"}
if(($h+$c) -match 'Fatal error:|Assertion failed:|Ensure condition failed:|NetworkFailure:'){throw "Runtime error: $logs"}
$after=@(Get-ChildItem "$root/Saved/SaveGames" -File -ErrorAction SilentlyContinue | ForEach-Object {$_.Name+':'+(Get-FileHash $_.FullName).Hash}|Sort-Object)
if(($before -join '|') -ne ($after -join '|')){throw 'Player save changed'}
[ordered]@{Passed=$true;Transport='Local IP';Players=2;SharedXP=$true;SharedGold=$true;TeamHeal=$true;VacuumReplicated=$true;VacuumExpired=$true;SpecialDropsPreserved=$true;SavesUnchanged=$true;Logs=$logs}|ConvertTo-Json|Set-Content "$root/Saved/Logs/PickupNetworkVerification.json" -Encoding UTF8
Write-Output "PASS team XP/gold/healing, vacuum replication and expiration, protected drops; saves unchanged. Local transport only. $logs"
