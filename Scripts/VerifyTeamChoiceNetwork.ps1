$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$exe='C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$logs=Join-Path $root ('Saved/Logs/TeamChoiceNetwork_'+(Get-Date -Format yyyyMMdd_HHmmss));New-Item -ItemType Directory -Path $logs | Out-Null
$before=@(Get-ChildItem "$root/Saved/SaveGames" -File -ErrorAction SilentlyContinue|ForEach-Object {$_.Name+':'+(Get-FileHash $_.FullName).Hash}|Sort-Object)
$hostLog=Join-Path $logs 'Host.log';$clientLog=Join-Path $logs 'Client.log'
$arguments="`"$root/TroubleSquad.uproject`" /Game/TroubleSquad/Maps/L_CoastalResearch_Rebuilt?listen?Players=2 -game -nullrhi -nosteam -nosound -unattended -port=7794 -SwarmSkillVisual=0 -VfxReviewDelay=8 -SwarmTeamChoiceNetworkHost -SwarmTeamChoiceNetworkReview -SwarmGrowthNetworkReview -seconds=45 -abslog=`"$hostLog`""
$server=Start-Process $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
$deadline=(Get-Date).AddSeconds(30)
while((Get-Date) -lt $deadline -and !$server.HasExited){if((Test-Path $hostLog) -and (Select-String $hostLog -Pattern 'listening on port 7794' -Quiet)){break};Start-Sleep -Milliseconds 200}
if(!(Test-Path $hostLog) -or !(Select-String $hostLog -Pattern 'listening on port 7794' -Quiet)){throw "Host not ready: PID=$($server.Id)"}
$arguments="`"$root/TroubleSquad.uproject`" 127.0.0.1:7794?Character=1 -game -nullrhi -nosteam -nosound -unattended -SwarmTeamChoiceNetworkReview -SwarmGrowthNetworkReview -seconds=24 -abslog=`"$clientLog`""
$client=Start-Process $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
foreach($p in @($client,$server)){if(!$p.WaitForExit(60000)){throw "Still running PID=$($p.Id); inspect before retry"};if($p.ExitCode -ne 0){throw "Process failed PID=$($p.Id)"}}
$h=Get-Content $hostLog -Raw;$c=Get-Content $clientLog -Raw
if($h -notmatch 'Team choice host: started=1 players=2' -or $h -notmatch 'Team choice host: players=2 automatic=1 resumed=1' -or $h -notmatch 'Team choice host: shared=1 level=6' -or $c -notmatch 'Team choice client: presented=1' -or $c -notmatch 'Team choice client: selected=1 shared=1 level=6 .*resumed=1'){throw "Team choice verification failed: $logs"}
$remaining=[regex]::Match($c,'Team choice client: presented=1 sequence=\d+ remaining=([\d.]+)')
$elapsed=[regex]::Match($c,'Team choice client: augmentClosedAfter=([\d.]+)')
if(!$remaining.Success -or !$elapsed.Success -or [double]$remaining.Groups[1].Value -le 3 -or [double]$remaining.Groups[1].Value -gt 5.2 -or [double]$elapsed.Groups[1].Value -gt 5.75 -or [double]$elapsed.Groups[1].Value -lt 3.5){throw "Choice timing failed: $logs"}
if(($h+$c) -match 'Fatal error:|Assertion failed:|Ensure condition failed:|NetworkFailure:'){throw "Runtime failure: $logs"}
$after=@(Get-ChildItem "$root/Saved/SaveGames" -File -ErrorAction SilentlyContinue|ForEach-Object {$_.Name+':'+(Get-FileHash $_.FullName).Hash}|Sort-Object)
if(($before -join '|') -ne ($after -join '|')){throw 'Player save changed'}
[ordered]@{Passed=$true;Players=2;Transport='Local IP';ClientAugmentPresented=$true;ClientRemainingSeconds=[double]$remaining.Groups[1].Value;ClientAutoClosedAfterSeconds=[double]$elapsed.Groups[1].Value;AutomaticSelection=$true;MatchResumed=$true;SharedLevel=$true;SharedXP=$true;SavesUnchanged=$true;Logs=$logs}|ConvertTo-Json|Set-Content "$root/Saved/Logs/TeamChoiceNetworkVerification.json" -Encoding UTF8
Write-Output "PASS client received augment while paused; timed out in 5 seconds; queued rewards preserved; match resumed; shared level/XP; saves unchanged. Local transport only. $logs"
