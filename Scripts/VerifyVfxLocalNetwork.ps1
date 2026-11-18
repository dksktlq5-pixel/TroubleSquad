param([string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$exe=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$hostLog=Join-Path $root 'Saved/Logs/VfxNetworkHost.log'
$clientLog=Join-Path $root 'Saved/Logs/VfxNetworkClient.log'
$hostArgs="`"$root/TroubleSquad.uproject`" /Game/TroubleSquad/Maps/L_CoastalResearch?listen?Players=2 -game -nullrhi -nosteam -nosound -unattended -port=7788 -SwarmSkillVisual=0 -VfxReviewDelay=2 -VfxBatchNetworkTest -VfxNetworkReview -seconds=28 -abslog=`"$hostLog`""
$hostProcess=Start-Process -FilePath $exe -ArgumentList $hostArgs -WindowStyle Hidden -PassThru
$deadline=(Get-Date).AddSeconds(45)
do {
    if($hostProcess.HasExited) { throw 'VFX listen server exited before startup' }
    $ready=(Test-Path $hostLog) -and (Select-String -LiteralPath $hostLog -Pattern 'Bringing World .*up for play' -Quiet)
    if(!$ready) { Start-Sleep -Milliseconds 200 }
} until($ready -or (Get-Date) -gt $deadline)
if(!$ready) { throw "Host still starting, PID=$($hostProcess.Id); inspect before retrying" }
$clientArgs="`"$root/TroubleSquad.uproject`" 127.0.0.1:7788?Character=0 -game -nullrhi -nosteam -nosound -unattended -VfxNetworkReview -seconds=23 -abslog=`"$clientLog`""
$clientProcess=Start-Process -FilePath $exe -ArgumentList $clientArgs -WindowStyle Hidden -PassThru
if(!$clientProcess.WaitForExit(60000)) { throw "Client still running PID=$($clientProcess.Id)" }
if(!$hostProcess.WaitForExit(60000)) { throw "Host still running PID=$($hostProcess.Id)" }
$hostText=Get-Content -LiteralPath $hostLog -Raw
$clientText=Get-Content -LiteralPath $clientLog -Raw
$tokens=[regex]::Matches($clientText,'VFX network sample received: token=(\d+)') | ForEach-Object { [int]$_.Groups[1].Value } | Sort-Object -Unique
if($hostText -notmatch 'VFX network review queued=40' -or $tokens.Count -ne 40) {
    throw "VFX network delivery failed: received $($tokens.Count)/40; inspect host/client logs"
}
if($hostText -match 'Fatal error:|Assertion failed:' -or $clientText -match 'Fatal error:|Assertion failed:') { throw 'VFX network runtime failure' }
Write-Output 'PASS local listen server/client: 40/40 distinct VFX events delivered. This does not verify Steam relay or separate accounts.'
