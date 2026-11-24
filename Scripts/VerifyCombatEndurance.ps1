param([string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8',[string]$LogDirectory)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$exe=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$logs=if($LogDirectory){$LogDirectory}else{Join-Path $root ('Saved/Logs/Endurance_'+(Get-Date -Format 'yyyyMMdd_HHmmss'))}
New-Item -ItemType Directory -Path $logs -Force | Out-Null
$maps=@('L_CoastalResearch','L_Outskirts','L_SubterraneanLab','L_Beachhead')
$results=@()
foreach($i in 0..3)
{
    $log=Join-Path $logs ($maps[$i]+'.log')
    $arguments="`"$root/TroubleSquad.uproject`" /Game/TroubleSquad/Maps/$($maps[$i]) -game -nullrhi -nosteam -nosound -unattended -SwarmSkillVisual=$i -SwarmCombatEnduranceTest -seconds=210 -abslog=`"$log`""
    $reuse=(Test-Path -LiteralPath $log) -and ((Get-Content -LiteralPath $log -Raw) -match 'Endurance complete:' )
    if(!$reuse) {
        $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
        if(!$process.WaitForExit(240000)) { throw "Endurance still running: PID=$($process.Id), $log" }
        if($process.ExitCode -ne 0) { throw "Endurance process failed: $log" }
    }
    $text=Get-Content -LiteralPath $log -Raw
    if($text -match 'Fatal error:|Assertion failed:|Ensure condition failed:') { throw "Endurance runtime failure: $log" }
    $match=[regex]::Match($text,'Endurance complete: arena=(\d+) character=(\d+) elapsed=([\d.]+) finished=(\d) victory=(\d) peakEnemies=(\d+) peakPickups=(\d+) kills=(\d+)')
    if(!$match.Success -or $text -notmatch 'Swarm final boss spawned:|Swarm timed enemy spawned:.*kind=3') { throw "Boss timeline was not reached: $log" }
    $result=[ordered]@{Map=$maps[$i];Seconds=[double]$match.Groups[3].Value;Victory=$match.Groups[5].Value -eq '1';PeakEnemies=[int]$match.Groups[6].Value;PeakPickups=[int]$match.Groups[7].Value;Kills=[int]$match.Groups[8].Value;Log=$log}
    $results+=[pscustomobject]$result
    Write-Output ($result|ConvertTo-Json -Compress)
}
$results|ConvertTo-Json|Set-Content -LiteralPath "$root/Saved/Logs/EnduranceLatest.json" -Encoding utf8
Write-Output 'Accelerated, headless, invulnerable full-build timeline test. This does not measure normal balance, rendered FPS, or manual playability.'
