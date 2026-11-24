#include "AnimaPreview.h"
#include "GameFramework/PlayerController.h"
#include "SwarmEncounter.h"
#include "SwarmArenas.h"
#include "SwarmEncounterTuning.h"
#include "DistrictProgress.h"
#include "SwarmLoot.h"
#include "CombatAudio.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#if WITH_EDITOR
void AAnimaPreviewCharacter::RunChampionMechanicsTest()
{
    for(TActorIterator<ASwarmEnemy> It(GetWorld());It;++It) { It->Destroy(); }
    ApplyCharacterChoice(3); SignatureWeaponLevel=1; bSignatureWeaponEvolved=false;
    RunWeaponStats.Init(0,8); OwnedAugments.Reset(); PermanentDamageMultiplier=1.f; PermanentDurationMultiplier=2.f; PermanentAreaMultiplier=1.f;
    CombatHazards.Reset(); TentacleVisuals.Reset(); InvulnerableEnd=GetCombatTime()+60.f;
    const float Now=GetCombatTime(); const FVector Origin=GetActorLocation();
    SpawnTentacle(Origin,Now);
    const bool Duration=CombatHazards.Num()==1&&FMath::IsNearlyEqual(CombatHazards[0].End-Now,16.f,.01f);
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Front=GetWorld()->SpawnActor<ASwarmEnemy>(Origin+FVector(400,0,0),FRotator::ZeroRotator,Spawn);
    auto* Side=GetWorld()->SpawnActor<ASwarmEnemy>(Origin+FVector(0,400,0),FRotator::ZeroRotator,Spawn);
    auto* Behind=GetWorld()->SpawnActor<ASwarmEnemy>(Origin-FVector(400,0,0),FRotator::ZeroRotator,Spawn);
    const float SideHealth=Side?Side->GetHealth():0.f,BackHealth=Behind?Behind->GetHealth():0.f;
    if(!CombatHazards.IsEmpty()) { CombatHazards[0].Direction=FVector::ForwardVector; CombatHazards[0].bAimLocked=true; CombatHazards[0].NextTick=Now; }
    TickAdditionalCombat(Now,.016f);
    const bool Directed=Front&&Front->GetHealth()<=0.f&&Side&&Behind&&FMath::IsNearlyEqual(Side->GetHealth(),SideHealth)&&FMath::IsNearlyEqual(Behind->GetHealth(),BackHealth);
    for(auto* Target:{Front,Side,Behind}) { if(IsValid(Target)) { Target->Destroy(); } }
    CombatHazards.Reset(); TentacleVisuals.Reset(); bSignatureWeaponEvolved=true; SignatureWeaponLevel=5; RunWeaponStats[5]=20;
    SpawnTentacle(Origin,Now);
    const bool Giant=TentacleVisuals.Num()==1&&TentacleVisuals[0].bGiant;
    PermanentAreaMultiplier=1.5f; TickAdditionalCombat(Now+.01f,.016f);
    const bool Area=CombatHazards.Num()==1&&FMath::IsNearlyEqual(CombatHazards[0].Radius,2100.f,.01f);
    TickAdditionalCombat(Now+33.f,.016f);
    const bool Expired=CombatHazards.IsEmpty()&&TentacleVisuals.IsEmpty();
    ApplyCharacterChoice(6); RunWeaponStats.Init(0,8); PermanentDamageMultiplier=PermanentDurationMultiplier=1.f; RivenShield=0.f; RivenShieldEnd=0.f;
    auto* Target=GetWorld()->SpawnActor<ASwarmEnemy>(GetActorLocation()+FVector(100,0,0),FRotator::ZeroRotator,Spawn);
    const float Before=Target?Target->GetHealth():0.f;
    ApplyGeneralWeaponDamage(Target,8.f,static_cast<uint8>(EGeneralWeapon::Launcher),false,false);
    const float Applied=Target?Before-Target->GetHealth():0.f;
    const bool Rune=Applied>8.f&&FMath::IsNearlyEqual(RivenShield,FMath::Min(Applied*.25f,MaxCombatHealth*.02f),.01f);
    if(IsValid(Target)) { Target->Destroy(); }
    ApplyCharacterChoice(4); RunWeaponStats.Init(0,8); PermanentDamageMultiplier=1.f; CombatHealth=MaxCombatHealth*.5f; bSignatureWeaponEvolved=false;
    const bool BriarMultiplier=FMath::IsNearlyEqual(GetCombatDamageMultiplier(),1.3f,.01f);
    FTransform Transform(GetActorLocation()+FVector(150,0,0));
    Target=GetWorld()->SpawnActorDeferred<ASwarmEnemy>(ASwarmEnemy::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if(Target) { Target->Kind=2; Target->FinishSpawning(Transform); }
    SetActorRotation(FRotator::ZeroRotator); const float Health=Target?Target->GetHealth():0.f;
    FireAdditionalWeapon(GetCombatTime());
    const float Expected=(25.f+.05f*MaxCombatHealth)*1.3f;
    const bool BriarOnce=Target&&FMath::IsNearlyEqual(Health-Target->GetHealth(),Expected,.02f);
    if(IsValid(Target)) { Target->Destroy(); }
    UE_LOG(LogTemp,Display,TEXT("Champion mechanics: tentacleDuration=%d directed=%d giantCrit=%d area=%d expired=%d rivenGeneral=%d briarGlobal=%d briarOnce=%d"),Duration,Directed,Giant,Area,Expired,Rune,BriarMultiplier,BriarOnce);
}

void AAnimaPreviewCharacter::RunBoomerangFlightTest()
{
    for(TActorIterator<ASwarmEnemy> It(GetWorld());It;++It) { It->Destroy(); }
    const uint8 Weapon=static_cast<uint8>(EGeneralWeapon::Boomerang);
    const FVector Origin=GetActorLocation();
    GeneralWeaponRanks.Init(1,GeneralWeaponCount); EvolvedGeneralWeapons.Reset(); GeneralProjectiles.Reset();
    LaunchGeneralProjectile(Weapon,Origin,Origin+FVector(300,0,0),30.f,8.f,false);
    TickGeneralProjectiles(GetCombatTime(),.2f);
    const bool Turn=GeneralProjectiles.Num()==1&&GeneralProjectiles[0].bReturning;
    SetActorLocation(Origin+FVector(0,120,0)); TickGeneralProjectiles(GetCombatTime()+.2f,.2f);
    const bool Return=GeneralProjectiles.IsEmpty(); SetActorLocation(Origin);
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Enemy=GetWorld()->SpawnActor<ASwarmEnemy>(Origin+FVector(200,0,0),FRotator::ZeroRotator,Spawn);
    LaunchGeneralProjectile(Weapon,Origin,Origin+FVector(400,0,0),30.f,8.f,false);
    TickGeneralProjectiles(GetCombatTime(),.3f);
    const float BeforeReturn=Enemy?Enemy->GetHealth():0.f;
    TickGeneralProjectiles(GetCombatTime()+.3f,.3f);
    const bool BothLegs=Enemy&&Enemy->GetHealth()<BeforeReturn;
    if(Enemy) { Enemy->Destroy(); }
    GeneralProjectiles.Reset(); EvolvedGeneralWeapons.Add(Weapon);
    Enemy=GetWorld()->SpawnActor<ASwarmEnemy>(Origin+FVector(200,0,0),FRotator::ZeroRotator,Spawn);
    LaunchGeneralProjectile(Weapon,Origin,Origin+FVector(800,0,0),30.f,8.f,false);
    TickGeneralProjectiles(GetCombatTime(),.2f);
    bool Split=GeneralProjectiles.Num()==3;
    for(const auto& P:GeneralProjectiles) { Split &= !P.bCanSplit&&P.bFirstOnly; }
    TickGeneralProjectiles(GetCombatTime()+.3f,.3f);
    const bool Expired=GeneralProjectiles.IsEmpty();
    if(Enemy) { Enemy->Destroy(); }
    UE_LOG(LogTemp,Display,TEXT("Boomerang flight: turn=%d return=%d bothLegs=%d split=%d expired=%d"),Turn,Return,BothLegs,Split,Expired);
}

void AAnimaPreviewCharacter::RunAugmentPoolTest()
{
    OwnedAugments.Reset(); bStoryAugmentsUnlocked=bExtremeAugmentsUnlocked=false;
    const bool Base=GetAvailableAugmentChoices().Num()==18;
    bStoryAugmentsUnlocked=true; const bool Story=GetAvailableAugmentChoices().Num()==26;
    bExtremeAugmentsUnlocked=true; const bool Extreme=GetAvailableAugmentChoices().Num()==32;
    OwnedAugments.Add(0); const auto Pool=GetAvailableAugmentChoices();
    const bool Duplicate=Pool.Num()==31&&!Pool.Contains(FirstAugmentChoice);
    OwnedAugments.Reset(); bStoryAugmentsUnlocked=bExtremeAugmentsUnlocked=false;
    UE_LOG(LogTemp,Display,TEXT("Augment pool acceptance: base=%d story=%d extreme=%d duplicates=%d"),Base,Story,Extreme,Duplicate);
}

void AAnimaPreviewCharacter::RunWorldLootAcceptanceTest()
{
    InvulnerableEnd=GetCombatTime()+120.f;
    const bool Weights=SwarmLoot::CrateDrop(.01f)==SwarmLoot::Magnet&&SwarmLoot::CrateDrop(.06f)==SwarmLoot::Bomb
        &&SwarmLoot::CrateDrop(.2f)==6&&SwarmLoot::CrateDrop(.6f)==5&&SwarmLoot::CrateDrop(.95f)==INDEX_NONE;
    auto* Crate=GetWorld()->SpawnActor<ASwarmLootCrate>(GetActorLocation()+FVector(300,0,0),FRotator::ZeroRotator);
    if(Crate) { UGameplayStatics::ApplyDamage(Crate,32.f,GetController(),this,UDamageType::StaticClass()); }
    const bool Hit=Crate&&FMath::IsNearlyEqual(Crate->GetMissingHealthFraction_Implementation(),.5f);
    if(Crate) { UGameplayStatics::ApplyDamage(Crate,32.f,GetController(),this,UDamageType::StaticClass()); }
    const bool Destroyed=Crate&&Crate->IsActorBeingDestroyed();
    for(TActorIterator<ASwarmExperienceOrb> It(GetWorld());It;++It) { It->Destroy(); }
    const int32 BeforeXP=Experience,BeforeLevel=CombatLevel,BeforeGold=RunGold;
    GetWorld()->SpawnActor<ASwarmExperienceOrb>(GetActorLocation()+FVector(3000,0,0),FRotator::ZeroRotator);
    ASwarmExperienceOrb::SpawnGold(GetWorld(),GetActorLocation()+FVector(3000,100,0));
    CollectWorldPowerup(SwarmLoot::Magnet);
    for(int32 Step=0;Step<100;++Step)
    { for(TActorIterator<ASwarmExperienceOrb> It(GetWorld());It;++It) { if(It->CanBeVacuumed()) { It->Tick(.05f); } } }
    const bool Magnet=(Experience!=BeforeXP||CombatLevel>BeforeLevel)&&RunGold>BeforeGold;
    LevelChoices.Reset(); PendingLevelChoices=PendingAccessCards=0; EndLevelChoiceSelection();
    auto* Enemy=GetWorld()->SpawnActor<ASwarmEnemy>(GetActorLocation()+FVector(350,0,0),FRotator::ZeroRotator);
    CollectWorldPowerup(SwarmLoot::Bomb);
    const bool Bomb=Enemy&&Enemy->GetHealth()<=0.f;
    PermanentBoonMultiplier=1.5f; InvulnerableEnd=0.f;
    CollectWorldPowerup(SwarmLoot::Supercar); UpdateSupercarVisual();
    const bool Car=SupercarVisual&&SupercarVisual->IsVisible()&&bSupercarMeshHidden&&HasTimedInvulnerability(GetCombatTime());
    const bool Duration=FMath::IsNearlyEqual(GetWorldBoonRemaining(),22.5f,.01f);
    WorldBoonEnd=GetCombatTime()-1.f; UpdateSupercarVisual();
    const bool Expired=!bSupercarMeshHidden&&GetMesh()->IsVisible()&&!HasTimedInvulnerability(GetCombatTime());
    auto* Progress=NewObject<UDistrictProgress>(); Progress->TotalBombPickups=10; Progress->bRodeSupercar=true; Progress->MostBoonsInRun=4; Progress->bHardAatroxDefeated=true;
    const bool Unlocks=Progress->IsUpgradeUnlocked(6)&&Progress->IsUpgradeUnlocked(7)&&Progress->IsUpgradeUnlocked(13)&&Progress->IsUpgradeUnlocked(14);
    UE_LOG(LogTemp,Display,TEXT("World loot acceptance: weights=%d crateHit=%d crateDestroyed=%d magnet=%d bomb=%d car=%d duration=%d expired=%d unlocks=%d"),Weights,Hit,Destroyed,Magnet,Bomb,Car,Duration,Expired,Unlocks);
}

void AAnimaPreviewCharacter::RunPermanentUpgradeTest()
{
    bPermanentRanksApplied=false; OwnedAugments.Reset(); RunHealthBonus=RunArmorBonus=0.f;
    const float BaseHP=MaxCombatHealth,Armor=BaseArmor,Speed=BaseMoveSpeed;
    TArray<uint8> Ranks; Ranks.Init(0,15); Ranks[0]=255;
    ServerApplyPermanentRanks_Implementation(Ranks);
    const bool Invalid=!bPermanentRanksApplied&&FMath::IsNearlyEqual(BaseHP,MaxCombatHealth);
    bool Caps=true; for(int32 I=0;I<15;++I) { Caps &= UDistrictProgress::UpgradeCap(I)>0; Ranks[I]=UDistrictProgress::UpgradeCap(I); }
    ServerApplyPermanentRanks_Implementation(Ranks);
    const bool Stats=bPermanentRanksApplied&&FMath::IsNearlyEqual(MaxCombatHealth,BaseHP*1.5f)
        &&FMath::IsNearlyEqual(BaseArmor,Armor+25.f)&&FMath::IsNearlyEqual(BaseMoveSpeed,Speed*2.2f)
        &&FMath::IsNearlyEqual(PermanentDamageMultiplier,2.8f)&&FMath::IsNearlyEqual(PermanentAbilityHaste,120.f)
        &&FMath::IsNearlyEqual(PermanentRegen,15.f)&&FMath::IsNearlyEqual(PermanentCritChance,.2f)
        &&PermanentProjectiles==2&&FMath::IsNearlyEqual(PermanentPickupMultiplier,2.75f)
        &&FMath::IsNearlyEqual(PermanentGoldMultiplier,1.45f)&&FMath::IsNearlyEqual(PermanentBoonMultiplier,1.5f);
    Ranks.Init(0,15); ServerApplyPermanentRanks_Implementation(Ranks);
    const bool Replay=FMath::IsNearlyEqual(PermanentDamageMultiplier,2.8f);
    auto* Progress=NewObject<UDistrictProgress>();
    const bool Locked=!Progress->IsUpgradeUnlocked(3)&&!Progress->IsUpgradeUnlocked(5)&&!Progress->IsUpgradeUnlocked(14);
    Progress->TotalHealthPacks=3; Progress->TotalExperienceOrbs=10000; Progress->TotalCriticalHits=40000;
    Progress->TotalSkillsUsed=200; Progress->HighestLevel=30; Progress->HighestSeraphineLevel=25; Progress->TotalGoldEarned=5000;
    const bool Unlocked=Progress->IsUpgradeUnlocked(3)&&Progress->IsUpgradeUnlocked(5)&&Progress->IsUpgradeUnlocked(8)
        &&Progress->IsUpgradeUnlocked(9)&&Progress->IsUpgradeUnlocked(10)&&Progress->IsUpgradeUnlocked(11)&&Progress->IsUpgradeUnlocked(12);
    UE_LOG(LogTemp,Display,TEXT("Permanent upgrade acceptance: caps=%d invalid=%d stats=%d replay=%d locked=%d unlocked=%d"),Caps,Invalid,Stats,Replay,Locked,Unlocked);
}

void AAnimaPreviewCharacter::RunEncounterTuningTest()
{
    bool Stages=true;
    const float Times[]={0.f,79.9f,80.f,239.9f,240.f,399.9f,400.f,600.f};
    float LastHealth=0.f,LastDamage=0.f,LastInterval=100.f; int32 LastCap=0;
    for(float Time:Times)
    {
        const auto S=GetSwarmEncounterStage(Time);
        Stages &= S.Health>=LastHealth&&S.Damage>=LastDamage&&S.Interval<=LastInterval&&S.PopulationCap>=LastCap;
        LastHealth=S.Health; LastDamage=S.Damage; LastInterval=S.Interval; LastCap=S.PopulationCap;
    }
    const bool Team=FMath::IsNearlyEqual(GetSwarmTeamHealthMultiplier(0),1.f)
        &&FMath::IsNearlyEqual(GetSwarmTeamHealthMultiplier(4),2.95f,.0001f)
        &&FMath::IsNearlyEqual(GetSwarmTeamHealthMultiplier(99),2.95f,.0001f);
    const FTransform Transform(GetActorLocation()+FVector(800.f,0.f,0.f));
    auto* Boss=GetWorld()->SpawnActorDeferred<ASwarmEnemy>(ASwarmEnemy::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if(Boss) { Boss->Kind=3; Boss->FinishSpawning(Transform); }
    const bool BossHealth=Boss&&Boss->GetMaxHealth()>=54000.f&&FMath::IsNearlyEqual(Boss->GetHealth(),Boss->GetMaxHealth());
    if(Boss) { Boss->Destroy(); }
    UE_LOG(LogTemp,Display,TEXT("Encounter tuning: stages=%d team=%d bossHealth=%d cap=%d"),Stages,Team,BossHealth,GetSwarmEncounterStage(900.f).PopulationCap);
}

void AAnimaPreviewCharacter::TickGrowthNetworkTest()
{
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmAudioEventNetwork")))
    {
        if(HasAuthority()&&GetController()&&!bGrowthNetworkReported&&GetCombatTime()>3.f)
        {
            if(AudioNetworkReadyAt==0.f){AudioNetworkReadyAt=GetCombatTime()+2.f;}
            if(GetCombatTime()>=AudioNetworkReadyAt)
            {
                bGrowthNetworkReported=true;const uint8 W=static_cast<uint8>(EGeneralWeapon::Launcher);
                EvolvedGeneralWeapons.AddUnique(W);GeneralWeaponRanks.Init(5,GeneralWeaponCount);
                EmitGeneralWeaponAudio(W,GetActorLocation(),false);EmitGeneralWeaponAudio(W,GetActorLocation()+FVector(100,0,0),true);
            }
        }
        return;
    }

    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmEnemyAttackNetwork")))
    {
        if(HasAuthority()&&GetController()&&GetCombatTime()>3.f&&!bGrowthNetworkReported)
        {
            bGrowthNetworkReported=true;const FTransform T(GetActorLocation()+FVector(500,0,0));auto* Probe=GetWorld()->SpawnActorDeferred<ASwarmEnemy>(ASwarmEnemy::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);Probe->Kind=1;Probe->ModelVariant=0;Probe->FinishSpawning(T);Probe->NextAttack=GetCombatTime()+60.f;Probe->GetCharacterMovement()->DisableMovement();Probe->TriggerAttackPose(GetCombatTime()+6.f,6.f);
        }
        if(!HasAuthority()&&IsLocallyControlled()&&!bGrowthNetworkReported)
        {for(TActorIterator<ASwarmEnemy> It(GetWorld());It;++It) {auto* Node=It->GetMesh()->GetSingleNodeInstance();if(It->AttackMotionImpactAt>GetCombatTime()&&Node&&It->AttackSequence&&Node->GetCurrentAsset()==It->AttackSequence.Get()) {bGrowthNetworkReported=true;UE_LOG(LogTemp,Display,TEXT("Enemy attack client: synchronized=1 pose=1"));break;}}}
        return;
    }

    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmEquipmentCooldownNetwork")))
    {
        if(HasAuthority()&&GetController()&&GetCombatTime()>3.f&&GetGeneralWeaponRank(0)==0)
        { UpgradeGeneralWeapon(0);GeneralWeaponReadyAt[0]=GetCombatTime()+12.f;ForceNetUpdate(); }
        if(!HasAuthority()&&IsLocallyControlled()&&!bGrowthNetworkReported&&GetGeneralWeaponRemaining(0)>1.f)
        { bGrowthNetworkReported=true;UE_LOG(LogTemp,Display,TEXT("Equipment cooldown client: rank=%d remaining=%.2f tooltip=%d"),GetGeneralWeaponRank(0),GetGeneralWeaponRemaining(0),!GetGeneralWeaponTooltip(0).IsEmpty()); }
        return;
    }

    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmTeamChoiceNetworkReview"))) { return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmPickupNetworkReview")))
    {
        if(HasAuthority()) { InvulnerableEnd=GetCombatTime()+120.f; }
        if(!HasAuthority()&&IsLocallyControlled())
        {
            bool TeamVacuum=false;
            for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It) { TeamVacuum|=It->GetVacuumRemaining()>0.f; }
            bPickupNetworkSawVacuum|=TeamVacuum;
            if(!bPickupNetworkReported&&bPickupNetworkSawVacuum&&RunGold>=3&&Experience>0&&CombatHealth>=400.f)
            {
                bPickupNetworkReported=true;
                UE_LOG(LogTemp,Display,TEXT("Pickup network client: vacuum=1 xp=1 gold=1 heal=1 health=%.1f"),CombatHealth);
            }
            if(bPickupNetworkReported&&!bPickupNetworkExpired&&!TeamVacuum)
            { bPickupNetworkExpired=true;UE_LOG(LogTemp,Display,TEXT("Pickup network client: expired=1")); }
        }
        return;
    }
    if(!FParse::Param(FCommandLine::Get(),TEXT("SwarmGrowthNetworkReview"))) { return; }
    if(HasAuthority()) { InvulnerableEnd=GetCombatTime()+120.f; }
    if(IsLocallyControlled()&&!LevelChoices.IsEmpty()) { SelectLevelCard(0); }
    if(!HasAuthority()&&IsLocallyControlled()&&!bGrowthNetworkReported&&OwnedAugments.Num()==1&&Experience>=4)
    {
        bGrowthNetworkReported=true;
        UE_LOG(LogTemp,Display,TEXT("Growth network client: selected=1 replicated=1 xpCollected=1 augment=%d"),OwnedAugments[0]);
    }
}

void AAnimaPreviewCharacter::StartCombatEnduranceTest()
{
    bEnduranceRunning=true; EnduranceStartedAt=GetCombatTime();
    EnduranceOrigin=GetActorLocation(); EnduranceMoveGoal=EnduranceOrigin;
    InvulnerableEnd=EnduranceStartedAt+1200.f;
    SignatureWeaponLevel=5; bSignatureWeaponEvolved=true;
    GeneralWeapons={static_cast<uint8>(EGeneralWeapon::Launcher),static_cast<uint8>(EGeneralWeapon::Crossbow),
        static_cast<uint8>(EGeneralWeapon::IceArmor),static_cast<uint8>(EGeneralWeapon::Tibber)};
    GeneralWeaponRanks.Init(5,GeneralWeaponCount); EvolvedGeneralWeapons=GeneralWeapons;
    RunWeaponStats.Init(5,8); RunArmorBonus=40.f; RunRegen=20.f;
    UGameplayStatics::SetGlobalTimeDilation(this,8.f);
    UE_LOG(LogTemp,Display,TEXT("Endurance started: character=%d timeScale=8 invulnerable=1 fullBuild=1"),CharacterChoice);
}

void AAnimaPreviewCharacter::TickCombatEnduranceTest(float DeltaTime)
{
    if(!bEnduranceRunning||!HasAuthority()) { return; }
    const float Now=GetCombatTime();
    if(!LevelChoices.IsEmpty()) { ServerSelectLevelChoice_Implementation(0,LevelChoiceSequence); }
    if(Now>=EnduranceNextSampleAt)
    {
        EnduranceNextSampleAt=Now+30.f;
        int32 Enemies=0,Pickups=0;
        for(TActorIterator<ASwarmEnemy> It(GetWorld());It;++It) { ++Enemies; }
        for(TActorIterator<ASwarmExperienceOrb> It(GetWorld());It;++It) { ++Pickups; }
        EndurancePeakEnemies=FMath::Max(EndurancePeakEnemies,Enemies);
        EndurancePeakPickups=FMath::Max(EndurancePeakPickups,Pickups);
        UE_LOG(LogTemp,Display,TEXT("Endurance sample: elapsed=%.1f enemies=%d pickups=%d level=%d projectiles=%d"),
            Now-EnduranceStartedAt,Enemies,Pickups,CombatLevel,GeneralProjectiles.Num());
    }
    if(bDistrictFinished||Now-EnduranceStartedAt>=1020.f)
    {
        bEnduranceRunning=false;
        UE_LOG(LogTemp,Display,TEXT("Endurance complete: arena=%d character=%d elapsed=%.1f finished=%d victory=%d peakEnemies=%d peakPickups=%d kills=%d"),
            SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this)),CharacterChoice,Now-EnduranceStartedAt,
            bDistrictFinished,bDistrictCleared,EndurancePeakEnemies,EndurancePeakPickups,EnemyKills);
        FPlatformMisc::RequestExit(false);
        return;
    }
    const int32 Arena=SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this));
    if(Arena==2 && Now-EnduranceStartedAt>SwarmRound::BossAt-5.f)
    {
        if(UGameplayStatics::GetCurrentLevelName(this)==TEXT("L_SubterraneanLab_Rebuilt"))
        {
            const FVector P=GetActorLocation();
            EnduranceMoveGoal=P.X<1900.f?FVector(1950.f,-560.f,P.Z):P.X<2150.f?FVector(2200.f,0.f,P.Z):P.X<3150.f?FVector(3250.f,0.f,P.Z):
                P.Y>-500.f?FVector(4300.f,-800.f,P.Z):FVector(4300.f,-1150.f,P.Z);
        }
        else if(GetActorLocation().X<3600.f)
        { EnduranceMoveGoal=FVector(3800.f,GetActorLocation().X<2800.f?-560.f:-1150.f,GetActorLocation().Z); }
    }
    const FVector MoveDirection=(EnduranceMoveGoal-GetActorLocation()).GetSafeNormal2D();
    if(FVector::DistSquared2D(EnduranceMoveGoal,GetActorLocation())>FMath::Square(55.f)) { AddMovementInput(MoveDirection,1.f,true); }
    if(Now<EnduranceNextActionAt) { return; }
    EnduranceNextActionAt=Now+.35f;
    AActor* Closest=nullptr; float Best=TNumericLimits<float>::Max();
    for(TActorIterator<ASwarmEnemy> It(GetWorld());It;++It)
    {
        if(It->GetHealth()<=0.f||It->IsBurrowed()) { continue; }
        const float Score=FVector::DistSquared2D(GetActorLocation(),It->GetActorLocation())*(It->Kind==4?.1f:1.f);
        if(Score<Best) { Best=Score; Closest=*It; }
    }
    if(Closest)
    {
        const FVector Aim=Closest->GetActorLocation();
        SetActorRotation((Aim-GetActorLocation()).Rotation()); AuroraAimPoint=Aim;
        ServerCastCharacterSkill_Implementation(false,Aim);
        ServerCastCharacterSkill_Implementation(true,Aim);
    }
    if(!(Arena==2 && Now-EnduranceStartedAt>SwarmRound::BossAt-5.f))
    {
        // Collect nearby XP using normal movement instead of leaving the entire build stationary.
        float Nearest=FMath::Square(1600.f); AActor* Loot=nullptr;
        for(TActorIterator<ASwarmExperienceOrb> It(GetWorld());It;++It)
        { const float Distance=FVector::DistSquared2D(GetActorLocation(),It->GetActorLocation()); if(Distance<Nearest) { Nearest=Distance; Loot=*It; } }
        EnduranceMoveGoal=Loot?Loot->GetActorLocation():EnduranceOrigin+FVector::ForwardVector.RotateAngleAxis((Now-EnduranceStartedAt)*2.f,FVector::UpVector)*600.f;
        if(Closest && Cast<ASwarmEnemy>(Closest)->Kind==3) { EnduranceMoveGoal=Closest->GetActorLocation(); }
    }
}
#endif

#if WITH_EDITOR
void AAnimaPreviewCharacter::RunPickupRulesTest()
{
    InvulnerableEnd=GetCombatTime()+90.f;
    for(TActorIterator<ASwarmExperienceOrb> It(GetWorld());It;++It) { It->Destroy(); }
    FActorSpawnParameters S;S.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Mate=GetWorld()->SpawnActor<AAnimaPreviewCharacter>(GetActorLocation()+FVector(-3000,-3000,0),FRotator::ZeroRotator,S);
    auto* MateController=GetWorld()->SpawnActor<APlayerController>(FVector::ZeroVector,FRotator::ZeroRotator,S);
    if(!Mate||!MateController) { UE_LOG(LogTemp,Error,TEXT("Pickup rules: teammate spawn failed"));return; }
    MateController->Possess(Mate);
    for(auto* Player:{this,Mate})
    {
        Player->OwnedAugments.Reset();Player->MaxCombatHealth=1000.f;Player->CombatHealth=200.f;Player->CombatLevel=30;Player->Experience=0;
        Player->RunGold=0;Player->PermanentGoldMultiplier=Player->PermanentExperienceMultiplier=Player->PermanentPickupMultiplier=1.f;
        Player->PermanentDurationMultiplier=1.f;Player->RunWeaponStats.Init(0,8);Player->LevelChoices.Reset();Player->PendingLevelChoices=0;
        Player->VacuumEnd=0.f;Player->InvulnerableEnd=GetCombatTime()+90.f;
    }
    if(auto* Mode=GetWorld()->GetAuthGameMode<AAnimaPreviewMode>()) { Mode->SetTeamProgressForReview(30,0); }
    auto Drop=[&](uint8 Kind,float X)
    {
        const FTransform T(GetActorLocation()+FVector(X,0,-38));
        auto* Orb=GetWorld()->SpawnActorDeferred<ASwarmExperienceOrb>(ASwarmExperienceOrb::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if(Orb) { Orb->Kind=Kind;Orb->FinishSpawning(T); }return Orb;
    };
    auto Alive=[](ASwarmExperienceOrb* Orb){return IsValid(Orb)&&!Orb->IsActorBeingDestroyed();};
    auto* XP=Drop(0,3000);auto* Gold=Drop(5,4000);auto* Health=Drop(6,3500);auto* Card=Drop(7,3600);
    auto* Bomb=Drop(8,3700);auto* Magnet=Drop(9,3800);auto* Boon=Drop(10,3900);auto* Car=Drop(11,4100);
    PermanentDurationMultiplier=1.5f;CollectWorldPowerup(SwarmLoot::Magnet);
    const bool Delayed=Experience==0&&RunGold==0&&Alive(XP)&&Alive(Gold);
    const bool Duration=FMath::IsNearlyEqual(GetVacuumRemaining(),9.f,.01f);
    const FVector Old=XP?XP->GetActorLocation():FVector::ZeroVector;if(XP) { XP->Tick(.05f); }
    const bool Flight=Alive(XP)&&!XP->GetActorLocation().Equals(Old,1.f);
    auto* Future=Drop(0,4200);const FVector FutureOld=Future?Future->GetActorLocation():FVector::ZeroVector;
    if(Future) { Future->Tick(.05f); }
    const bool FutureCaptured=Alive(Future)&&!Future->GetActorLocation().Equals(FutureOld,1.f);
    for(int32 I=0;I<120;++I) { for(TActorIterator<ASwarmExperienceOrb> It(GetWorld());It;++It) { It->Tick(.05f); } }
    const bool Shared=Experience>0&&Mate->Experience==Experience&&RunGold==3&&Mate->RunGold==3;
    const bool Preserved=Alive(Health)&&Alive(Card)&&Alive(Bomb)&&Alive(Magnet)&&Alive(Boon)&&Alive(Car)&&CombatHealth==200.f&&Mate->CombatHealth==200.f;
    VacuumEnd=GetCombatTime()-1.f;auto* Late=Drop(0,5000);const FVector LateOld=Late?Late->GetActorLocation():FVector::ZeroVector;
    if(Late) { Late->Tick(.1f); }
    const bool Expired=Alive(Late)&&Late->GetActorLocation().Equals(LateOld,.01f);
    Mate->OwnedAugments={static_cast<uint8>(ESwarmAugment::RegenTissue)};Mate->AugmentRegenStacks=0;
    if(Health) { Health->CollectBy(this); }
    const bool TeamHeal=FMath::IsNearlyEqual(CombatHealth,400.f)&&FMath::IsNearlyEqual(Mate->CombatHealth,400.f);
    if(Health) { Health->CollectBy(this); }if(Gold) { Gold->CollectBy(this); }
    const bool Once=FMath::IsNearlyEqual(CombatHealth,400.f)&&Mate->CombatHealth==400.f&&RunGold==3&&Mate->RunGold==3;
    const bool Regen=Mate->AugmentRegenStacks==5;
    CombatHealth=950.f;Mate->CombatHealth=999.f;auto* CapPack=Drop(6,2500);if(CapPack) { CapPack->CollectBy(this); }
    const bool Capped=CombatHealth==1000.f&&Mate->CombatHealth==1000.f;
    Mate->CombatHealth=0.f;auto* DeadPack=Drop(6,2500);if(DeadPack) { DeadPack->CollectBy(this); }
    const bool NoRevive=Mate->CombatHealth==0.f;Mate->CombatHealth=400.f;
    OwnedAugments={static_cast<uint8>(ESwarmAugment::Magnet)};AugmentNextMagnetAt=GetCombatTime();LevelChoices.Reset();TickAugments(.01f);
    const bool Periodic=GetVacuumRemaining()>0.f&&Alive(Card)&&Alive(Bomb)&&Alive(Magnet)&&Alive(Boon)&&Alive(Car);
    VacuumEnd=GetCombatTime()-1.f;CombatHealth=400.f;auto* Touch=Drop(6,40);if(Touch) { Touch->Tick(.2f); }
    const bool TouchSpecial=Touch&&Touch->IsActorBeingDestroyed()&&FMath::IsNearlyEqual(CombatHealth,600.f);
    if(auto* Mode=GetWorld()->GetAuthGameMode<AAnimaPreviewMode>()) { Mode->SetTeamProgressForReview(30,0); }
    Experience=0;CollectExperience(100,GetActorLocation(),false);const bool XPBonus=Experience==115;
    OwnedAugments.Add(static_cast<uint8>(ESwarmAugment::Hoarder));CombatHealth=400.f;CollectRunGold(3,GetActorLocation(),false);
    const bool GoldHeal=FMath::IsNearlyEqual(CombatHealth,415.f);
    Mate->OwnedAugments={static_cast<uint8>(ESwarmAugment::Hoarder)};Mate->CombatHealth=0.f;Mate->CollectRunGold(3,GetActorLocation(),false);
    const bool GoldNoRevive=Mate->CombatHealth==0.f;
    UE_LOG(LogTemp,Display,TEXT("Pickup rules: delayed=%d flight=%d future=%d duration=%d shared=%d preserved=%d expired=%d teamHeal=%d once=%d capped=%d noRevive=%d periodic=%d touch=%d xpBonus=%d goldHeal=%d goldNoRevive=%d regen=%d"),
        Delayed,Flight,FutureCaptured,Duration,Shared,Preserved,Expired,TeamHeal,Once,Capped,NoRevive,Periodic,TouchSpecial,XPBonus,GoldHeal,GoldNoRevive,Regen);
    MateController->UnPossess();Mate->Destroy();MateController->Destroy();VacuumEnd=0.f;
}
#endif

#if WITH_EDITOR
void AAnimaPreviewCharacter::RunPickupNetworkHostTest()
{
    if(auto* Mode=GetWorld()->GetAuthGameMode<AAnimaPreviewMode>()) { Mode->SetTeamProgressForReview(30,0); }
    TArray<AAnimaPreviewCharacter*> Players;
    for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
    {
        if(!It->GetController()) { continue; }Players.Add(*It);
        It->SetActorLocation(FVector(1600.f+Players.Num()*300.f,600.f,120.f),false);
        It->GetCharacterMovement()->StopMovementImmediately();
        It->MaxCombatHealth=1000.f;It->CombatHealth=200.f;It->CombatLevel=30;It->Experience=0;It->RunGold=0;
        It->OwnedAugments.Reset();It->RunWeaponStats.Init(0,8);It->PermanentGoldMultiplier=It->PermanentExperienceMultiplier=1.f;
        It->PermanentDurationMultiplier=1.f;It->VacuumEnd=0.f;It->JinxWeaponReadyAt=GetCombatTime()+40.f;
        It->InvulnerableEnd=GetCombatTime()+90.f;It->LevelChoices.Reset();It->ForceNetUpdate();
    }
    for(TActorIterator<ASwarmExperienceOrb> It(GetWorld());It;++It) { It->Destroy(); }
    auto Spawn=[&](uint8 Kind,FVector Offset)
    {
        const FTransform T(GetActorLocation()+Offset-FVector(0,0,38));
        auto* Orb=GetWorld()->SpawnActorDeferred<ASwarmExperienceOrb>(ASwarmExperienceOrb::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if(Orb) { Orb->Kind=Kind;Orb->FinishSpawning(T); }return TWeakObjectPtr<ASwarmExperienceOrb>(Orb);
    };
    Spawn(0,FVector(3000,1800,0));Spawn(5,FVector(3500,1800,0));
    const auto Pack=Spawn(6,FVector(6000,2000,0)),Card=Spawn(7,FVector(6200,2000,0)),Bomb=Spawn(8,FVector(6400,2000,0));
    CollectWorldPowerup(SwarmLoot::Magnet);
    UE_LOG(LogTemp,Display,TEXT("Pickup network host: players=%d started=%d"),Players.Num(),GetVacuumRemaining()>0.f);
    FTimerHandle Heal,Verify;
    GetWorldTimerManager().SetTimer(Heal,FTimerDelegate::CreateWeakLambda(this,[this,Pack]() { if(Pack.IsValid()) { Pack->CollectBy(this); } }),2.f,false);
    GetWorldTimerManager().SetTimer(Verify,FTimerDelegate::CreateWeakLambda(this,[this,Card,Bomb]()
    {
        int32 Shared=0;
        for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
        { if(It->GetController()&&It->RunGold>=3&&It->Experience>0&&It->CombatHealth>=400.f) { ++Shared; } }
        const bool Protected=Card.IsValid()&&!Card->IsActorBeingDestroyed()&&Bomb.IsValid()&&!Bomb->IsActorBeingDestroyed();
        UE_LOG(LogTemp,Display,TEXT("Pickup network host: sharedPlayers=%d protected=%d"),Shared,Protected);
    }),4.f,false);
}
#endif

#if WITH_EDITOR
void AAnimaPreviewCharacter::RunTeamProgressTest()
{
    auto* Mode=GetWorld()->GetAuthGameMode<AAnimaPreviewMode>();if(!Mode) { return; }
    Mode->SetTeamProgressForReview(1,0);
    FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    TArray<AAnimaPreviewCharacter*> Players={this};TArray<APlayerController*> Controllers;
    for(int32 I=0;I<2;++I)
    {
        auto* Mate=GetWorld()->SpawnActor<AAnimaPreviewCharacter>(GetActorLocation()+FVector(3000,2000+I*400,0),FRotator::ZeroRotator,P);
        auto* PC=GetWorld()->SpawnActor<APlayerController>(FVector::ZeroVector,FRotator::ZeroRotator,P);
        if(!Mate||!PC) { UE_LOG(LogTemp,Error,TEXT("Team progress: fixture spawn failed"));return; }
        PC->Possess(Mate);Players.Add(Mate);Controllers.Add(PC);
    }
    auto Clear=[&]()
    {
        for(auto* Player:Players)
        {
            Player->LevelChoices.Reset();Player->LevelChoiceDeadline=0.f;Player->PendingLevelChoices=Player->PendingAccessCards=Player->PendingAugmentRewards=0;
            Player->bAugmentRewardOpen=Player->bAccessRewardOpen=false;Player->EndLevelChoiceSelection();
        }
    };
    Clear();Mode->SetTeamProgressForReview(5,GetSwarmExperienceRequired(5)-1);
    for(auto* Player:Players)
    {
        Player->CombatLevel=5;Player->Experience=GetSwarmExperienceRequired(5)-1;Player->CombatHealth=Player->MaxCombatHealth;
        Player->OwnedAugments.Reset();Player->RunWeaponStats.Init(0,8);Player->PermanentExperienceMultiplier=1.f;
        Player->InvulnerableEnd=GetCombatTime()+90.f;
    }
    Players[1]->PermanentExperienceMultiplier=2.f;Players[2]->CombatHealth=0.f;
    CollectExperience(1,GetActorLocation(),false);
    bool Shared=Mode->GetTeamLevel()==6&&Mode->GetTeamExperience()==1,Downed=true;
    for(auto* Player:Players) { Shared&=Player->CombatLevel==6&&Player->Experience==1&&Player->PendingLevelChoices==1; }
    Downed=Players[2]->CombatHealth==0.f&&Players[2]->LevelChoices.IsEmpty();
    Players[1]->CollectExperience(1,GetActorLocation(),false);
    bool Independent=Mode->GetTeamExperience()==3;
    for(auto* Player:Players) { Independent&=Player->Experience==3; }
    const int32 OldSeq=LevelChoiceSequence;const int32 OldPending=PendingLevelChoices;
    GrantAugmentReward();const bool Priority=bAugmentRewardOpen&&PendingLevelChoices==OldPending&&LevelChoiceSequence>OldSeq;
    const bool Five=FMath::IsNearlyEqual(GetLevelChoiceTimeRemaining(),5.f,.05f);
    const int32 OldCount=OwnedAugments.Num();ServerSelectLevelChoice_Implementation(0,OldSeq);
    const bool Stale=OwnedAugments.Num()==OldCount&&bAugmentRewardOpen;
    LevelChoiceDeadline=GetLevelChoiceClock()-.1f;Tick(.01f);
    const bool Auto=OwnedAugments.Num()==OldCount+1&&PendingAugmentRewards==0&&PendingLevelChoices==OldPending;
    const FSwarmChoiceOffer Current=LevelChoiceOffer;FSwarmChoiceOffer Older=Current;--Older.Sequence;Older.Cards.Reset();ApplyLevelChoiceOffer(Older);
    const bool Snapshot=LevelChoiceSequence==Current.Sequence&&LevelChoices==Current.Cards;
    auto* Late=GetWorld()->SpawnActor<AAnimaPreviewCharacter>(GetActorLocation()+FVector(-3000,2000,0),FRotator::ZeroRotator,P);
    auto* LatePC=GetWorld()->SpawnActor<APlayerController>(FVector::ZeroVector,FRotator::ZeroRotator,P);
    bool Join=false;
    if(Late&&LatePC)
    {
        LatePC->Possess(Late);const int32 Pending=Late->PendingLevelChoices;Mode->SyncTeamProgress(Late);
        Join=Late->CombatLevel==6&&Late->Experience==3&&Pending==5&&Late->PendingLevelChoices==Pending;
        Players.Add(Late);Controllers.Add(LatePC);
    }
    Clear();const bool Resume=FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(this),1.f,.001f);
    bool Curve=GetSwarmExperienceRequired(1)==120&&GetSwarmExperienceRequired(5)==240&&GetSwarmExperienceRequired(20)>276*2&&GetSwarmExperienceRequired(30)>396*4;
    for(int32 L=2;L<=100;++L) { Curve&=GetSwarmExperienceRequired(L)>GetSwarmExperienceRequired(L-1); }
    Mode->SetTeamProgressForReview(10,0);
    for(auto* Player:Players) { Player->CombatLevel=10;Player->Experience=0;Player->OwnedAugments.Reset(); }
    CollectExperience(1000,GetActorLocation(),false);
    bool Bulk=Mode->GetTeamLevel()>11;
    for(auto* Player:Players) { Bulk&=Player->CombatLevel==Mode->GetTeamLevel()&&Player->Experience==Mode->GetTeamExperience()&&Player->PendingLevelChoices==Mode->GetTeamLevel()-10; }
    Clear();Players[2]->CombatHealth=0.f;Players[2]->GrantAugmentReward();
    const bool Deferred=Players[2]->PendingAugmentRewards==1&&Players[2]->LevelChoices.IsEmpty();
    Players[2]->CombatHealth=100.f;Players[2]->Tick(.01f);
    const bool ReviveOffer=Deferred&&Players[2]->bAugmentRewardOpen&&!Players[2]->LevelChoices.IsEmpty();
    Clear();
    UE_LOG(LogTemp,Display,TEXT("Team progress: shared=%d singleCredit=%d downed=%d priority=%d fiveSeconds=%d stale=%d automatic=%d snapshot=%d lateJoin=%d resumed=%d curve=%d bulk=%d reviveOffer=%d"),Shared,Independent,Downed,Priority,Five,Stale,Auto,Snapshot,Join,Resume,Curve,Bulk,ReviveOffer);
    for(auto* PC:Controllers) { APawn* Pawn=PC->GetPawn();PC->UnPossess();if(Pawn) { Pawn->Destroy(); }PC->Destroy(); }
}

void AAnimaPreviewCharacter::StartTeamChoiceNetworkReview()
{
    auto* Mode=GetWorld()->GetAuthGameMode<AAnimaPreviewMode>();if(!Mode) { return; }
    Mode->SetTeamProgressForReview(5,GetSwarmExperienceRequired(5)-1);int32 Count=0;
    for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
    {
        if(!It->GetController()) { continue; }
        It->CombatLevel=5;It->Experience=GetSwarmExperienceRequired(5)-1;It->CombatHealth=It->MaxCombatHealth;It->OwnedAugments.Reset();
        It->LevelChoices.Reset();It->PendingLevelChoices=It->PendingAccessCards=It->PendingAugmentRewards=0;
        It->bAugmentRewardOpen=It->bAccessRewardOpen=false;It->EndLevelChoiceSelection();
        It->RunWeaponStats.Init(0,8);It->PermanentExperienceMultiplier=++Count==1?1.f:2.f;
        It->InvulnerableEnd=GetCombatTime()+120.f;It->JinxWeaponReadyAt=GetCombatTime()+120.f;
        It->JinxRockets.Reset();It->GeneralProjectiles.Reset();It->SeraphineWaves.Reset();It->CombatHazards.Reset();
    }
    for(TActorIterator<ASwarmExperienceOrb> It(GetWorld());It;++It) { It->Destroy(); }
    TeamChoiceReviewStart=GetLevelChoiceClock();TeamChoiceReviewStage=1;
    CollectExperience(1,GetActorLocation(),false);
    for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It) { if(It->GetController()) { It->GrantAugmentReward(); } }
    UE_LOG(LogTemp,Display,TEXT("Team choice host: started=%d players=%d"),TeamChoiceReviewStage,Count);
}

void AAnimaPreviewCharacter::TickTeamChoiceNetworkReview()
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("SwarmTeamChoiceNetworkReview"))) { return; }
    if(HasAuthority())
    {
        InvulnerableEnd=GetCombatTime()+120.f;JinxWeaponReadyAt=GetCombatTime()+120.f;
        if(TeamChoiceReviewStart<0.f) { return; }
        const float Elapsed=GetLevelChoiceClock()-TeamChoiceReviewStart;
        if(TeamChoiceReviewStage==1&&Elapsed>.8f)
        { ServerSelectLevelChoice_Implementation(0,LevelChoiceSequence);TeamChoiceReviewStage=2; }
        if(TeamChoiceReviewStage==2&&Elapsed>11.f)
        {
            bool Ready=true;int32 Count=0;
            for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
            { if(It->GetController()) { ++Count;Ready&=It->OwnedAugments.Num()==1&&It->LevelChoices.IsEmpty()&&It->PendingLevelChoices==0; } }
            const bool Resume=UGameplayStatics::GetGlobalTimeDilation(this)>.99f;
            UE_LOG(LogTemp,Display,TEXT("Team choice host: players=%d automatic=%d resumed=%d"),Count,Ready,Resume);
            CollectExperience(4,GetActorLocation(),false);TeamChoiceReviewStage=3;
        }
        if(TeamChoiceReviewStage==3&&Elapsed>12.f)
        {
            auto* Mode=GetWorld()->GetAuthGameMode<AAnimaPreviewMode>();bool Shared=Mode&&Mode->GetTeamLevel()==6;
            for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
            { if(It->GetController()) { Shared&=It->CombatLevel==6&&It->Experience==Mode->GetTeamExperience(); } }
            UE_LOG(LogTemp,Display,TEXT("Team choice host: shared=%d level=%d xp=%d"),Shared,CombatLevel,Experience);TeamChoiceReviewStage=4;
        }
    }
    else if(IsLocallyControlled())
    {
        if(!bTeamChoiceClientOfferSeen&&bAugmentRewardOpen&&!LevelChoices.IsEmpty())
        {
            bTeamChoiceClientOfferSeen=true;TeamChoiceClientOfferStart=GetLevelChoiceClock();
            UE_LOG(LogTemp,Display,TEXT("Team choice client: presented=1 sequence=%d remaining=%.2f"),LevelChoiceSequence,GetLevelChoiceTimeRemaining());
        }
        if(bTeamChoiceClientOfferSeen&&TeamChoiceClientOfferStart>=0.f&&!bAugmentRewardOpen)
        {
            UE_LOG(LogTemp,Display,TEXT("Team choice client: augmentClosedAfter=%.2f"),GetLevelChoiceClock()-TeamChoiceClientOfferStart);TeamChoiceClientOfferStart=-1.f;
        }
        if(!bTeamChoiceClientReady&&bTeamChoiceClientOfferSeen&&LevelChoices.IsEmpty()&&OwnedAugments.Num()==1&&CombatLevel==6&&Experience>=9)
        { bTeamChoiceClientReady=true;UE_LOG(LogTemp,Display,TEXT("Team choice client: selected=1 shared=1 level=%d xp=%d resumed=%d"),CombatLevel,Experience,UGameplayStatics::GetGlobalTimeDilation(this)>.99f); }
    }
}
#endif

#if WITH_EDITOR
void AAnimaPreviewCharacter::RunCoopRecoveryTest()
{
    FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* MatePC=GetWorld()->SpawnActor<APlayerController>(FVector::ZeroVector,FRotator::ZeroRotator,P);
    auto* OtherPC=GetWorld()->SpawnActor<APlayerController>(FVector::ZeroVector,FRotator::ZeroRotator,P);
    auto* Mate=GetWorld()->SpawnActor<AAnimaPreviewCharacter>(GetActorLocation()+FVector(300,0,0),FRotator::ZeroRotator,P);
    auto* Other=GetWorld()->SpawnActor<AAnimaPreviewCharacter>(GetActorLocation()+FVector(900,0,0),FRotator::ZeroRotator,P);
    if(!MatePC||!OtherPC||!Mate||!Other) { return; }
    MatePC->Possess(Mate);OtherPC->Possess(Other);Mate->CombatHealth=Mate->MaxCombatHealth;Other->CombatHealth=Other->MaxCombatHealth;
    CombatHealth=0.f;GetCharacterMovement()->Velocity=FVector(100,0,0);TickDownedState(0.f);
    const bool Stopped=GetCharacterMovement()->MovementMode==MOVE_None&&GetVelocity().IsNearlyZero();
    const bool Golden=FMath::IsNearlyEqual(GetReviveWindowRemaining(),30.f,.1f);
    UpdateSpectatorView();auto* First=SpectatorTarget.Get();UpdateSpectatorView(1);auto* Second=SpectatorTarget.Get();UpdateSpectatorView(-1);
    const bool Spectate=First&&Second&&First!=Second&&SpectatorTarget.Get()==First;
    TickDownedState(4.f);const bool Progress=FMath::IsNearlyEqual(ReviveProgress,4.f);
    Mate->SetActorLocation(GetActorLocation()+FVector(600,0,0));TickDownedState(.1f);const bool Reset=ReviveProgress==0.f;
    Mate->SetActorLocation(GetActorLocation()+FVector(300,0,0));TickDownedState(4.f);const bool NotEarly=CombatHealth==0.f;
    TickDownedState(1.f);const bool Revived=FMath::IsNearlyEqual(CombatHealth,MaxCombatHealth*.3f)&&GetCharacterMovement()->MovementMode==MOVE_Walking&&!SpectatorTarget.IsValid();
    CombatHealth=0.f;TickDownedState(0.f);ReviveWindowEndsAt=GetCombatTime()-.1f;TickDownedState(10.f);
    const bool Expired=bReviveExpired&&CombatHealth==0.f&&ReviveProgress==0.f;
    CombatHealth=MaxCombatHealth;TickDownedState(0.f);MatePC->Destroy();OtherPC->Destroy();Mate->Destroy();Other->Destroy();
    UE_LOG(LogTemp,Display,TEXT("Coop recovery: stopped=%d golden30=%d spectator=%d progress=%d reset=%d notEarly=%d revived5=%d expired=%d"),Stopped,Golden,Spectate,Progress,Reset,NotEarly,Revived,Expired);
}
#endif

#if WITH_EDITOR
void AAnimaPreviewCharacter::RunBossChallengeTest()
{
    const float Now=GetCombatTime();InvulnerableEnd=0.f;MaxCombatHealth=CombatHealth=10000.f;RunArmorBonus=0.f;BaseArmor=0.f;
    const FTransform MiniTransform(GetActorLocation()+FVector(650,0,0));
    auto* Mini=GetWorld()->SpawnActorDeferred<ASwarmEnemy>(ASwarmEnemy::StaticClass(),MiniTransform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    Mini->Kind=2;Mini->ModelVariant=6;Mini->FinishSpawning(MiniTransform);
    const auto* RigAsset=Mini->GetMesh()->GetSkeletalMeshAsset();
    const bool Size=RigAsset&&FMath::IsNearlyEqual(RigAsset->GetBounds().BoxExtent.GetMax()*2.f*Mini->BodyScale,300.f,1.f);
    bool Scheduled=true;for(int32 Arena=0;Arena<4;++Arena) {Scheduled&=GetSwarmTimedMiniBossVariant(0,Arena)==6&&GetSwarmTimedMiniBossVariant(2,Arena)==6&&GetSwarmTimedMiniBossVariant(1,Arena)<7&&GetSwarmTimedMiniBossVariant(3,Arena)<7;}
    Mini->TickSeahorseCharge(this,Now,.01f);FVector Start,End;float Radius;bool Dash=false;
    const bool Warn=Mini->GetAttackWarning(Now,Start,End,Radius,Dash)&&Dash&&Radius==125.f&&Mini->SeahorseChargeEnd==0.f;
    const FVector Locked=Mini->SeahorseChargeDirection;SetActorLocation(Mini->GetActorLocation()+FVector(0,700,0));Mini->TickSeahorseCharge(this,Now+1.f,.1f);
    const bool Lock=Mini->SeahorseChargeDirection.Equals(Locked)&&Mini->SeahorseChargeEnd>Now;
    Mini->TickSeahorseCharge(this,Now+4.f,.1f);const bool Recovery=Mini->SeahorseChargeEnd==0.f&&Mini->NextAttack>Now+4.f;
    const FTransform BT(GetActorLocation()+FVector(700,0,0));auto* Boss=GetWorld()->SpawnActorDeferred<ASwarmEnemy>(ASwarmEnemy::StaticClass(),BT,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);Boss->Kind=3;Boss->FinishSpawning(BT);
    if(FParse::Param(FCommandLine::Get(),TEXT("BossChallengeVisualReview")))
    {
        InvulnerableEnd=Now+60.f;Mini->SetActorLocation(GetActorLocation()+FVector(220,-350,0));Mini->NextAttack=Now+60.f;
        Boss->SetActorLocation(GetActorLocation()+FVector(420,250,0));Boss->NextAttack=Now+60.f;Boss->GetCharacterMovement()->DisableMovement();Boss->SpawnBossPressurePattern();
        FTimerHandle Shot;GetWorldTimerManager().SetTimer(Shot,[](){FScreenshotRequest::RequestScreenshot(false);},.15f,false);return;
    }
    const bool Damage=Boss->GetSpawnDamage()>=220.f&&Mini->GetSpawnDamage()>=105.f;
    const bool Calm=!Boss->IsBossEnraged();Boss->Health=Boss->GetMaxHealth()*.39f;const bool Enrage=Calm&&Boss->IsBossEnraged();Boss->SpawnBossPressurePattern();int32 Marks=0;bool Telegraph=true;
    for(TActorIterator<ASwarmBossHazard> It(GetWorld());It;++It) { if(It->GetOwner()==Boss) {++Marks;Telegraph&=It->ActivateAt>Now+.9f;It->Destroy();} }
    const bool Pattern=Marks>0&&Telegraph;
    const auto CreateMark=[&](FVector At,float AtTime) {const FTransform T(At);auto* H=GetWorld()->SpawnActorDeferred<ASwarmBossHazard>(ASwarmBossHazard::StaticClass(),T,Boss,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);H->ActivateAt=AtTime;H->Damage=300.f;H->Radius=150.f;H->FinishSpawning(T);return H;};
    auto* Hit=CreateMark(GetActorLocation()-FVector(0,0,80),Now+1.f);const float Before=CombatHealth;Hit->Tick(0.f);const bool NoEarly=CombatHealth==Before;
    Hit->ActivateAt=Now-.1f;Hit->Tick(0.f);const float After=CombatHealth;Hit->Tick(0.f);const bool Once=After<Before-100.f&&CombatHealth==After;
    auto* Miss=CreateMark(GetActorLocation()+FVector(700,0,0),Now-.1f);Miss->Tick(0.f);const bool Dodge=CombatHealth==After;
    auto* Cancel=CreateMark(GetActorLocation(),Now-.1f);Boss->Health=0.f;Cancel->Tick(0.f);const bool Cleanup=Cancel->IsActorBeingDestroyed()&&CombatHealth==After;
    Hit->Destroy();Miss->Destroy();Mini->Destroy();Boss->Destroy();
    UE_LOG(LogTemp,Display,TEXT("Boss challenge: seahorseSize=%d scheduled=%d warning=%d locked=%d recovery=%d damage=%d enrage=%d pattern=%d noEarly=%d once=%d dodge=%d cleanup=%d"),Size,Scheduled,Warn,Lock,Recovery,Damage,Enrage,Pattern,NoEarly,Once,Dodge,Cleanup);
}
#endif

#if WITH_EDITOR
void AAnimaPreviewCharacter::RunEquipmentTooltipTest()
{
    GeneralWeaponRanks.Init(1,GeneralWeaponCount);EvolvedGeneralWeapons.Reset();RunWeaponStats.Init(0,8);OwnedAugments.Reset();PermanentAbilityHaste=100.f;JinxPassiveEnd=XayahBuffEnd=0.f;
    const float Now=GetCombatTime();bool Descriptions=true,Cooldowns=true,Remaining=true,Passives=true;
    for(uint8 W=0;W<GeneralWeaponCount;++W)
    {
        GeneralWeaponReadyAt[W]=Now+7.f;const auto Type=static_cast<EGeneralWeapon>(W);const auto Stats=GetGeneralWeaponStats(W,1,false);
        const bool NoHaste=Type==EGeneralWeapon::Trail||Type==EGeneralWeapon::Radiance||Type==EGeneralWeapon::Glove;
        float Expected=NoHaste?Stats.Cooldown:GetCombatCooldown(Stats.Cooldown);if(Type==EGeneralWeapon::Cyclone) {Expected=FMath::Max(Expected,Stats.Duration*GetCombatDurationMultiplier());}
        Descriptions&=CombatAudio::Load(*FString::Printf(TEXT("General/%s_Weapon"),GetGeneralWeaponInfo(W).AssetSuffix))!=nullptr&&CombatAudio::Load(*FString::Printf(TEXT("General/%s_Impact"),GetGeneralWeaponInfo(W).AssetSuffix))!=nullptr;
        Descriptions&=GetGeneralWeaponTooltip(W).Contains(GetGeneralWeaponInfo(W).Name)&&GetGeneralWeaponTooltip(W).Contains(TEXT("피해"));
        Cooldowns&=FMath::IsNearlyEqual(GetGeneralWeaponCooldown(W),Expected,.001f);Remaining&=FMath::IsNearlyEqual(GetGeneralWeaponRemaining(W),7.f,.01f);
    }
    for(int32 C=0;C<9;++C) {CharacterChoice=C;Passives&=GetPassiveTooltip().Contains(TEXT("패시브"))&&GetPassiveTooltip().Contains(TEXT("대기시간"));
        Descriptions&=!GetSignatureWeaponName(false).IsEmpty()&&GetSignatureWeaponName(false)!=GetSignatureWeaponName(true)&&GetSignatureWeaponTooltip().Contains(GetSignatureWeaponName(false))&&GetSignatureWeaponTooltip().Contains(GetSignatureWeaponName(true));
    }CharacterChoice=0;
    const uint8 Cyclone=static_cast<uint8>(EGeneralWeapon::Cyclone);EvolvedGeneralWeapons.Add(Cyclone);GeneralWeaponReadyAt[Cyclone]=TNumericLimits<float>::Max();
    const bool Permanent=GetGeneralWeaponRemaining(Cyclone)==0.f&&GetGeneralWeaponCooldown(Cyclone)==0.f&&GetGeneralWeaponTooltip(Cyclone).Contains(TEXT("영구 유지"));
    UE_LOG(LogTemp,Display,TEXT("Equipment tooltips: descriptions=%d cooldowns=%d remaining=%d passives=%d permanent=%d"),Descriptions,Cooldowns,Remaining,Passives,Permanent);
    if(FParse::Param(FCommandLine::Get(),TEXT("EquipmentTooltipReview")))
    {
        GeneralWeapons={0,1,2,3};InvulnerableEnd=Now+60.f;ASwarmExperienceOrb::SpawnPickup(GetWorld(),GetActorLocation()+FVector(220,250,0),6);
        FTimerHandle Shot;GetWorldTimerManager().SetTimer(Shot,[](){FScreenshotRequest::RequestScreenshot(false);},.15f,false);
    }

}
#endif

#if WITH_EDITOR
void AAnimaPreviewCharacter::RunEnemyAttackMotionTest()
{
    int32 Loaded=0,Changed=0,Restored=0,Total=0,Sounds=0,FrozenCancelled=0;const float Now=GetCombatTime();
    for(int32 Kind=0;Kind<4;++Kind)
    {
        const int32 Count=Kind==0?10:Kind==1?7:Kind==2?8:4;
        for(int32 Variant=0;Variant<Count;++Variant)
        {
            const FTransform T(GetActorLocation()+FVector(800,0,0));auto* Enemy=GetWorld()->SpawnActorDeferred<ASwarmEnemy>(ASwarmEnemy::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            Enemy->Kind=Kind;Enemy->ModelVariant=Variant;Enemy->FinishSpawning(T);
            if(Kind==3)
            {
                static const TCHAR* Names[]={TEXT("Reksai"),TEXT("Briar"),TEXT("Belveth"),TEXT("Aatrox")};const FString Folder=FString::Printf(TEXT("/Game/TroubleSquad/Enemies/Locomotion/%s/"),Names[Variant]);
                Enemy->GetMesh()->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,*(Folder+TEXT("SK_")+Names[Variant]+TEXT(".SK_")+Names[Variant])));
                Enemy->bAnimatedLocomotion=true;Enemy->BodyScale=1.f;Enemy->bBurrowed=false;Enemy->GetMesh()->SetHiddenInGame(false);
                Enemy->WalkSequence=LoadObject<UAnimSequence>(nullptr,*(Folder+TEXT("AN_")+Names[Variant]+TEXT("_Walk.AN_")+Names[Variant]+TEXT("_Walk")));
                Enemy->AttackSequence=LoadObject<UAnimSequence>(nullptr,*(Folder+TEXT("AN_")+Names[Variant]+TEXT("_Attack.AN_")+Names[Variant]+TEXT("_Attack")));
            }
            ++Total;if(Enemy->AttackSequence&&Enemy->WalkSequence)
            {
                const FString Stem=Enemy->AttackSequence->GetName().Mid(3).LeftChop(7);for(const TCHAR* Cue:{TEXT("Hit"),TEXT("Death"),TEXT("Windup"),TEXT("Attack")}) {Sounds+=CombatAudio::Load(*FString::Printf(TEXT("Enemies/Variants/%s_%s"),*Stem,Cue))!=nullptr;}
                ++Loaded;Enemy->AttackMotionStartAt=Now-.5f;Enemy->AttackMotionImpactAt=Now+.5f;Enemy->UpdateBodyPose();Enemy->GetMesh()->TickAnimation(0,false);Enemy->GetMesh()->RefreshBoneTransforms();const FTransform Before=Enemy->GetMesh()->GetBoneTransform(Enemy->GetMesh()->GetBoneIndex(TEXT("Torso")));
                Enemy->AttackMotionImpactAt=Now-.01f;Enemy->UpdateBodyPose();Enemy->GetMesh()->TickAnimation(0,false);Enemy->GetMesh()->RefreshBoneTransforms();const FTransform After=Enemy->GetMesh()->GetBoneTransform(Enemy->GetMesh()->GetBoneIndex(TEXT("Torso")));Changed+=!Before.Equals(After,.001f);
                Enemy->AttackMotionImpactAt=Now-2.f;Enemy->UpdateBodyPose();auto* Node=Enemy->GetMesh()->GetSingleNodeInstance();Restored+=Node&&Node->GetCurrentAsset()==Enemy->WalkSequence.Get();if(Kind<3) {Enemy->TriggerAttackPose(Now+1.f,1.f);Enemy->Freeze(1.f);Enemy->UpdateBodyPose();auto* FrozenNode=Enemy->GetMesh()->GetSingleNodeInstance();FrozenCancelled+=Enemy->AttackMotionImpactAt==0.f&&Enemy->SeahorseChargeEnd==0.f&&FrozenNode&&FrozenNode->GetCurrentAsset()==Enemy->WalkSequence.Get();}
            }
            Enemy->Destroy();
        }
    }
    UE_LOG(LogTemp,Display,TEXT("Enemy attack motions: loaded=%d changed=%d restored=%d total=%d audio=%d freeze=%d"),Loaded,Changed,Restored,Total,Sounds,FrozenCancelled);
}

void AAnimaPreviewCharacter::RunOriginalAudioAcceptanceTest()
{
    static const TCHAR* Names[]={TEXT("Jinx"),TEXT("Seraphine"),TEXT("Leona"),TEXT("Illaoi"),TEXT("Briar"),TEXT("Yasuo"),TEXT("Riven"),TEXT("Aurora"),TEXT("Xayah")};
    bool Assets=true,Variants=true,Music=true;
    for(const TCHAR* N:Names)
    for(const TCHAR* Slot:{TEXT("Weapon"),TEXT("WeaponEvolved"),TEXT("WeaponImpact"),TEXT("WeaponImpactEvolved"),TEXT("E"),TEXT("R")})
    {
        const FString Cue=FString::Printf(TEXT("Champions/%s_%s"),N,Slot);
        Assets&=CombatAudio::Load(*Cue)!=nullptr;
        for(int32 V=1;V<CombatAudio::GetVariationCount(*Cue);++V)
        {Variants&=CombatAudio::Load(*FString::Printf(TEXT("%s_V%d"),*Cue,V))!=nullptr;}
    }
    for(int32 A=0;A<4;++A)
    {
        Music&=CombatAudio::SelectMusicCue(A,0,false,false)==FString::Printf(TEXT("Music/Map%d"),A);
        Music&=CombatAudio::SelectMusicCue(A,200,false,false)==TEXT("Music/Mid");
        Music&=CombatAudio::SelectMusicCue(A,400,false,false)==TEXT("Music/Late");
        Music&=CombatAudio::SelectMusicCue(A,600,true,false)==FString::Printf(TEXT("Music/Boss%d"),A);
        for(const FString Cue:{CombatAudio::SelectMusicCue(A,0,false,false),CombatAudio::SelectMusicCue(A,200,false,false),CombatAudio::SelectMusicCue(A,400,false,false),CombatAudio::SelectMusicCue(A,600,true,false)}){Music&=CombatAudio::Load(*Cue)!=nullptr;}
    }
    Music&=CombatAudio::SelectMusicCue(3,600,true,true)==TEXT("Music/Boss3Phase2")&&CombatAudio::Load(TEXT("Music/Boss3Phase2"))!=nullptr;
    const bool NoPop=CombatAudio::IsIntentionallySilent(TEXT("Enemies/Minion_Hit"))&&CombatAudio::IsIntentionallySilent(TEXT("Enemies/Variants/Swarmer_Hit"))&&!CombatAudio::IsIntentionallySilent(TEXT("Enemies/Variants/Swarmer_Death"));
    UE_LOG(LogTemp,Display,TEXT("Original audio: heroes=%d variants=%d music=%d noPop=%d"),Assets,Variants,Music,NoPop);
}

void AAnimaPreviewCharacter::RunAudioEventTimingTest()
{
    for(TActorIterator<ASwarmEnemy> It(GetWorld());It;++It){It->Destroy();}
    GeneralProjectiles.Reset();GeneralWeapons.Reset();WeaponZones.Reset();
    const uint8 W=static_cast<uint8>(EGeneralWeapon::Launcher);
    GeneralWeaponRanks.Init(1,GeneralWeaponCount);EvolvedGeneralWeapons.Reset();RunWeaponStats.Init(0,8);
    OwnedAugments.Reset();RunMoveBonus=0.f;PermanentDamageMultiplier=1.f;
    AudioTimingLaunches=AudioTimingImpacts=0;
    for(float& T:NextGeneralAudioAt){T=0.f;}
    const float Now=GetCombatTime();
    FireGeneralWeapon(W,Now);
    const bool Empty=AudioTimingLaunches==0&&GeneralProjectiles.IsEmpty();
    FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Enemy=GetWorld()->SpawnActor<ASwarmEnemy>(GetActorLocation()+GetActorForwardVector()*600.f,FRotator::ZeroRotator,Params);
    if(!Enemy){return;}Enemy->Freeze(60.f);const float Before=Enemy->GetHealth();
    FireGeneralWeapon(W,Now);
    const bool Launch=AudioTimingLaunches==1&&AudioTimingImpacts==0&&!GeneralProjectiles.IsEmpty();
    for(int32 I=1;I<=4;++I){TickGeneralProjectiles(Now+.02f*I,.02f);}
    const bool Flight=AudioTimingLaunches==1&&AudioTimingImpacts==0&&IsValid(Enemy)&&FMath::IsNearlyEqual(Enemy->GetHealth(),Before);
    for(int32 I=5;I<=20&&!GeneralProjectiles.IsEmpty();++I){TickGeneralProjectiles(Now+.02f*I,.02f);}
    const bool Impact=AudioTimingLaunches==1&&AudioTimingImpacts==1&&(!IsValid(Enemy)||Enemy->GetHealth()<Before);
    const int32 Hits=AudioTimingImpacts;
    for(int32 I=0;I<8;++I){RenderWeaponVfx(W,GetActorLocation(),GetActorLocation()+GetActorForwardVector()*100.f,20.f,I%3,false);}
    const bool Visuals=AudioTimingLaunches==1&&AudioTimingImpacts==Hits;
    GeneralProjectiles.Reset();WeaponZones.Reset();AudioTimingLaunches=AudioTimingImpacts=0;
    const uint8 Fire=static_cast<uint8>(EGeneralWeapon::Fire);
    const auto Stats=GetGeneralWeaponStats(Fire,1,false);
    auto* Burning=GetWorld()->SpawnActor<ASwarmEnemy>(GetActorLocation()+GetActorForwardVector()*Stats.Range,FRotator::ZeroRotator,Params);
    bool Burn=false;
    if(Burning)
    {
        Burning->Health=10000.f;Burning->Freeze(60.f);
        NextGeneralAudioAt[Fire*2]=NextGeneralAudioAt[Fire*2+1]=0.f;
        FireGeneralWeapon(Fire,Now);
        TickGeneralWeapons(Now+.3f,.3f);const float First=Burning->GetHealth();
        NextGeneralAudioAt[Fire*2+1]=0.f;
        TickGeneralWeapons(Now+.9f,.6f);
        Burn=First<10000.f&&Burning->GetHealth()<First&&AudioTimingLaunches==1&&AudioTimingImpacts==1;
    }
    CombatAudio::FGeneralVoiceBudget Budget;
    const bool Separate=Budget.TryReserve(10.f,false)&&Budget.TryReserve(10.f,true)
        &&!Budget.TryReserve(10.01f,false)&&!Budget.TryReserve(10.01f,true)
        &&Budget.TryReserve(10.03f,false)&&Budget.TryReserve(10.03f,true);
    UE_LOG(LogTemp,Display,TEXT("Audio event timing: empty=%d launch=%d flight=%d impact=%d visualsSilent=%d burnOnce=%d separateBudget=%d"),Empty,Launch,Flight,Impact,Visuals,Burn,Separate);
}
#endif
