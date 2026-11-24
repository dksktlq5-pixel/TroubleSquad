param([ValidateSet(0,1)][int]$Group=0)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$exe='C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$dir=Join-Path $root 'Saved/ReferenceSyncReview';New-Item -ItemType Directory -Path $dir -Force | Out-Null
$cases=@()
foreach($n in 0..8){foreach($s in 0..1){$ordinal=$n*2+$s;if($ordinal%2 -eq $Group){$cases+=@{Name="Champion_${n}_"+@('E','R')[$s];Character=$n;Flags='-ArcadeSkillReview '+$(if($s){'-UltimateOnly'}else{''});Pattern='Skill visual cast accepted=1'}}}}
if($Group -eq 0){$cases+=@{Name='Weapons';Character=1;Flags='-ArcadeVfxGallery';Pattern='VFX gallery: weapons=20'}}
else{$cases+=@{Name='Lifecycle';Character=0;Flags='-ArcadeVfxLifecycleTest -nullrhi';Pattern='VFX lifecycle: budget=1 expired=1 active=0'}}
foreach($case in $cases){
 $log=Join-Path $dir ($case.Name+'.log')
 $args="`"$root/TroubleSquad.uproject`" /Game/TroubleSquad/Maps/L_CoastalResearch_Rebuilt -game -nosteam -nosound -unattended -RenderOffscreen -windowed -ResX=1280 -ResY=720 -ExecCmds=DisableAllScreenMessages -SwarmSkillVisual=$($case.Character) $($case.Flags) -seconds=9 -abslog=`"$log`""
 $p=Start-Process -FilePath $exe -ArgumentList $args -WindowStyle Hidden -PassThru
 if(!$p.WaitForExit(60000)){throw "Still running $($case.Name) PID=$($p.Id); inspect before retry"}
 $s=Get-Content $log -Raw
 if($p.ExitCode -ne 0 -or $s -match 'Fatal error:|Assertion failed:|Ensure condition failed:' -or $s -notmatch $case.Pattern){throw "Failed $($case.Name): $log"}
 if($case.Name -ne 'Lifecycle'){
  $captures=[regex]::Matches($s,'Tracing Screenshot "([^"]+)"');if(!$captures.Count){throw "No capture $($case.Name)"}
  $name=$captures[$captures.Count-1].Groups[1].Value
  Copy-Item -LiteralPath "$root/Saved/Screenshots/WindowsEditor/$name.png" -Destination (Join-Path $dir ($case.Name+'.png'))
 }
 Write-Output "PASS $($case.Name)"
}
