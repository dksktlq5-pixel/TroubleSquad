param([string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8',[string[]]$Only)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$exe=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$directory=Join-Path $root ('Saved/Logs/GrowthRegression_'+(Get-Date -Format 'yyyyMMdd_HHmmss'))
New-Item -ItemType Directory -Path $directory | Out-Null
$saveDirectory=Join-Path $root 'Saved/SaveGames'
$before=@(Get-ChildItem -LiteralPath $saveDirectory -File -ErrorAction SilentlyContinue | ForEach-Object { $_.Name+':'+(Get-FileHash -LiteralPath $_.FullName).Hash } | Sort-Object)
$cases=@(
    @{Name='AudioEventTiming';Index=0;Flag='SwarmAudioEventTimingTest';Pattern='Audio event timing: empty=1 launch=1 flight=1 impact=1 visualsSilent=1 burnOnce=1 separateBudget=1'},
    @{Name='OriginalAudio';Index=0;Flag='SwarmOriginalAudioTest';Pattern='Original audio: heroes=1 variants=1 music=1 noPop=1'},
    @{Name='EnemyAttackMotions';Index=0;Flag='SwarmEnemyAttackMotionTest';Pattern='Enemy attack motions: loaded=29 changed=29 restored=29 total=29 audio=116 freeze=25'},
    @{Name='EquipmentTooltips';Index=0;Flag='SwarmEquipmentTooltipTest';Pattern='Equipment tooltips: descriptions=1 cooldowns=1 remaining=1 passives=1 permanent=1'},
    @{Name='BossChallenge';Index=0;Flag='SwarmBossChallengeTest';Pattern='Boss challenge: seahorseSize=1 scheduled=1 warning=1 locked=1 recovery=1 damage=1 enrage=1 pattern=1 noEarly=1 once=1 dodge=1 cleanup=1'},
    @{Name='CoopRecovery';Index=0;Flag='SwarmCoopRecoveryTest';Pattern='Coop recovery: stopped=1 golden30=1 spectator=1 progress=1 reset=1 notEarly=1 revived5=1 expired=1'},
    @{Name='Density';Index=0;Flag='SwarmDensityAcceptanceTest';Pattern='Density acceptance: batch=3 scheduled=1 population=80 cap=80 filled=1 bounded=1'},
    @{Name='EnemyMotion';Index=0;Flag='SwarmEnemyMotionTest';Pattern='Enemy motion: upright=1 facing=1 moving=1 animated=1 count=3'},
    @{Name='PlayerFeedback';Index=0;Flag='SwarmPlayerFeedbackTest';Pattern='Player feedback: sizes=1 directions=1 ultimateLocked=1 ultimateUnlocked=1 haste=1 settings=1 rigs=1 footing=1 damage=1 rigCount=25'},
    @{Name='ChampionMechanics';Index=0;Flag='SwarmChampionMechanicsTest';Pattern='Champion mechanics: tentacleDuration=1 directed=1 giantCrit=1 area=1 expired=1 rivenGeneral=1 briarGlobal=1 briarOnce=1'},
    @{Name='Boomerang';Index=0;Flag='SwarmBoomerangFlightTest';Pattern='Boomerang flight: turn=1 return=1 bothLegs=1 split=1 expired=1'},
    @{Name='GeneralFlight';Index=0;Flag='SwarmGeneralProjectileTest';Pattern='General projectile: flight=1 impact=1 bounce=1 rangeExpiry=1 budget=1 pierced=3'},
    @{Name='YuumiVariants';Index=0;Flag='SwarmYuumiVariantsTest';Pattern='Yuumi variants: collectStarted=1 carrying=1 delivered=1 raceStarted=1 race=1 drop=1'},
    @{Name='AugmentPools';Index=0;Flag='SwarmAugmentPoolTest';Pattern='Augment pool acceptance: base=1 story=1 extreme=1 duplicates=1'},
    @{Name='TeamProgress';Index=0;Flag='SwarmTeamProgressTest';Pattern='Team progress: shared=1 singleCredit=1 downed=1 priority=1 fiveSeconds=1 stale=1 automatic=1 snapshot=1 lateJoin=1 resumed=1 curve=1 bulk=1 reviveOffer=1'},
    @{Name='PickupRules';Index=0;Flag='SwarmPickupRulesTest';Pattern='Pickup rules: delayed=1 flight=1 future=1 duration=1 shared=1 preserved=1 expired=1 teamHeal=1 once=1 capped=1 noRevive=1 periodic=1 touch=1 xpBonus=1 goldHeal=1 goldNoRevive=1 regen=1'},
    @{Name='WorldLoot';Index=0;Flag='SwarmWorldLootTest';Pattern='World loot acceptance: weights=1 crateHit=1 crateDestroyed=1 magnet=1 bomb=1 car=1 duration=1 expired=1 unlocks=1'},
    @{Name='BoonSpire';Index=0;Flag='SwarmBoonSpireTest';Pattern='Boon spire acceptance: started=1 outside=1 success=1 timeout=1 rewards=1'},
    @{Name='PermanentUpgrades';Index=0;Flag='SwarmPermanentUpgradeTest';Pattern='Permanent upgrade acceptance: caps=1 invalid=1 stats=1 replay=1 locked=1 unlocked=1'},
    @{Name='EncounterTuning';Index=0;Flag='SwarmEncounterTuningTest';Pattern='Encounter tuning: stages=1 team=1 bossHealth=1 cap=180'},
    @{Name='Augments';Index=0;Flag='SwarmAugmentAcceptanceTest';Pattern='Augment acceptance: catalog=32/32 glass=1 spray=1 ultimate=1 cards=1 levels=1 critical=1 three=1 queue=1 replay=1 cap=1 xpPersistent=1'},
    @{Name='Yuumi';Index=0;Flag='SwarmYuumiAcceptanceTest';Pattern='Yuumi acceptance: started=1 outside=1 complete=1 timeout=1 rewards=1'},
    @{Name='Growth';Index=0;Flag='SwarmGrowthSmokeTest';Pattern='Growth smoke: projectiles=1 cappedRejected=1 finalRank=1 fallback=1 gold=1'},
    @{Name='Evolution';Index=0;Flag='SwarmEvolutionSmokeTest';Pattern='Signature evolution smoke total: 9/9'},
    @{Name='Slots';Index=0;Flag='SwarmDamageSlotsSmokeTest';Pattern='Damage slots smoke: sharedDamage=1 seventhRejected=1 existingUpgrade=1 pool=1'},
    @{Name='ProjectileCounts';Index=0;Flag='SwarmProjectileIntegrationTest';Pattern='Projectile integration: passed=120 total=120'}
)
foreach($i in 0..8) { $cases+=@{Name="Cooldown$i";Index=$i;Flag='SwarmCooldownSmokeTest';Pattern="Cooldown cast smoke: character=$i .*pass=1"} }
if($Only) { $cases=@($cases | Where-Object {$Only -contains $_.Name}); if(!$cases.Count) { throw 'No matching regression cases.' } }
foreach($case in $cases)
{
    $log=Join-Path $directory ($case.Name+'.log')
    $arguments="`"$root/TroubleSquad.uproject`" /Game/TroubleSquad/Maps/L_CoastalResearch -game -nullrhi -nosteam -nosound -unattended -SwarmSkillVisual=$($case.Index) -$($case.Flag) -seconds=9 -abslog=`"$log`""
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(!$process.WaitForExit(60000)) { throw "Test still running: $($case.Name), PID=$($process.Id)" }
    $content=Get-Content -LiteralPath $log -Raw
    if($process.ExitCode -ne 0 -or $content -match 'Fatal error:|Assertion failed:|Ensure condition failed:') { throw "Runtime failure: $($case.Name), $log" }
    if($content -notmatch $case.Pattern) { throw "Acceptance failure: $($case.Name), $log" }
    Write-Output "PASS $($case.Name)"
}
$after=@(Get-ChildItem -LiteralPath $saveDirectory -File -ErrorAction SilentlyContinue | ForEach-Object { $_.Name+':'+(Get-FileHash -LiteralPath $_.FullName).Hash } | Sort-Object)
if(($before -join '|') -ne ($after -join '|')) { throw 'Regression changed a player save file.' }
Write-Output "PASS $($cases.Count) cases; player saves unchanged; logs: $directory"
