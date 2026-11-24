param([string[]]$Only,[switch]$Force)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$exe='C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
$cases=@(
 @{Name='Lobby';Map='L_SoloMenu';Flags='-ClientUISmokeTest'},
 @{Name='SettingsLobby';Map='L_SoloMenu';Flags='-ClientUISmokeTest -SettingsVisualReview'},
 @{Name='MapSelection';Map='L_SoloMenu';Flags='-ClientUISmokeTest -MapSelectionReview'},
 @{Name='ChampionSelection';Map='L_SoloMenu';Flags='-ClientUISmokeTest -ChampionSelectionReview'},
 @{Name='Upgrades';Map='L_SoloMenu';Flags='-ClientUISmokeTest -UpgradeVisualReview'},
 @{Name='PlayerFeedback';Map='L_CoastalResearch';Flags='-SwarmSkillVisual=0 -SwarmPlayerFeedbackTest'},
 @{Name='SettingsCombat';Map='L_CoastalResearch';Flags='-SwarmSkillVisual=0 -SwarmPlayerFeedbackTest -SettingsVisualReview'}
)
foreach($map in 'CoastalResearch','Outskirts','SubterraneanLab','Beachhead'){$cases+=@{Name="Map_$map";Map="L_$map";Flags='-SwarmSkillVisual=0 -ArenaLayoutReview'}}
if($Only){$cases=@($cases|Where-Object {$Only -contains $_.Name})}
$dir="$root/Saved/FeedbackReview";New-Item -ItemType Directory -Force -Path $dir|Out-Null
foreach($case in $cases){
 $log="$dir/$($case.Name).log"
 $args="`"$root/TroubleSquad.uproject`" /Game/TroubleSquad/Maps/$($case.Map) -game -nosteam -nosound -unattended -windowed -ResX=1600 -ResY=900 -ExecCmds=DisableAllScreenMessages $($case.Flags) -seconds=9 -abslog=`"$log`""
 $reuse=(!$Force) -and (Test-Path $log) -and (Get-Item $log).LastWriteTime -gt (Get-Item "$root/Binaries/Win64/UnrealEditor-TroubleSquad.dll").LastWriteTime
 if($reuse){$reuse=(Get-Content $log -Raw) -match '(?s)Tracing Screenshot .*Log file closed,'}
 if(!$reuse){
  $p=Start-Process $exe -ArgumentList $args -WindowStyle Hidden -PassThru
  if(!$p.WaitForExit(60000)){if(!$p.WaitForExit(60000)){throw "Review still live: PID=$($p.Id)"}}
  if($p.ExitCode -ne 0){throw "Review process failed: $($case.Name)"}
 }
 $text=Get-Content $log -Raw
 if($text -match 'Fatal error:|Assertion failed:'){throw "Review crashed: $($case.Name)"}
 $matches=[regex]::Matches($text,'Tracing Screenshot "([^"]+)"')
 if(!$matches.Count){throw "No capture: $($case.Name)"}
 Copy-Item "$root/Saved/Screenshots/WindowsEditor/$($matches[-1].Groups[1].Value).png" "$dir/$($case.Name).png"
 if($case.Name -eq 'PlayerFeedback' -and $text -notmatch 'Player feedback: sizes=1 directions=1 ultimateLocked=1 ultimateUnlocked=1 haste=1 settings=1 rigs=1 footing=1 damage=1 rigCount=25'){throw 'Player feedback acceptance failed'}
 Write-Output "PASS $($case.Name) runtime/capture. Visual inspection required."
}
