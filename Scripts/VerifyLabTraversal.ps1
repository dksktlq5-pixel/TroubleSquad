$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$exe='C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$log=Join-Path $root 'Saved/Logs/LabMovementTest.log'
& $exe "$root/TroubleSquad.uproject" /Game/TroubleSquad/Maps/L_SubterraneanLab_Rebuilt -game -nullrhi -nosteam -nosound -unattended -SwarmSkillVisual=0 -SwarmLabTraversalTest -seconds=8 "-abslog=$log" *> $null
if($LASTEXITCODE -ne 0) {throw 'Lab traversal process failed'}
$s=Get-Content $log -Raw
if($s -notmatch 'Lab movement: heroes=108/108 enemies=36/36 waterSafe=1 waterBlocked=1 corrected=1' -or $s -match 'Fatal error:|Assertion failed:|Ensure condition failed:') {throw "Lab traversal acceptance failed: $log"}
Write-Output 'PASS 108 champion crossings, 36 enemy crossings, water blocked and unsafe destinations corrected'
