#include "AnimaPreview.h"
#include "SwarmEncounter.h"
#include "SwarmLoot.h"
#include "DistrictProgress.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Misc/CommandLine.h"

void AAnimaPreviewCharacter::BeginWorldVacuum(float Duration)
{
    if(!HasAuthority() || CombatHealth<=0.f || bDistrictFinished || !FMath::IsFinite(Duration)) { return; }
    VacuumEnd=FMath::Max(VacuumEnd,GetCombatTime()+FMath::Clamp(Duration,.1f,60.f));
    for(TActorIterator<ASwarmExperienceOrb> It(GetWorld());It;++It)
    { if(It->CanBeVacuumed()) { It->AttractTo(this,true); } }
    ClientPlayCombatCue(4);ForceNetUpdate();
}

void AAnimaPreviewCharacter::CollectWeaponHealth()
{
    if(!HasAuthority() || CombatHealth<=0.f || bDistrictFinished) { return; }
    const float Before=CombatHealth;
    CombatHealth=FMath::Min(MaxCombatHealth,CombatHealth+MaxCombatHealth*.20f);
    NotifyAugmentHealPickup();
    if(CombatHealth>Before) { ClientPlayCombatCue(2); }
    ForceNetUpdate();
}

void AAnimaPreviewCharacter::CollectWorldPowerup(uint8 Kind)
{
    if(!HasAuthority()||CombatHealth<=0.f||bDistrictFinished) { return; }
    const FVector Position=GetActorLocation();
    if(Kind==SwarmLoot::Magnet)
    {
        BeginWorldVacuum(6.f*GetCombatDurationMultiplier());
    }
    else if(Kind==SwarmLoot::Bomb)
    {
        ClientRecordWorldProgress(1,false,RunBoonsCollected,false);
        ShowWeaponVfx(static_cast<uint8>(EGeneralWeapon::Bomb),Position,Position,1800.f,1,true);
        for(TActorIterator<ASwarmEnemy> It(GetWorld());It;++It)
        {
            if(It->Kind==4||FVector::DistSquared2D(Position,It->GetActorLocation())>FMath::Square(1800.f)) { continue; }
            const float Damage=It->Kind==3?It->GetMaxHealth()*.05f:It->GetMaxHealth()*2.f;
            UGameplayStatics::ApplyDamage(*It,Damage,GetController(),this,UDamageType::StaticClass());
        }
        ClientPlayCombatCue(5);
    }
    else if(Kind==SwarmLoot::Boon||Kind==SwarmLoot::Supercar)
    {
        WorldBoonType=Kind==SwarmLoot::Supercar?3:FMath::RandRange(0,3);
        WorldBoonEnd=GetCombatTime()+15.f*PermanentBoonMultiplier;
        ++RunBoonsCollected;
        ClientRecordWorldProgress(0,WorldBoonType==3,RunBoonsCollected,false);
        ClientPlayCombatCue(5); ForceNetUpdate();
    }
}

void AAnimaPreviewCharacter::ClientRecordWorldProgress_Implementation(int32 Bombs,bool bSupercar,int32 Boons,bool bHardAatrox)
{
#if WITH_EDITOR
    int32 DiagnosticCharacter=0;
    if(FParse::Value(FCommandLine::Get(),TEXT("SwarmSkillVisual="),DiagnosticCharacter)
        ||FParse::Param(FCommandLine::Get(),TEXT("SwarmGrowthNetworkReview"))) { return; }
#endif
    UDistrictProgress::RecordWorldProgress(Bombs,bSupercar,Boons,bHardAatrox);
}

void AAnimaPreviewCharacter::ServerSetAugmentUnlocks_Implementation(bool bStory,bool bExtreme)
{
    if(bAugmentUnlocksSubmitted||bDistrictFinished) { return; }
    bAugmentUnlocksSubmitted=true; bStoryAugmentsUnlocked=bStory; bExtremeAugmentsUnlocked=bExtreme;
    ForceNetUpdate();
}

void AAnimaPreviewCharacter::ClientRecordDifficultyClear_Implementation(int32 Arena,int32 Difficulty)
{
#if WITH_EDITOR
    int32 DiagnosticCharacter=0;
    if(FParse::Value(FCommandLine::Get(),TEXT("SwarmSkillVisual="),DiagnosticCharacter)
        ||FParse::Param(FCommandLine::Get(),TEXT("SwarmGrowthNetworkReview"))) { return; }
#endif
    UDistrictProgress::RecordDifficultyClear(Arena,Difficulty);
}

int32 AAnimaPreviewMode::GetDifficulty() const
{ return FMath::Clamp(UGameplayStatics::GetIntOption(OptionsString,TEXT("Difficulty"),0),0,2); }

void AAnimaPreviewCharacter::UpdateSupercarVisual()
{
    if(GetNetMode()==NM_DedicatedServer) { return; }
    const bool Active=WorldBoonType==3&&GetWorldBoonRemaining()>0.f&&CombatHealth>0.f&&!bDistrictFinished;
    if(Active&&!SupercarVisual)
    {
        if(!SupercarAsset) { SupercarAsset=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/TroubleSquad/CombatVisuals/SM_Supercar.SM_Supercar")); }
        if(SupercarAsset)
        {
            SupercarVisual=NewObject<UStaticMeshComponent>(this); SupercarVisual->SetupAttachment(GetRootComponent());
            SupercarVisual->SetStaticMesh(SupercarAsset); SupercarVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            const FBox Bounds=SupercarAsset->GetBoundingBox(); const float Scale=210.f/FMath::Max(1.f,Bounds.GetSize().GetMax());
            SupercarVisual->SetRelativeScale3D(FVector(Scale));
            SupercarVisual->SetRelativeLocation(FVector(0,0,-GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-Bounds.Min.Z*Scale+12.f));
            SupercarVisual->RegisterComponent();
        }
    }
    if(SupercarVisual) { SupercarVisual->SetVisibility(Active); }
    if(Active&&SupercarVisual) { GetMesh()->SetVisibility(false); bSupercarMeshHidden=true; }
    else if(bSupercarMeshHidden) { GetMesh()->SetVisibility(true); bSupercarMeshHidden=false; }
}

void AAnimaPreviewMode::RegisterBoonSpireKill(FVector Position)
{
    if(BoonKills>=0&&FVector::DistSquared2D(Position,BoonCenter)<=FMath::Square(650.f)) { BoonKills=FMath::Min(25,BoonKills+1); }
}

void AAnimaPreviewMode::TickBoonSpire(float Now)
{
    static constexpr float Starts[]={120.f,240.f,360.f,480.f};
    if(BoonKills<0&&NextBoonSpire<UE_ARRAY_COUNT(Starts)&&Now>=Starts[NextBoonSpire]&&Now<550.f)
    {
        for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
        { if(It->GetController()&&It->GetCombatHealth()>0.f) { BoonCenter=It->GetActorLocation(); BoonKills=0; BoonEndsAt=Now+60.f; ++NextBoonSpire; break; } }
    }
    const bool Success=BoonKills>=25;
    const bool End=BoonKills>=0&&(Success||Now>=BoonEndsAt||Now>=550.f);
    for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
    {
        It->BoonSpireKills=End?-1:BoonKills;
        It->BoonSpireRemaining=BoonKills<0||End?0.f:FMath::Max(0.f,BoonEndsAt-Now);
        It->BoonSpireCenter=BoonCenter;
        if(Success) { It->CollectWorldPowerup(SwarmLoot::Boon); }
    }
    if(End) { BoonKills=-1; }
}

#if WITH_EDITOR
void AAnimaPreviewMode::RunBoonSpireAcceptanceTest()
{
    NextBoonSpire=0; BoonKills=-1; TickBoonSpire(180.f);
    const bool Started=BoonKills==0;
    RegisterBoonSpireKill(BoonCenter+FVector(2000,0,0)); const bool Outside=BoonKills==0;
    for(int32 I=0;I<25;++I) { RegisterBoonSpireKill(BoonCenter); }
    TickBoonSpire(181.f);
    int32 Rewards=0; for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It) { if(It->GetWorldBoonRemaining()>0.f&&It->RunBoonsCollected==1) { ++Rewards; } }
    const bool Success=BoonKills==-1&&Rewards>0;
    TickBoonSpire(360.f); TickBoonSpire(421.f);
    const bool Timeout=BoonKills==-1;
    UE_LOG(LogTemp,Display,TEXT("Boon spire acceptance: started=%d outside=%d success=%d timeout=%d rewards=%d"),Started,Outside,Success,Timeout,Rewards);
}
#endif

void AAnimaPreviewMode::TickLootCrates(float Now)
{
    if(Now<NextLootCrateAt||Now>=880.f) { return; }
    NextLootCrateAt=Now+25.f;
    int32 Count=0; for(TActorIterator<ASwarmLootCrate> It(GetWorld());It;++It) { ++Count; }
    if(Count>=12) { return; }
    for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
    {
        if(!It->GetController()||It->GetCombatHealth()<=0.f) { continue; }
        for(int32 Attempt=0;Attempt<8;++Attempt)
        {
            const FVector Candidate=It->GetActorLocation()+FVector::ForwardVector.RotateAngleAxis(FMath::FRandRange(-180.f,180.f),FVector::UpVector)*FMath::FRandRange(650.f,1050.f);
            FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(LootCrateGround),false,*It);
            if(!GetWorld()->LineTraceSingleByObjectType(Hit,Candidate+FVector(0,0,300),Candidate-FVector(0,0,500),FCollisionObjectQueryParams(ECC_WorldStatic),Query)||Hit.ImpactNormal.Z<.8f) { continue; }
            FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
            if(GetWorld()->SpawnActor<ASwarmLootCrate>(Hit.ImpactPoint+FVector(0,0,42),FRotator(0,FMath::FRandRange(-180.f,180.f),0),Spawn)) { return; }
        }
    }
}
