param([string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$exe=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe'
$log=Join-Path $root 'Saved/Logs/RenderedCombatPerformance.log'
$arguments="`"$root/TroubleSquad.uproject`" /Game/TroubleSquad/Maps/L_CoastalResearch -game -nosteam -nosound -unattended -windowed -ResX=1600 -ResY=900 -ExecCmds=DisableAllScreenMessages -SwarmSkillVisual=3 -SwarmRenderStressTest -seconds=50 -abslog=`"$log`""
$process=Start-Process $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
if(!$process.WaitForExit(90000)) { throw "Render test still running: PID=$($process.Id)" }
$text=Get-Content -LiteralPath $log -Raw
if($process.ExitCode -ne 0 -or $text -match 'Fatal error:|Assertion failed:|Ensure condition failed:') { throw "Rendered stress test failed: $log" }
$match=[regex]::Match($text,'Render stress complete: samples=(\d+) medianMs=([\d.]+) p95Ms=([\d.]+) averageFPS=([\d.]+) enemies=(\d+) projectiles=(\d+)')
if(!$match.Success) { throw "Rendered stress result missing: $log" }
$result=[ordered]@{Width=1600;Height=900;Samples=[int]$match.Groups[1].Value;MedianMs=[double]$match.Groups[2].Value;P95Ms=[double]$match.Groups[3].Value;AverageFPS=[double]$match.Groups[4].Value;Enemies=[int]$match.Groups[5].Value;Projectiles=[int]$match.Groups[6].Value;Log=$log}
$shots=[regex]::Matches($text,'Tracing Screenshot "([^"]+)"')
if($shots.Count) { Copy-Item -LiteralPath "$root/Saved/Screenshots/WindowsEditor/$($shots[$shots.Count-1].Groups[1].Value).png" -Destination "$root/Saved/VfxReview/RenderedCombatStress.png" }
$result|ConvertTo-Json|Set-Content -LiteralPath "$root/Saved/Logs/RenderedCombatPerformance.json" -Encoding utf8
$result|ConvertTo-Json
if($result.Enemies -lt 100 -or $result.Samples -lt 300) { throw 'Insufficient density or frame samples.' }
if($result.P95Ms -gt 33.34) { throw '95th percentile frame time exceeded the 30 FPS playtest target on this machine.' }
Write-Output 'PASS synthetic rendered stress at 1600x900 on this machine. This does not certify other PCs, Steam, or full-run balance.'
