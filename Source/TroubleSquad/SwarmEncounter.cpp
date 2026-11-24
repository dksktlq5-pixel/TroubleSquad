#include "SwarmEncounter.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "SwarmLoot.h"
#include "SwarmBolt.h"
#include "SwarmArenaGate.h"
#include "SwarmArenas.h"
#include "SwarmEncounterTuning.h"
#include "SwarmEnemyArchetypes.h"
#include "CombatAudio.h"
#include "CombatHUD.h"
#include "Misc/ConfigCacheIni.h"
#include "DistrictProgress.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameStateBase.h"

#include "AnimaPreview.h"
#include "GeneralWeapons.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Misc/ScopeExit.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Misc/CommandLine.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
struct FEnemyStats
{
    float Health;
    float Speed;
    float Damage;
    int32 Experience;
    FLinearColor Color;
};
const TCHAR* const CommonEnemyMeshes[] = { TEXT("Swarmer"), TEXT("Pix"), TEXT("Ghoul"), TEXT("Fuemigo"),
    TEXT("Red"), TEXT("Fast"), TEXT("Exploder"), TEXT("Shy"), TEXT("Divider"), TEXT("Spawner") };
const TCHAR* const HeavyEnemyMeshes[] = { TEXT("Tibbers"), TEXT("Daisy"), TEXT("Shield"), TEXT("Hunter"),
    TEXT("Lancer"), TEXT("Minidasher"), TEXT("MiniReksai") };
const TCHAR* const SpecialEnemyMeshes[] = { TEXT("Shark"), TEXT("Toad"), TEXT("Spider"), TEXT("Beetle"),
    TEXT("Crusher"), TEXT("Artillery"), TEXT("Seahorse"), TEXT("Loot") };

const TCHAR* EnemyMeshName(uint8 Kind, uint8 Variant)
{
    switch (Kind)
    {
    case 0: return CommonEnemyMeshes[Variant % UE_ARRAY_COUNT(CommonEnemyMeshes)];
    case 1: return HeavyEnemyMeshes[Variant % UE_ARRAY_COUNT(HeavyEnemyMeshes)];
    case 2: return SpecialEnemyMeshes[Variant % UE_ARRAY_COUNT(SpecialEnemyMeshes)];
    default: return nullptr;
    }
}

const FEnemyStats Stats[] = {
    {55.f, 245.f, 14.f, 4, FLinearColor(.04f,.42f,1.f)},
    {115.f, 190.f, 23.f, 9, FLinearColor(.13f,.90f,.41f)},
    {1400.f, 150.f, 105.f, 28, FLinearColor(.83f,.31f,1.f)},
    {5400.f, 285.f, 220.f, 0, FLinearColor(.42f,.07f,.59f)},
    {520.f, 0.f, 0.f, 0, FLinearColor(.74f,.19f,.32f)}
};

void ColorBody(UStaticMeshComponent* Body, FLinearColor Color)
{
    if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Material, Body);
        Instance->SetVectorParameterValue(TEXT("Color"), Color);
        Body->SetMaterial(0, Instance);
    }
}
}

ASwarmEnemy::ASwarmEnemy()
{
    bReplicates = true;
    SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick = true;
    Tags.Add(TEXT("Enemy"));
    GetCharacterMovement()->bRunPhysicsWithNoController = true;
    GetCapsuleComponent()->InitCapsuleSize(35.f, 66.f);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnemyPrototypeMesh"));
    Body->SetupAttachment(GetRootComponent());
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    PrototypeBody = Sphere.Object;
    Body->SetStaticMesh(Sphere.Object);
}

void ASwarmEnemy::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
    {
        int32 Players=0;
        for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It) { if(It->GetController()&&It->GetCombatHealth()>0.f) { ++Players; } }
        const auto Stage=GetSwarmEncounterStage(GetWorld()->GetTimeSeconds());
        const float Base=GetMaxHealth();
        const auto* Mode=GetWorld()->GetAuthGameMode<AAnimaPreviewMode>();
        const int32 Difficulty=Mode?Mode->GetDifficulty():0;
        SpawnMaximumHealth=Base*(Kind==3?10.f:Kind<3?Stage.Health:1.f)*GetSwarmTeamHealthMultiplier(Players)*GetSwarmDifficultyHealth(Difficulty);
        SpawnDamageMultiplier=(Kind<3?Stage.Damage:1.f)*GetSwarmDifficultyDamage(Difficulty);
        NextOffspringAt=GetWorld()->GetTimeSeconds()+6.f;
        Health = GetMaxHealth();
        if (Kind == 3 && GetArenaIndex() == 0) { EnterBurrow(); }
        if (Kind == 3)
        {
            Tags.AddUnique(TEXT("FinalBoss"));
            if (GetArenaIndex() == 3)
            {
                FActorSpawnParameters GateSpawn;
                GateSpawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                BeachheadGate = GetWorld()->SpawnActor<ASwarmArenaGate>(FVector(1050.f, 0.f, 125.f),
                    FRotator::ZeroRotator, GateSpawn);
            }
        }
    }
    OnRep_Kind();
    if (HasAuthority() && Kind == 3) { PlayEnemyCue(4, GetActorLocation()); }
}

void ASwarmEnemy::EndPlay(const EEndPlayReason::Type Reason)
{
    if (HasAuthority() && Reason == EEndPlayReason::Destroyed)
    {
        if (BeachheadGate.IsValid()) { BeachheadGate->Destroy(); }
        if (Tunnel.IsValid()) { Tunnel->Destroy(); }
    }
    Super::EndPlay(Reason);
}

void ASwarmEnemy::PlayEnemyCue_Implementation(uint8 Cue, FVector_NetQuantize Location)
{
    if(Cue==8)
    {
        if(auto* Local=Cast<AAnimaPreviewCharacter>(UGameplayStatics::GetPlayerPawn(this,0))) { Local->RenderEnemyBurst(Location,200.f); }
        CombatAudio::PlayWorld(GetWorld(),TEXT("Enemies/Exploder_Explosion"),Location,.30f); return;
    }
    if(Cue==1)
    {
        static const TCHAR* BossNames[]={TEXT("Reksai"),TEXT("Briar"),TEXT("Belveth"),TEXT("Aatrox")};
        const TCHAR* Name=Kind==3?BossNames[GetArenaIndex()]:EnemyMeshName(Kind,ModelVariant);
        const FString Specific=Name?FString::Printf(TEXT("Enemies/Variants/%s_Death"),Name):FString();
        const FString DeathCue=!Specific.IsEmpty()&&CombatAudio::Load(*Specific)?Specific:FString::Printf(TEXT("Enemies/Minion_Death_%d"),FMath::RandRange(0,2));
        CombatAudio::PlayWorld(GetWorld(),*DeathCue,Location,Kind==3?.38f:.14f); return;
    }
    static const TCHAR* Cues[] = { TEXT("Enemies/Minion_Hit"), TEXT("Enemies/Minion_Death"),
        TEXT("Enemies/Elite_Windup"), TEXT("Enemies/Elite_Impact"), TEXT("Enemies/Boss_Entrance"),
        TEXT("Enemies/Boss_Shockwave") };
    if (Cue == 7)
    {
        if (GetNetMode() == NM_DedicatedServer) { return; }
        static TWeakObjectPtr<UParticleSystem> DiveImpact;
        if (!DiveImpact.IsValid())
        {
            DiveImpact = LoadObject<UParticleSystem>(nullptr,
                TEXT("/Game/Realistic_Starter_VFX_Pack_Vol2/Particles/Explosion/P_Explosion_Big_A.P_Explosion_Big_A"));
        }
        if (UParticleSystem* Template = DiveImpact.Get())
        {
            if (auto* Impact = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Template, Location,
                FRotator::ZeroRotator, FVector(1.1f), true))
            {
                FTimerHandle Cleanup;
                GetWorldTimerManager().SetTimer(Cleanup, FTimerDelegate::CreateWeakLambda(Impact,
                    [Impact]() { Impact->DestroyComponent(); }), 1.35f, false);
            }
        }
        Cue = 5;
    }
    if (Cue == 0 || Cue == 6) { HitPoseUntil = GetWorld()->GetTimeSeconds() + .16; }
    if(Kind==3&&(Cue==4||Cue==5))
    {
        static const TCHAR* Names[]={TEXT("Reksai"),TEXT("Briar"),TEXT("Belveth"),TEXT("Aatrox")};
        const FString Specific=FString::Printf(TEXT("Enemies/Variants/%s_%s"),Names[GetArenaIndex()],Cue==5?TEXT("Shockwave"):(GetArenaIndex()==3&&bSecondPhase?TEXT("Revive"):TEXT("Entrance")));
        CombatAudio::PlayWorld(GetWorld(),*Specific,Location,.55f);return;
    }
    if(Cue==0||Cue==2||Cue==3||Cue==5)
    {
        static const TCHAR* BossNames[]={TEXT("Reksai"),TEXT("Briar"),TEXT("Belveth"),TEXT("Aatrox")};const TCHAR* Name=Kind==3?BossNames[GetArenaIndex()]:EnemyMeshName(Kind,ModelVariant);
        if(Name) {const FString Specific=FString::Printf(TEXT("Enemies/Variants/%s_%s"),Name,Cue==0?TEXT("Hit"):Cue==2?TEXT("Windup"):TEXT("Attack"));if(CombatAudio::Load(*Specific)) {CombatAudio::PlayWorld(GetWorld(),*Specific,Location,Cue==0?.07f:Kind==3?.66f:.25f);return;}}
    }

    if (Cue < UE_ARRAY_COUNT(Cues)) { CombatAudio::PlayWorld(GetWorld(), Cues[Cue], Location, Cue < 2 ? .24f : .61f); }
}

void ASwarmEnemy::OnRep_Kind()
{
    GetCharacterMovement()->MaxWalkSpeed = Stats[Kind].Speed*GetSwarmArchetype(Kind,ModelVariant).Speed;
    const bool bMiniBoss=Kind==2 && ModelVariant<UE_ARRAY_COUNT(SpecialEnemyMeshes);
    GetCapsuleComponent()->SetCapsuleSize(bMiniBoss?72.f:35.f,bMiniBoss?100.f:66.f);
    BodyScale = 0.f; bAnimatedLocomotion=false; GetMesh()->SetHiddenInGame(true); Body->SetHiddenInGame(false);
    Body->SetRelativeRotation(BodyUprightRotation);
    static constexpr const TCHAR* BossMeshes[] = { TEXT("Reksai"), TEXT("Briar"), TEXT("Belveth"), TEXT("Aatrox") };
    const TCHAR* Name = Kind == 3 && !bBurrowed ? BossMeshes[GetArenaIndex()] : EnemyMeshName(Kind, ModelVariant);
    if(Name && GetNetMode()!=NM_DedicatedServer)
    {
        const FString Folder=FString::Printf(TEXT("/Game/TroubleSquad/Enemies/Locomotion/%s/"),Name);
        auto* Rig=LoadObject<USkeletalMesh>(nullptr,*(Folder+TEXT("SK_")+Name+TEXT(".SK_")+Name));
        auto* Walk=LoadObject<UAnimSequence>(nullptr,*(Folder+TEXT("AN_")+Name+TEXT("_Walk.AN_")+Name+TEXT("_Walk")));
        WalkSequence=Walk;AttackSequence=LoadObject<UAnimSequence>(nullptr,*(Folder+TEXT("AN_")+Name+TEXT("_Attack.AN_")+Name+TEXT("_Attack")));
        if(Rig&&Walk)
        {
            const FBoxSphereBounds B=Rig->GetBounds();
            const float Size=Kind==3?390.f:Kind==2?(bMiniBoss?(ModelVariant==6?300.f:250.f):177.f):Kind==1?108.f:76.f;
            BodyScale=Size/FMath::Max(1.f,B.BoxExtent.GetMax()*2.f);
            BodyMinZ=B.Origin.Z-B.BoxExtent.Z;
            GetMesh()->SetSkeletalMesh(Rig);GetMesh()->SetRelativeScale3D(FVector(BodyScale));
            GetMesh()->SetRelativeRotation(FRotator::ZeroRotator);
            GetMesh()->SetRelativeLocation(FVector(0,0,-GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-BodyMinZ*BodyScale));
            GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);GetMesh()->SetCastShadow(Kind>=2);GetMesh()->SetHiddenInGame(false);
            GetMesh()->PlayAnimation(Walk,true);GetMesh()->SetPlayRate(0.f);Body->SetHiddenInGame(true);bAnimatedLocomotion=true;return;
        }
    }
    if (Name)
    {
        static TMap<FString, TWeakObjectPtr<UStaticMesh>> MeshCache;
        auto& Cached = MeshCache.FindOrAdd(Name);
        if (!Cached.IsValid())
        {
            const FString Path = FString::Printf(TEXT("/Game/TroubleSquad/Enemies/SM_%s.SM_%s"), Name, Name);
            Cached = LoadObject<UStaticMesh>(nullptr, *Path);
        }
        if (UStaticMesh* EnemyMesh = Cached.Get())
        {
            Body->SetStaticMesh(EnemyMesh);
            Body->SetMaterial(0, EnemyMesh->GetMaterial(0));
            const FBox Bounds = EnemyMesh->GetBoundingBox().TransformBy(FTransform(BodyUprightRotation));
            const float Scale = (Kind == 3 ? 390.f : Kind == 2 ? (bMiniBoss?(ModelVariant==6?300.f:250.f):177.f) : Kind == 1 ? 108.f : 76.f) /
                FMath::Max(1.f, Bounds.GetSize().GetMax());
            BodyScale = Scale;
            BodyMinZ = Bounds.Min.Z;
            Body->SetRelativeScale3D(FVector(Scale));
            Body->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - Bounds.Min.Z * Scale));
            return;
        }
    }
    Body->SetStaticMesh(PrototypeBody);
    Body->SetMaterial(0, nullptr);
    Body->SetRelativeLocation(FVector::ZeroVector);
    Body->SetRelativeScale3D(FVector(.63 + Kind * .20, .49 + Kind * .15, .68 + Kind * .23));
    ColorBody(Body, Stats[Kind].Color * .55f);
    if (Kind == 3 && bBurrowed) { Body->SetRelativeScale3D(FVector(1.2f, .85f, .20f)); }
    if (Kind == 4)
    {
        const bool bPillar = GetArenaIndex() == 3;
        Body->SetRelativeScale3D(bPillar ? FVector(.64f, .64f, 2.4f) : FVector(1.40f, 1.40f, .38f));
    }
}

void ASwarmEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASwarmEnemy, Kind);
    DOREPLIFETIME(ASwarmEnemy, TelegraphUntil);
    DOREPLIFETIME(ASwarmEnemy, TelegraphCenter);
    DOREPLIFETIME(ASwarmEnemy, AttackStart);
    DOREPLIFETIME(ASwarmEnemy, BossAttack);
    DOREPLIFETIME(ASwarmEnemy, AttackMotionStartAt);
    DOREPLIFETIME(ASwarmEnemy, AttackMotionImpactAt);
    DOREPLIFETIME(ASwarmEnemy, SeahorseChargeEnd);
    DOREPLIFETIME(ASwarmEnemy, AatroxSlamsRemaining);
    DOREPLIFETIME(ASwarmEnemy, ModelVariant);
    DOREPLIFETIME(ASwarmEnemy, bSecondPhase);
    DOREPLIFETIME(ASwarmEnemy, LabCoralsRemaining);
    DOREPLIFETIME(ASwarmEnemy, bBeachheadTetherActive);
    DOREPLIFETIME(ASwarmEnemy, bAatroxPillarDive);
    DOREPLIFETIME(ASwarmEnemy, LabChamberStartedAt);
    DOREPLIFETIME(ASwarmEnemy, bOutskirtsFieldActive);
    DOREPLIFETIME(ASwarmEnemy, PhaseStartedAt);
    DOREPLIFETIME(ASwarmEnemy, Health);
    DOREPLIFETIME(ASwarmEnemy, SpawnMaximumHealth);
    DOREPLIFETIME(ASwarmEnemy, bBurrowed);
}

int32 ASwarmEnemy::GetArenaIndex() const
{
    return FMath::Max(0, SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this)));
}

float ASwarmEnemy::GetMaxHealth() const
{
    if(SpawnMaximumHealth>0.f) { return SpawnMaximumHealth; }
    if (Kind == 2 && ModelVariant >= UE_ARRAY_COUNT(SpecialEnemyMeshes)) { return Stats[1].Health; }
    return (Kind == 3 ? Stats[Kind].Health * (1.f + .22f * GetArenaIndex()) : Stats[Kind].Health)*GetSwarmArchetype(Kind,ModelVariant).Health;
}

int32 ASwarmEnemy::GetExperienceReward() const { return GetSwarmArchetype(Kind,ModelVariant).Experience; }

void ASwarmEnemy::SpawnOffspring(int32 Count)
{
    int32 Alive=0; for(TActorIterator<ASwarmEnemy> It(GetWorld());It;++It) { if(It->GetHealth()>0.f) { ++Alive; } }
    for(int32 I=0;I<FMath::Min(Count,GetSwarmEncounterStage(GetWorld()->GetTimeSeconds()).PopulationCap-Alive);++I)
    {
        const FVector Candidate=GetActorLocation()+FVector::ForwardVector.RotateAngleAxis(I*180.f+90.f,FVector::UpVector)*100.f;
        FHitResult Hit;
        if(!GetWorld()->LineTraceSingleByObjectType(Hit,Candidate+FVector(0,0,200),Candidate-FVector(0,0,400),FCollisionObjectQueryParams(ECC_WorldStatic))||Hit.ImpactNormal.Z<.7f) { continue; }
        FTransform Transform(Hit.ImpactPoint+FVector(0,0,68));
        auto* Child=GetWorld()->SpawnActorDeferred<ASwarmEnemy>(ASwarmEnemy::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
        if(Child) { Child->Kind=0; Child->ModelVariant=0; Child->FinishSpawning(Transform); }
    }
}

float ASwarmEnemy::GetSpawnDamage() const { const float Base=Kind==2&&ModelVariant>=UE_ARRAY_COUNT(SpecialEnemyMeshes)?48.f:Stats[Kind].Damage;return Base*SpawnDamageMultiplier; }

float ASwarmEnemy::GetMissingHealthFraction_Implementation() const
{
    return FMath::Clamp(1.f - Health / GetMaxHealth(), 0.f, 1.f);
}

float ASwarmEnemy::TakeDamage(float Damage, const FDamageEvent& Event, AController* InstigatorController, AActor* Causer)
{
    if (!HasAuthority() || Health <= 0.f || Damage <= 0.f || bBurrowed) { return 0.f; }
#if WITH_EDITOR
    if(ActorHasTag(TEXT("RenderStressTarget")) && FParse::Param(FCommandLine::Get(),TEXT("SwarmRenderStressTest"))) { return 0.f; }
#endif
    if (Kind == 3 && IsLabChamberActive() && Causer && Causer->GetActorLocation().X < 1750.f) { return 0.f; }
    if (bBeachheadTetherActive && BeachheadPillar.IsValid()) { return 0.f; }
    auto* Attacker = Cast<AAnimaPreviewCharacter>(Causer);
    if(Kind==1&&GetSwarmArchetype(Kind,ModelVariant).Behavior==ESwarmBehavior::Shield&&Attacker
        &&FVector::DotProduct(GetActorForwardVector(),(Attacker->GetActorLocation()-GetActorLocation()).GetSafeNormal2D())>.4f) { Damage*=.3f; }
    if (LabCoralsRemaining > 0) { return 0.f; }
    const bool bLabTransition = Kind == 3 && GetArenaIndex() == 2 && !bSecondPhase;
    const float DamageLimit = bLabTransition ? FMath::Max(0.f, Health - GetMaxHealth() * .5f) : Health;
    const float Applied = FMath::Min(DamageLimit, Damage * (Attacker ? Attacker->GetCombatDamageMultiplier()*Attacker->GetCombatTargetDamageMultiplier(this) : 1.f));
    Health -= Applied;
    PendingDamageNumber+=Applied;
    if(PendingDamageNumber>0.f && (Health<=0.f || GetWorld()->TimeSeconds>=NextDamageNumberAt))
    { ShowDamageNumber(GetActorLocation(),PendingDamageNumber); PendingDamageNumber=0.f; NextDamageNumberAt=GetWorld()->TimeSeconds+.1f; }
    if(Attacker) { Attacker->NotifyChampionDamageDealt(Applied); }
    if (bLabTransition && Health <= GetMaxHealth() * .5f)
    {
        LabCoralsRemaining = 4;
        TelegraphUntil = 0.f;
        GetCharacterMovement()->StopMovementImmediately();
        ForceNetUpdate();
    }
    if (Kind == 3 && GetArenaIndex() == 1 && !bSecondPhase && Health > 0.f && Health <= GetMaxHealth() * .5f)
    {
        bSecondPhase = true;
        PlayEnemyCue(7, GetActorLocation() - FVector::UpVector * 55.f);
        ForceNetUpdate();
    }
    if (Kind == 3 && GetArenaIndex() == 3 && Health <= 0.f && !bSecondPhase)
    {
        bSecondPhase = true;
        AatroxSlamsRemaining = 0;
        PhaseStartedAt = GetWorld()->GetTimeSeconds();
        const FTransform PillarTransform(FVector(2050.f, -450.f, 105.f));
        auto* Pillar = GetWorld()->SpawnActorDeferred<ASwarmEnemy>(StaticClass(), PillarTransform,
            nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (Pillar)
        {
            Pillar->Kind = 4;
            Pillar->FinishSpawning(PillarTransform);
            BeachheadPillar = Pillar;
            BeachheadPillarPosition = Pillar->GetActorLocation();
            bBeachheadTetherActive = true;
            int32 Slot = 0;
            for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
            {
                if (It->GetController() && It->GetCombatHealth() > 0.f)
                {
                    It->TeleportTo(FVector(1740.f, -900.f + 280.f * Slot++, 110.f), FRotator::ZeroRotator);
                }
            }
        }
        Health = GetMaxHealth();
        SetActorLocation(FVector(-1650.f, 0.f, GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
        if (!bBeachheadTetherActive && BeachheadGate.IsValid()) { BeachheadGate->Destroy(); }
        TelegraphUntil = 0.f;
        NextAttack = GetWorld()->TimeSeconds + 2.f;
        PlayEnemyCue(4, GetActorLocation());
        ForceNetUpdate();
    }
    Super::TakeDamage(Applied, Event, InstigatorController, Causer);
    if (Applied > 0.f && Health > 0.f && Kind < 4 && GetWorld()->TimeSeconds >= NextHitFeedbackAt)
    {
        NextHitFeedbackAt = GetWorld()->TimeSeconds + .18f;
        PlayEnemyCue(Kind < 2 ? 0 : 6, GetActorLocation());
    }
    if (Health <= 0.f)
    {
        if(Kind==0&&GetSwarmArchetype(Kind,ModelVariant).Behavior==ESwarmBehavior::Divider) { SpawnOffspring(2); }
        if (Kind < 3) { PlayEnemyCue(1, GetActorLocation()); }
        auto* Killer = Cast<AAnimaPreviewCharacter>(Causer);
        if (!Killer && InstigatorController) { Killer = Cast<AAnimaPreviewCharacter>(InstigatorController->GetPawn()); }
        if (Killer && Kind != 4)
        {
            Killer->RegisterEnemyKill();
            Killer->NotifyAugmentEnemyDeath(GetActorLocation(),Kind==1||Kind==2);
            if(auto* Mode=GetWorld()->GetAuthGameMode<AAnimaPreviewMode>()) { Mode->RegisterYuumiQuestKill(GetActorLocation()); Mode->RegisterBoonSpireKill(GetActorLocation()); }
            if(Killer->HasAugment(ESwarmAugment::Vegetarian) && FMath::FRand()<.08f) { ASwarmExperienceOrb::SpawnPickup(GetWorld(),GetActorLocation(),6); }
            if(Killer->HasAugment(ESwarmAugment::DeathTaxes) && FMath::FRand()<.1f) { ASwarmExperienceOrb::SpawnGold(GetWorld(),GetActorLocation()); }
        }
        if (Kind == 3 && SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this)) != INDEX_NONE)
        {
            if (auto* Mode = GetWorld()->GetAuthGameMode<AAnimaPreviewMode>())
            {
                Mode->CompleteDistrictEncounter(true);
            }
        }
        if (Kind == 2)
        {
            if (ModelVariant % UE_ARRAY_COUNT(SpecialEnemyMeshes) == 1)
            {
                const FVector Ground = GetActorLocation() - FVector::UpVector *
                    GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
                GetWorld()->SpawnActor<ASwarmPoisonPool>(Ground, FRotator::ZeroRotator);
            }
            if (ModelVariant % UE_ARRAY_COUNT(SpecialEnemyMeshes) == 7)
            {
                for (int32 Coin = 0; Coin < 6; ++Coin)
                {
                    const FVector Offset = FRotator(0.f, 60.f * Coin, 0.f).Vector() * 75.f;
                    ASwarmExperienceOrb::SpawnGold(GetWorld(), GetActorLocation() + Offset);
                }
            }
            else if (ModelVariant < UE_ARRAY_COUNT(SpecialEnemyMeshes)) { ASwarmExperienceOrb::SpawnPickup(GetWorld(), GetActorLocation(), 7); }
        }
        if (GetExperienceReward() > 0)
        {
            const FTransform Transform(GetActorLocation() - FVector(0, 0, 38));
            auto* Orb = GetWorld()->SpawnActorDeferred<ASwarmExperienceOrb>(ASwarmExperienceOrb::StaticClass(), Transform,
                nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            if (Orb)
            {
                Orb->Kind = Kind == 2 && ModelVariant >= UE_ARRAY_COUNT(SpecialEnemyMeshes) ? 0 : Kind;
                Orb->ExperienceAmount=GetExperienceReward();
                Orb->FinishSpawning(Transform);
            }
        }
        if (Kind != 4)
        {
            for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
            {
                It->NotifyGeneralWeaponEnemyDeath(GetActorLocation());
            }
        }
        Destroy();
    }
    return Applied;
}

bool ASwarmEnemy::UsesDashAttack() const
{
    if(Kind<2) { return GetSwarmArchetype(Kind,ModelVariant).Behavior==ESwarmBehavior::Dasher; }
    if (Kind == 2) { return ModelVariant % UE_ARRAY_COUNT(SpecialEnemyMeshes) == 3; }
    const int32 Arena = GetArenaIndex();
    return Kind == 3 && ((Arena == 2 && LabCoralsRemaining == 0 && (!bSecondPhase || BossAttack % 3 != 0)) || (Arena == 1 && BossAttack % 3 != 0));
}

bool ASwarmEnemy::UsesAatroxLineSlam() const
{
    return Kind == 3 && GetArenaIndex() == 3 && !bAatroxPillarDive && BossAttack % 2 == 0;
}

void ASwarmEnemy::ResolveAatroxLineSlam(float HalfWidth)
{
    const FVector Direction = (TelegraphCenter - AttackStart).GetSafeNormal2D();
    const double Length = FVector::Dist2D(AttackStart, TelegraphCenter);
    const FVector Side = FVector::CrossProduct(FVector::UpVector, Direction);
    for (TActorIterator<AAnimaPreviewCharacter> Player(GetWorld()); Player; ++Player)
    {
        if (!Player->GetController() || Player->GetCombatHealth() <= 0.f) { continue; }
        const FVector Offset = Player->GetActorLocation() - AttackStart;
        const double Along = FVector::DotProduct(Offset, Direction);
        if (Along < 0.0 || Along > Length || FMath::Abs(FVector::DotProduct(Offset, Side)) > HalfWidth) { continue; }
        const float Applied = UGameplayStatics::ApplyDamage(*Player, GetSpawnDamage() * (bSecondPhase ? 1.35f : 1.f),
            nullptr, this, UDamageType::StaticClass());
        if (Applied > 0.f && Player->GetCombatHealth() > 0.f)
        {
            Player->LaunchCharacter(FVector::UpVector * 435.f, false, true);
        }
    }
}

float ASwarmEnemy::GetAttackRadius() const
{
    if (bAatroxPillarDive) { return 385.f; }
    if (LabCoralsRemaining > 0) { return 410.f; }
    if (UsesAatroxLineSlam()) { return bSecondPhase ? 145.f : 115.f; }
    if (AatroxSlamsRemaining > 0) { return 250.f + 55.f * (3 - AatroxSlamsRemaining) + (bSecondPhase ? 55.f : 0.f); }
    if (bBurrowed && BossAttack % 2 != 0) { return 28.f; }
    if (Kind==2&&ModelVariant==6) { return 125.f; }
    if (UsesDashAttack()) { return Kind == 2 ? 125.f : 145.f; }
    if (Kind == 2)
    {
        const int32 Variant = ModelVariant % UE_ARRAY_COUNT(SpecialEnemyMeshes);
        return Variant == 0 ? 15.f : Variant == 4 ? 295.f : Variant == 5 ? 155.f : 200.f;
    }
    if (Kind != 3) { return 200.f; }
    const int32 Arena = GetArenaIndex();
    return Arena == 1 ? 365.f : Arena == 3 ? (bSecondPhase ? 420.f : 310.f) : 245.f;
}

float ASwarmEnemy::GetArtilleryFlightProgress(double ServerTime) const
{
    constexpr float FlightTime = 1.25f;
    if (Kind != 2 || ModelVariant % UE_ARRAY_COUNT(SpecialEnemyMeshes) != 5 || TelegraphUntil <= ServerTime)
    {
        return -1.f;
    }
    return FMath::Clamp(1.f - static_cast<float>(TelegraphUntil - ServerTime) / FlightTime, 0.f, 1.f);
}

bool ASwarmEnemy::GetAttackWarning(double ServerTime, FVector& Start, FVector& End, float& Radius, bool& bDash) const
{
    if (TelegraphUntil <= ServerTime || Health <= 0.f) { return false; }
    bDash = UsesDashAttack() || UsesAatroxLineSlam() || (bBurrowed && BossAttack % 2 != 0) || (Kind == 2 && (ModelVariant % UE_ARRAY_COUNT(SpecialEnemyMeshes) == 0 || ModelVariant % UE_ARRAY_COUNT(SpecialEnemyMeshes) == 6));
    Radius = GetAttackRadius();
    End = TelegraphCenter;
    Start = FVector(AttackStart.X, AttackStart.Y, End.Z);
    return true;
}

void ASwarmEnemy::ResolveDashAttack(float Radius)
{
    const FVector End(TelegraphCenter.X, TelegraphCenter.Y, AttackStart.Z);
    SetActorLocation(End, true);
    const FVector Landed = GetActorLocation();
    for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
    {
        const FVector Point(It->GetActorLocation().X, It->GetActorLocation().Y, Landed.Z);
        if (It->GetController() && FMath::PointDistToSegment(Point, AttackStart, Landed) <= Radius)
        {
            UGameplayStatics::ApplyDamage(*It, GetSpawnDamage() * (bSecondPhase ? 1.35f : 1.f), nullptr, this, UDamageType::StaticClass());
        }
    }
}

void ASwarmEnemy::EnterBurrow()
{
    const FVector Location = GetActorLocation() - GetActorForwardVector() * 175.f;
    const FTransform Transform(Location);
    auto* Entrance = GetWorld()->SpawnActorDeferred<ASwarmEnemy>(StaticClass(), Transform,
        nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Entrance)
    {
        ExposedUntil = GetWorld()->TimeSeconds + 3.f;
        return;
    }
    Entrance->Kind = 4;
    Entrance->FinishSpawning(Transform);
    Tunnel = Entrance;
    bBurrowed = true;
    OnRep_Kind();
    ForceNetUpdate();
}

void ASwarmEnemy::FireBolt(FVector Direction, float Damage)
{
    if (Direction.IsNearlyZero()) { return; }
    if(AttackMotionImpactAt<GetWorld()->GetTimeSeconds()-.05f) {TriggerAttackPose(GetWorld()->GetTimeSeconds(),.25f);}
    FActorSpawnParameters Params;
    Params.Owner = this;
    const FVector Muzzle = GetActorLocation() + Direction * 55.f - FVector(0, 0, 22.f);
    if (auto* Bolt = GetWorld()->SpawnActor<ASwarmBolt>(Muzzle, Direction.Rotation(), Params))
    {
        Bolt->Damage = Damage;
        PlayEnemyCue(3,Muzzle);
    }
}

void ASwarmEnemy::ResolveAreaAttack(float Radius, float Damage)
{
    for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
    {
        if (It->GetController() && FVector::DistSquared2D(It->GetActorLocation(), TelegraphCenter) <= FMath::Square(Radius))
        {
            const float Applied = UGameplayStatics::ApplyDamage(*It, Damage, nullptr, this, UDamageType::StaticClass());
            if (Applied > 0.f && It->GetCombatHealth() > 0.f && Kind == 2 &&
                ModelVariant % UE_ARRAY_COUNT(SpecialEnemyMeshes) == 4)
            {
                const FVector Outward = (It->GetActorLocation() - TelegraphCenter).GetSafeNormal2D();
                It->LaunchCharacter(Outward * 260.f + FVector(0.f, 0.f, 285.f), true, true);
            }
        }
    }
}

void ASwarmEnemy::Freeze(float Duration)
{
    if (Kind >= 3) { return; }
    FrozenUntil = FMath::Max(FrozenUntil, GetWorld()->TimeSeconds + Duration);
    TelegraphUntil = 0.f;AttackMotionStartAt=AttackMotionImpactAt=0.f;SeahorseChargeEnd=0.f;ChargeTarget.Reset();ForceNetUpdate();
    GetCharacterMovement()->StopMovementImmediately();
}

bool ASwarmEnemy::GetArenaHazardBoundary(double ServerTime, float& Boundary, bool& bDangerEast) const
{
    if (Kind != 3) { return false; }
    if (IsLabChamberActive())
    {
        bDangerEast = false;
        Boundary = FMath::Min(1700.f, -4700.f + static_cast<float>(FMath::Max(0.0, ServerTime - LabChamberStartedAt)) * 390.f);
        return true;
    }
    if (bSecondPhase && PhaseStartedAt > 0.f && GetArenaIndex() == 3)
    {
        bDangerEast = true;
        Boundary = FMath::Max(-950.f, 4550.f - static_cast<float>(FMath::Max(0.0, ServerTime - PhaseStartedAt)) * 165.f);
        return true;
    }
    return false;
}

void ASwarmEnemy::StartOutskirtsField()
{
    bOutskirtsFieldActive = true;
    ForceNetUpdate();
}

bool ASwarmEnemy::GetOutskirtsField(FVector& Center, float& Radius) const
{
    if (!bOutskirtsFieldActive) { return false; }
    Center = FVector(1050.f, -700.f, 8.f);
    Radius = 1725.f;
    return true;
}

void ASwarmEnemy::StartLabChamber()
{
    LabChamberStartedAt = GetWorld()->GetTimeSeconds();
    ForceNetUpdate();
}

void AAnimaPreviewCharacter::BeginLabEvacuation(FVector Destination)
{
    if (!HasAuthority() || CombatHealth <= 0.f) { return; }
    TeleportTo(Destination, GetActorRotation());
    GetCharacterMovement()->StopMovementImmediately();
    LabSprintEnd = GetCombatTime() + 14.f;
    ForceNetUpdate();
}

void ASwarmEnemy::TickSeahorseCharge(AAnimaPreviewCharacter* NearestTarget,float Now,float DeltaTime)
{
    if(Now<NextAttack||!NearestTarget) { return; }
    auto* Movement=GetCharacterMovement();const FVector Position=GetActorLocation();
    const auto Stop=[&]() { Movement->StopMovementImmediately();Movement->MaxWalkSpeed=Stats[Kind].Speed;ChargeTarget.Reset();SeahorseChargeEnd=0.f;TelegraphUntil=0.f;NextAttack=Now+1.5f;TriggerAttackPose(Now,.1f);ForceNetUpdate(); };
    if(TelegraphUntil>0.f)
    {
        Movement->StopMovementImmediately();if(Now<TelegraphUntil) { return; }
        TelegraphUntil=0.f;SeahorseChargeEnd=Now+1.7f;ChargeSpeed=350.f;PreviousChargePosition=Position;ForceNetUpdate();
    }
    if(SeahorseChargeEnd<=0.f)
    {
        ChargeTarget=NearestTarget;AttackStart=Position;SeahorseChargeDirection=(NearestTarget->GetActorLocation()-Position).GetSafeNormal2D();
        if(SeahorseChargeDirection.IsNearlyZero()) { SeahorseChargeDirection=GetActorForwardVector(); }
        TelegraphCenter=Position+SeahorseChargeDirection*1200.f;TelegraphCenter.Z-=95.f;
        TelegraphUntil=Now+.9f;SetActorRotation(SeahorseChargeDirection.Rotation());Movement->StopMovementImmediately();PlayEnemyCue(2,Position);ForceNetUpdate();return;
    }
    if(Now>=SeahorseChargeEnd) { Stop();return; }
    for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
    {
        if(!It->GetController()||It->GetCombatHealth()<=0.f||FMath::Abs(It->GetActorLocation().Z-Position.Z)>140.f) { continue; }
        if(FMath::PointDistToSegment(FVector(It->GetActorLocation().X,It->GetActorLocation().Y,Position.Z),PreviousChargePosition,Position)<=125.f)
        {
            const float Applied=UGameplayStatics::ApplyDamage(*It,GetSpawnDamage()*1.65f,nullptr,this,UDamageType::StaticClass());
            if(Applied>0.f&&It->GetCombatHealth()>0.f) { It->LaunchCharacter(SeahorseChargeDirection*650.f+FVector(0,0,260),true,true); }
            PlayEnemyCue(3,It->GetActorLocation());Stop();return;
        }
    }
    PreviousChargePosition=Position;ChargeSpeed=FMath::Min(1000.f,ChargeSpeed+650.f*DeltaTime);Movement->MaxWalkSpeed=ChargeSpeed;
    SetActorRotation(SeahorseChargeDirection.Rotation());AddMovementInput(SeahorseChargeDirection,1.f,true);
}

FVector ASwarmEnemy::GetLabCoralGroundPosition(int32 Index) const
{
    FVector Position = GetActorLocation() + FRotator(0.f, Index * 90.f, 0.f).Vector() * 390.f - FVector::UpVector * 55.f;
    if (UGameplayStatics::GetCurrentLevelName(this, true) == TEXT("L_SubterraneanLab_Rebuilt"))
    {
        const FVector ChamberCenter(4300.f, -1150.f, Position.Z);
        Position = ChamberCenter + (Position - ChamberCenter).GetClampedToMaxSize2D(710.f);
    }
    return Position;
}

void ASwarmEnemy::UpdateLabCoralVisuals()
{
    if (GetNetMode() == NM_DedicatedServer) { return; }
    if (LabCoralsRemaining == 0)
    {
        for (UStaticMeshComponent* Coral : LabCoralVisuals) { Coral->DestroyComponent(); }
        LabCoralVisuals.Reset();
        return;
    }

    if (LabCoralVisuals.IsEmpty())
    {
        UStaticMesh* CoralMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
        if (!CoralMesh) { return; }
        for (int32 Index = 0; Index < 4; ++Index)
        {
            UStaticMeshComponent* Coral = NewObject<UStaticMeshComponent>(this);
            Coral->SetStaticMesh(CoralMesh);
            Coral->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Coral->SetCastShadow(false);
            Coral->RegisterComponent();
            ColorBody(Coral, FLinearColor(.78f, .07f, .63f));
            LabCoralVisuals.Add(Coral);
        }
    }

    const AGameStateBase* GameState = GetWorld()->GetGameState();
    const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    const int32 ActiveIndex = 4 - LabCoralsRemaining;
    for (int32 Index = 0; Index < LabCoralVisuals.Num(); ++Index)
    {
        UStaticMeshComponent* Coral = LabCoralVisuals[Index];
        Coral->SetVisibility(Index >= ActiveIndex);
        if (Index < ActiveIndex) { continue; }

        const double Absorption = Index == ActiveIndex && TelegraphUntil > 0.f
            ? FMath::Clamp(1.0 - (TelegraphUntil - Now) / .35, 0.0, 1.0) : 0.0;
        const FVector From = GetLabCoralGroundPosition(Index) + FVector::UpVector * 65.f;
        Coral->SetWorldLocation(FMath::Lerp(From, GetActorLocation(), Absorption)
            + FVector::UpVector * (90.0 * FMath::Sin(Absorption * UE_PI)));
        Coral->SetWorldScale3D(FVector(.52f, .52f, 1.45f) * (1.0 - Absorption * .7));
    }
}

void ASwarmEnemy::UpdateBodyPose()
{
    if (BodyScale <= 0.f || GetNetMode() == NM_DedicatedServer) { return; }

    const AGameStateBase* GameState = GetWorld()->GetGameState();
    const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    const double Speed = GetVelocity().Size2D();
    const double Stride = FMath::Clamp(Speed / Stats[Kind].Speed, 0.0, 1.0);
    const bool bWindingUp = TelegraphUntil > Now;
    if(bAnimatedLocomotion)
    {
        const bool Charging=SeahorseChargeEnd>Now;
        if(AttackSequence&&Health>0.f&&AttackMotionImpactAt>0.f&&(Now<=AttackMotionImpactAt+.32f||Charging))
        {
            auto* Node=GetMesh()->GetSingleNodeInstance();if(!Node||Node->GetCurrentAsset()!=AttackSequence.Get()) {GetMesh()->PlayAnimation(AttackSequence,false);}
            const float Phase=Charging?.68f:Now<AttackMotionImpactAt?.6f*FMath::Clamp(float((Now-AttackMotionStartAt)/FMath::Max(.02f,AttackMotionImpactAt-AttackMotionStartAt)),0.f,1.f):.6f+.4f*FMath::Clamp(float((Now-AttackMotionImpactAt)/.32f),0.f,1.f);
            GetMesh()->SetPosition(AttackSequence->GetPlayLength()*Phase,false);GetMesh()->SetPlayRate(0.f);return;
        }
        if(auto* Node=GetMesh()->GetSingleNodeInstance();WalkSequence&&(!Node||Node->GetCurrentAsset()!=WalkSequence.Get())) {GetMesh()->PlayAnimation(WalkSequence,true);}

        GetMesh()->SetPlayRate(bWindingUp||Speed<5.f?0.f:FMath::Clamp(Speed/250.f,.6f,2.2f));
        GetMesh()->SetRelativeRotation(FRotator(bWindingUp?-8.f:0.f,0,0));
        GetMesh()->SetRelativeLocation(FVector(0,0,-GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-BodyMinZ*BodyScale));
        return;
    }
    const double Phase = Now * (Kind == 0 ? 17.0 : Kind == 3 ? 8.0 : 12.0) + GetActorLocation().Y * .012;
    const double Sway = FMath::Sin(Phase);
    const double Bounce = FMath::Abs(Sway) * Stride * (Kind == 3 ? 9.0 : 7.0);
    const double Compression = bWindingUp ? .13 : -.025 * Sway * Stride;
    const double Recoil = FMath::Clamp((HitPoseUntil - GetWorld()->GetTimeSeconds()) / .16, 0.0, 1.0);
    const double HeightScale = BodyScale * (1.0 - Compression);
    Body->SetRelativeScale3D(FVector(BodyScale));
    Body->SetRelativeRotation((FRotator((bWindingUp ? -10.0 : 2.0 * Stride) - 8.0 * Recoil,0.0,Sway*Stride*3.0).Quaternion()*BodyUprightRotation.Quaternion()).Rotator());
    FVector BodyPosition(0.0, 0.0,
        -GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - BodyMinZ * BodyScale + Bounce);
    if (bAatroxPillarDive && bWindingUp)
    {
        constexpr double FlightSeconds = .55;
        const double Flight = FMath::Clamp(1.0 - (TelegraphUntil - Now) / FlightSeconds, 0.0, 1.0);
        const FVector Travel = (TelegraphCenter - AttackStart).GetSafeNormal2D()
            * FVector::Dist2D(TelegraphCenter, AttackStart) * Flight;
        BodyPosition += GetActorQuat().UnrotateVector(Travel);
        BodyPosition.Z += 440.0 * 4.0 * Flight * (1.0 - Flight);
    }
    Body->SetRelativeLocation(BodyPosition);
}

void ASwarmEnemy::Tick(float DeltaTime)
{
    if(HasAuthority()&&UGameplayStatics::GetCurrentLevelName(this)==TEXT("L_SubterraneanLab_Rebuilt"))
    {
        const FVector Safe=SwarmArenas::LabPositionOnLand(GetActorLocation(),GetCapsuleComponent()->GetScaledCapsuleRadius());
        if(!Safe.Equals(GetActorLocation(),.1f)) {SetActorLocation(Safe,false);GetCharacterMovement()->StopMovementImmediately();}
    }

    if(HasAuthority() && PendingDamageNumber>0.f && GetWorld()->TimeSeconds>=NextDamageNumberAt)
    { ShowDamageNumber(GetActorLocation(),PendingDamageNumber); PendingDamageNumber=0.f; NextDamageNumberAt=GetWorld()->TimeSeconds+.1f; }
    if(HasAuthority()&&TelegraphUntil>GetWorld()->GetTimeSeconds()&&!FMath::IsNearlyEqual(AttackMotionImpactAt,TelegraphUntil,.01f))
    {TriggerAttackPose(TelegraphUntil,TelegraphUntil-GetWorld()->GetTimeSeconds());if(Kind==3){PlayEnemyCue(2,GetActorLocation());}}
    Super::Tick(DeltaTime);
    UpdateBodyPose();
    UpdateLabCoralVisuals();
    if (!HasAuthority() || Health <= 0.f || Kind == 4) { return; }

    const float Now = GetWorld()->TimeSeconds;
    if (bBeachheadTetherActive)
    {
        if (const ASwarmEnemy* Pillar = BeachheadPillar.Get())
        {
            const FVector Center = Pillar->GetActorLocation();
            for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
            {
                if (!It->GetController() || It->GetCombatHealth() <= 0.f) { continue; }
                const FVector Offset = It->GetActorLocation() - Center;
                if (Offset.SizeSquared2D() > FMath::Square(850.f))
                {
                    const FVector Edge = Center + Offset.GetSafeNormal2D() * 820.f;
                    It->SetActorLocation(FVector(Edge.X, Edge.Y, It->GetActorLocation().Z), true);
                }
            }
        }
        else
        {
            bBeachheadTetherActive = false;
            bAatroxPillarDive = true;
            AttackStart = GetActorLocation();
            TelegraphCenter = BeachheadPillarPosition - FVector::UpVector * 55.f;
            TelegraphUntil = Now + 1.35f;
            NextAttack = Now + 3.15f;
            GetCharacterMovement()->StopMovementImmediately();
            if (BeachheadGate.IsValid()) { BeachheadGate->Destroy(); }
            ForceNetUpdate();
        }
    }
    FVector FieldCenter;
    float FieldRadius = 0.f;
    if (Now >= NextArenaHazardAt && GetOutskirtsField(FieldCenter, FieldRadius))
    {
        NextArenaHazardAt = Now + .55f;
        for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
        {
            if (It->GetController() && FVector::DistSquared2D(It->GetActorLocation(), FieldCenter) > FMath::Square(FieldRadius))
            {
                UGameplayStatics::ApplyDamage(*It, 110.f, nullptr, this, UDamageType::StaticClass());
            }
        }
    }
    float Boundary = 0.f;
    bool bDangerEast = false;
    if (Now >= NextArenaHazardAt && GetArenaHazardBoundary(Now, Boundary, bDangerEast))
    {
        NextArenaHazardAt = Now + (bDangerEast ? .65f : .5f);
        for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
        {
            const float X = It->GetActorLocation().X;
            if (bDangerEast ? X >= Boundary : X < Boundary)
            {
                UGameplayStatics::ApplyDamage(*It, bDangerEast ? 150.f : 95.f, nullptr, this, UDamageType::StaticClass());
            }
        }
    }
    if (Now < FrozenUntil || GetCharacterMovement()->MovementMode == MOVE_None) { return; }
    const bool bBriarFrenzy = Kind == 3 && GetArenaIndex() == 1 && bSecondPhase;
    if (bBriarFrenzy && !GetCharacterMovement()->ComponentHasTag(TEXT("SeraphineWaveSlow")))
    {
        GetCharacterMovement()->MaxWalkSpeed = Stats[Kind].Speed * 1.38f;
    }
    if (Kind == 3 && GetArenaIndex() == 0)
    {
        if (bBurrowed && !Tunnel.IsValid())
        {
            bBurrowed = false;
            ExposedUntil = Now + 12.f;
            OnRep_Kind();
            ForceNetUpdate();
        }
        else if (!bBurrowed && Now >= ExposedUntil)
        {
            EnterBurrow();
        }
    }

    if (LabCoralsRemaining > 0)
    {
        if (TelegraphUntil <= 0.f)
        {
            AttackStart = GetActorLocation();
            TelegraphCenter = GetLabCoralGroundPosition(4 - LabCoralsRemaining);
            TelegraphUntil = Now + 1.1f;
            ForceNetUpdate();
        }
        else if (Now >= TelegraphUntil)
        {
            ResolveAreaAttack(GetAttackRadius(), GetSpawnDamage());
            PlayEnemyCue(5, TelegraphCenter);
            TelegraphUntil = 0.f;
            if (--LabCoralsRemaining == 0)
            {
                bSecondPhase = true;
                NextAttack = Now + .8f;
            }
            ForceNetUpdate();
        }
        return;
    }

    if(Kind==2&&ModelVariant%UE_ARRAY_COUNT(SpecialEnemyMeshes)==6)
    {
        AAnimaPreviewCharacter* Nearest=nullptr;double BestCharge=TNumericLimits<double>::Max();
        for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It) { const double D=FVector::DistSquared2D(GetActorLocation(),It->GetActorLocation());if(It->GetController()&&It->GetCombatHealth()>0.f&&D<BestCharge) {Nearest=*It;BestCharge=D;} }
        if(Nearest) { TickSeahorseCharge(Nearest,Now,DeltaTime); } return;
    }
    if (TelegraphUntil > 0.f)
    {
        const float Radius = GetAttackRadius();
        if (Now < TelegraphUntil) { return; }
        TelegraphUntil = 0.f;
        PlayEnemyCue(bAatroxPillarDive ? 7 : Kind == 3 ? 5 : 3, TelegraphCenter);
        if (bAatroxPillarDive)
        {
            TeleportTo(BeachheadPillarPosition, GetActorRotation());
            ResolveAreaAttack(Radius, GetSpawnDamage() * 1.35f);
            bAatroxPillarDive = false;
            ForceNetUpdate();
        }
        else if (bBurrowed && BossAttack % 2 != 0)
        {
            FireBolt((TelegraphCenter - GetActorLocation()).GetSafeNormal2D(), GetSpawnDamage());
        }
        else if (UsesDashAttack()) { ResolveDashAttack(Radius); }
        else if (UsesAatroxLineSlam()) { ResolveAatroxLineSlam(Radius); }
        else if (Kind == 2 && ModelVariant % UE_ARRAY_COUNT(SpecialEnemyMeshes) == 0)
        {
            const FVector Direction = (TelegraphCenter - GetActorLocation()).GetSafeNormal2D();
            for (int32 Shot = 0; Shot < 3; ++Shot)
            {
                const auto Fire = [this, Direction]() { FireBolt(Direction, GetSpawnDamage() * .65f); };
                if (Shot == 0) { Fire(); }
                else
                {
                    FTimerHandle Delay;
                    GetWorldTimerManager().SetTimer(Delay, FTimerDelegate::CreateWeakLambda(this, Fire), .17f * Shot, false);
                }
            }
        }
        else if (Kind == 2 && ModelVariant % UE_ARRAY_COUNT(SpecialEnemyMeshes) == 2)
        {
            for (int32 Shot = 0; Shot < 8; ++Shot)
            {
                const FVector Direction = GetActorForwardVector().RotateAngleAxis(45.f * Shot, FVector::UpVector);
                FireBolt(Direction, GetSpawnDamage() * .60f);
            }
        }
        else { ResolveAreaAttack(Radius, GetSpawnDamage() * (Kind==0&&GetSwarmArchetype(Kind,ModelVariant).Behavior==ESwarmBehavior::Exploder?1.6f:bSecondPhase?1.35f:1.f)); }
        if(Kind==3 && BossAttack%(IsBossEnraged()?2:3)==0) { SpawnBossPressurePattern(); }
        if(Kind==0&&GetSwarmArchetype(Kind,ModelVariant).Behavior==ESwarmBehavior::Exploder)
        {
            PlayEnemyCue(8,GetActorLocation());
            auto* Killer=ChargeTarget.Get();
            UGameplayStatics::ApplyDamage(this,GetMaxHealth()*100.f,Killer?Killer->GetController():nullptr,Killer,UDamageType::StaticClass());
            return;
        }
        if (AatroxSlamsRemaining > 0 && --AatroxSlamsRemaining > 0)
        {
            const FVector Direction = (TelegraphCenter - AttackStart).GetSafeNormal2D();
            TelegraphCenter += Direction * 115.f;
            TelegraphUntil = Now + (bSecondPhase ? .58f : .78f);
            ForceNetUpdate();
            return;
        }
    }
    AAnimaPreviewCharacter* Target = nullptr;
    double Best = TNumericLimits<double>::Max();
    for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
    {
        if (IsLabChamberActive() && It->GetActorLocation().X < 1750.f) { continue; }
        const double Distance = FVector::DistSquared2D(GetActorLocation(), It->GetActorLocation());
        if (It->GetController() && It->GetCombatHealth() > 0.f && Distance < Best)
        {
            Best = Distance;
            Target = *It;
        }
    }
    if (!Target) { return; }
    FVector PursuitPoint = Target->GetActorLocation();
    FVector ArenaCenter;
    float ArenaRadius = 0.f;
    if (GetOutskirtsField(ArenaCenter, ArenaRadius))
    {
        constexpr float BoundaryClearance = 110.f;
        const FVector Offset = PursuitPoint - ArenaCenter;
        if (Offset.SizeSquared2D() > FMath::Square(ArenaRadius - BoundaryClearance))
        {
            const FVector Edge = ArenaCenter + Offset.GetSafeNormal2D() * (ArenaRadius - BoundaryClearance);
            PursuitPoint.X = Edge.X;
            PursuitPoint.Y = Edge.Y;
        }
    }
    if(Kind<3 && UGameplayStatics::GetCurrentLevelName(this)==TEXT("L_SubterraneanLab_Rebuilt"))
    {
        const FVector Crossing=SwarmArenas::LabTransitWaypoint(GetActorLocation(),PursuitPoint);
        if(!Crossing.Equals(PursuitPoint,1.f))
        {
            FVector BridgeDirection=(Crossing-GetActorLocation()).GetSafeNormal2D();
            FCollisionQueryParams Avoidance(SCENE_QUERY_STAT(LabEquipmentAvoidance),false,this);
            const FVector ProbeOrigin=GetActorLocation()+FVector(0,0,55.f);
            FHitResult Obstacle;
            if(GetWorld()->SweepSingleByChannel(Obstacle,ProbeOrigin,ProbeOrigin+BridgeDirection*180.f,
                FQuat::Identity,ECC_WorldStatic,FCollisionShape::MakeSphere(25.f),Avoidance))
            {
                const float Side=(ModelVariant&1)?1.f:-1.f;
                for(float Angle:{Side*35.f,-Side*35.f,Side*70.f,-Side*70.f,Side*100.f,-Side*100.f})
                {
                    const FVector Candidate=BridgeDirection.RotateAngleAxis(Angle,FVector::UpVector);
                    if(!GetWorld()->SweepSingleByChannel(Obstacle,ProbeOrigin,ProbeOrigin+Candidate*180.f,
                        FQuat::Identity,ECC_WorldStatic,FCollisionShape::MakeSphere(25.f),Avoidance))
                    { BridgeDirection=Candidate;break; }
                }
            }
            SetActorRotation(BridgeDirection.Rotation());AddMovementInput(BridgeDirection,1.f,true);return;
        }
    }
    FVector Direction = (PursuitPoint - GetActorLocation()).GetSafeNormal2D();
    if(Kind<3&&UGameplayStatics::GetCurrentLevelName(this)==TEXT("L_SubterraneanLab_Rebuilt"))
    {
        const float ProbeRadius=GetCapsuleComponent()->GetScaledCapsuleRadius()+5.f;
        const FVector Probe=GetActorLocation()+FVector(0,0,ProbeRadius);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(LabLandEquipmentAvoidance),false,this);FHitResult Hit;
        if(GetWorld()->SweepSingleByObjectType(Hit,Probe,Probe+Direction*190.f,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeSphere(ProbeRadius),Query))
        {
            const float Side=(ModelVariant&1)?1.f:-1.f;
            for(float Angle:{Side*40.f,-Side*40.f,Side*75.f,-Side*75.f,Side*110.f,-Side*110.f})
            {const FVector Candidate=Direction.RotateAngleAxis(Angle,FVector::UpVector);if(!GetWorld()->SweepSingleByObjectType(Hit,Probe,Probe+Candidate*190.f,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeSphere(ProbeRadius),Query)) {Direction=Candidate;break;}}
        }
    }
    const auto Archetype=GetSwarmArchetype(Kind,ModelVariant);
    if(Kind<2&&Archetype.Behavior==ESwarmBehavior::Ranged)
    {
        SetActorRotation(Direction.Rotation());
        if(Best<FMath::Square(300.f)) { AddMovementInput(-Direction,1.f,true); }
        else if(Best>FMath::Square(650.f)) { AddMovementInput(Direction,1.f,true); }
        if(Now>=NextAttack&&Best<=FMath::Square(1000.f)) { NextAttack=Now+2.2f; FireBolt(Direction,GetSpawnDamage()*(Kind==1?1.2f:.65f)); }
        return;
    }
    if(Kind==0&&Archetype.Behavior==ESwarmBehavior::Spawner&&OffspringWaves<3&&Now>=NextOffspringAt)
    { NextOffspringAt=Now+8.f; ++OffspringWaves; SpawnOffspring(2); }
    const int32 SpecialVariant = Kind == 2 ? ModelVariant % UE_ARRAY_COUNT(SpecialEnemyMeshes) : INDEX_NONE;
    if (SpecialVariant == 6)
    {
        TickSeahorseCharge(Target, Now, DeltaTime);
        return;
    }
    if (SpecialVariant == 7)
    {
        SetActorRotation((-Direction).Rotation());
        AddMovementInput(-Direction, 1.f, true);
        return;
    }
    SetActorRotation(Direction.Rotation());
    if (bBurrowed && Best <= FMath::Square(115.f) && Now >= NextBurrowContactAt)
    {
        NextBurrowContactAt = Now + 1.f;
        UGameplayStatics::ApplyDamage(Target, GetSpawnDamage(), nullptr, this, UDamageType::StaticClass());
    }
    const bool bRangedSpecial = SpecialVariant == 0 || SpecialVariant == 2 || SpecialVariant == 3 || SpecialVariant == 5;
    const bool RoleTelegraph=Kind<2&&(Archetype.Behavior==ESwarmBehavior::Dasher||Archetype.Behavior==ESwarmBehavior::Exploder||Archetype.Behavior==ESwarmBehavior::Heavy);
    const float AttackRange=RoleTelegraph?(UsesDashAttack()?650.f:Archetype.Behavior==ESwarmBehavior::Exploder?180.f:280.f):Kind==3?(GetArenaIndex()==2?1050.f:690.f):bRangedSpecial?780.f:SpecialVariant==4?290.f:220.f;
    if ((Kind >= 2||RoleTelegraph) && Now >= NextAttack && Best <= FMath::Square(AttackRange))
    {
        NextAttack = Now + (bBriarFrenzy ? 1.35f : Kind == 3 ? (IsBossEnraged()?1.6f:2.2f) : 3.2f);
        ++BossAttack;
        if (Kind == 3 && GetArenaIndex() == 3 && BossAttack % 2 != 0)
        {
            AatroxSlamsRemaining = 3;
        }
        AttackStart = GetActorLocation();
        TelegraphUntil = Now + (SpecialVariant == 5 ? 1.25f : SpecialVariant == 3 ? 1.05f : Kind==3 ? (IsBossEnraged()?.75f:1.f) : .85f);
        if (Kind != 3) { PlayEnemyCue(2, GetActorLocation()); }
        TelegraphCenter = UsesDashAttack() || (Kind == 3 && !(GetArenaIndex() == 2 && !UsesDashAttack())) || (bRangedSpecial && SpecialVariant != 2) ? PursuitPoint : GetActorLocation();
        if(Kind==0&&Archetype.Behavior==ESwarmBehavior::Exploder) { ChargeTarget=Target; }
        if (UsesAatroxLineSlam())
        {
            const FVector SlamDirection = Direction.IsNearlyZero() ? GetActorForwardVector() : Direction;
            TelegraphCenter = AttackStart + SlamDirection * (bSecondPhase ? 1140.f : 930.f);
        }
        TelegraphCenter.Z -= 55.f;
        ForceNetUpdate();
        GetCharacterMovement()->StopMovementImmediately();
        return;
    }
    if (FVector::DistSquared2D(PursuitPoint, GetActorLocation()) > FMath::Square(100.f))
    {
        AddMovementInput(Direction, 1.f, true);
    }
    else if (Kind < 2 && Now >= NextAttack)
    {
        NextAttack = GetWorld()->TimeSeconds + 1.1f;
        TriggerAttackPose(Now,.25f);PlayEnemyCue(3,GetActorLocation());
        UGameplayStatics::ApplyDamage(Target, GetSpawnDamage(), nullptr, this, UDamageType::StaticClass());
    }
}

ASwarmExperienceOrb::ASwarmExperienceOrb()
{
    bReplicates = true;
    SetReplicateMovement(true);
    SetNetUpdateFrequency(20.f);
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval=.05f;
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExperienceSphere"));
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("PickupRoot")));
    Body->SetupAttachment(GetRootComponent());
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Body->SetStaticMesh(Sphere.Object);
}

void ASwarmExperienceOrb::SpawnGold(UWorld* World, FVector Position)
{
    SpawnPickup(World, Position, 5);
}

void ASwarmExperienceOrb::SpawnPickup(UWorld* World, FVector Position, uint8 PickupKind)
{
    if(!World || PickupKind>SwarmLoot::LastKind || Position.ContainsNaN()) { return; }
    FVector SpawnPosition=Position-FVector(0,0,38);
    if(PickupKind==6)
    {
        FHitResult Ground;FCollisionQueryParams Query(SCENE_QUERY_STAT(HealthPackGround),false);
        if(World->LineTraceSingleByObjectType(Ground,Position+FVector(0,0,100),Position-FVector(0,0,400),FCollisionObjectQueryParams(ECC_WorldStatic),Query)) {SpawnPosition=Ground.ImpactPoint+FVector(0,0,42);}
    }
    const FTransform Transform(FRotator::ZeroRotator,SpawnPosition);
    auto* Orb = World->SpawnActorDeferred<ASwarmExperienceOrb>(StaticClass(), Transform);
    if (Orb)
    {
        Orb->Kind = PickupKind;
        Orb->FinishSpawning(Transform);
    }
}

void ASwarmExperienceOrb::OnRep_Kind()
{
    if(Kind>SwarmLoot::LastKind) { return; }
    SetActorScale3D(FVector(Kind >= 5 ? .19 : .13 + Kind * .055));
    if(Kind<3&&ExperienceAmount>0)
    {
        SetActorScale3D(FVector(FMath::Clamp(.1f+.025f*FMath::Log2(1.f+ExperienceAmount),.13f,.28f)));
        ColorBody(Body,ExperienceAmount>=20?FLinearColor(.65f,.12f,1.f):ExperienceAmount>=8?FLinearColor(.1f,.9f,.35f):ExperienceAmount>=4?FLinearColor(.1f,.8f,1.f):FLinearColor(.15f,.35f,1.f));
        return;
    }
    if(Kind==6)
    {
        if(auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/TroubleSquad/CombatVisuals/Pickups/SM_HealthPack.SM_HealthPack")))
        {Body->SetStaticMesh(Mesh);SetActorScale3D(FVector(1.f));Body->SetRelativeLocation(FVector(0,0,-40));Body->SetRelativeRotation(FRotator(0,45,0));for(int32 I=0;I<Mesh->GetStaticMaterials().Num();++I) {Body->SetMaterial(I,Mesh->GetMaterial(I));}return;}
    }
    if(Kind>=8)
    {
        static const FLinearColor Colors[]={FLinearColor(1,.22f,.05f),FLinearColor(.05f,.85f,1),FLinearColor(1,.75f,.1f),FLinearColor(.9f,.15f,.8f)};
        SetActorScale3D(FVector(.28f)); ColorBody(Body,Colors[Kind-8]); return;
    }
    ColorBody(Body, Kind == 7 ? FLinearColor(.95f, .20f, .95f) : Kind == 6 ? FLinearColor(.18f, 1.f, .35f) : Kind == 5 ? FLinearColor(1.f, .67f, .08f) : Stats[Kind].Color);
}

void ASwarmExperienceOrb::BeginPlay()
{
    Super::BeginPlay();
    OnRep_Kind();
    if(!HasAuthority()) { SetActorTickEnabled(false); }
    // Pickups persist until collected or the world is unloaded. Uncollected XP must not vanish.
    SetLifeSpan(0.f);
}

void ASwarmExperienceOrb::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASwarmExperienceOrb, Kind);
    DOREPLIFETIME(ASwarmExperienceOrb, ExperienceAmount);
}

void ASwarmExperienceOrb::AttractTo(AAnimaPreviewCharacter* Collector,bool bVacuum)
{
    if(!HasAuthority() || bCollectionCommitted || IsActorBeingDestroyed() || !IsValid(Collector)
        || Collector->GetCombatHealth()<=0.f || Collector->HasFinishedDistrict() || (bVacuum&&!CanBeVacuumed())) { return; }
    // Once in flight, another magnet does not steal this pickup from its collector.
    if(MagnetCollector.IsValid() && MagnetCollector.Get()!=Collector) { return; }
    MagnetCollector=Collector;bVacuumAttraction|=bVacuum;
}

void ASwarmExperienceOrb::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if(!HasAuthority() || bCollectionCommitted || IsActorBeingDestroyed()) { return; }
    AAnimaPreviewCharacter* Collector=MagnetCollector.Get();
    if(Collector && (!Collector->GetController() || Collector->GetCombatHealth()<=0.f || Collector->HasFinishedDistrict()))
    { MagnetCollector.Reset();bVacuumAttraction=false;Collector=nullptr; }
    double Nearest=TNumericLimits<double>::Max();bool bSelectedVacuum=false;
    if(!Collector) for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
    {
        if(!It->GetController() || It->GetCombatHealth()<=0.f || It->HasFinishedDistrict()) { continue; }
        const double Distance=FVector::DistSquared2D(GetActorLocation(),It->GetActorLocation());
        const bool Vacuum=CanBeVacuumed() && It->GetVacuumRemaining()>0.f;
        // Removing the XP pickup radius must never prevent touching a healthpack, card or power-up.
        const float Radius=FMath::Max(CanBeVacuumed()?24.f:75.f,145.f*It->GetLootPickupRadiusMultiplier());
        if((Vacuum || Distance<FMath::Square(Radius)) && Distance<Nearest)
        { Collector=*It;Nearest=Distance;bSelectedVacuum=Vacuum; }
    }
    if(Collector)
    {
        AttractTo(Collector,bSelectedVacuum);
        const FVector Destination=Collector->GetActorLocation()-FVector(0,0,38.f);
        const float Speed=bVacuumAttraction?FMath::Max(1800.f,FVector::Dist(GetActorLocation(),Destination)*3.f):1150.f;
        const FVector Next=FMath::VInterpConstantTo(GetActorLocation(),Destination,DeltaTime,Speed);
        SetActorLocation(Next);
        if(FVector::DistSquared(Next,Destination)<=FMath::Square(24.f)) { CollectBy(Collector); }
    }
}

void ASwarmExperienceOrb::CollectBy(AAnimaPreviewCharacter* Collector)
{
    if (!HasAuthority() || bCollectionCommitted || IsActorBeingDestroyed() || !IsValid(Collector) || Kind>SwarmLoot::LastKind || Collector->GetCombatHealth()<=0.f || Collector->HasFinishedDistrict()) { return; }
    bCollectionCommitted=true;
    if(Kind>=8) { Collector->CollectWorldPowerup(Kind); Destroy(); return; }
    if (Kind == 7)
    {
        for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
        {
            if (It->GetController() && It->GetCombatHealth() > 0.f) { It->CollectAccessCard(); }
        }
        Destroy();
        return;
    }
    if(Kind!=6) { Collector->ClientPlayCombatCue(Kind==5?1:0); }
    if(Kind<5) { ++Collector->CareerOrbs; }
    if(Kind==6) { ++Collector->CareerHeals; }
    if(Kind==5)
    {
        for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
        { if(It->GetController()&&!It->HasFinishedDistrict()) { It->CollectRunGold(3,GetActorLocation(),*It==Collector); } }
    }
    else if(Kind==6)
    {
        for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
        { if(It->GetController()) { It->CollectWeaponHealth(); } }
    }
    else { Collector->CollectExperience(ExperienceAmount>0?ExperienceAmount:Stats[Kind].Experience,GetActorLocation(),true); }
    Destroy();
}

void AAnimaPreviewMode::BeginPlay()
{
    Super::BeginPlay();
    const int32 Arena = SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this));
    if (Arena != INDEX_NONE)
    {
        if (Arena == 2)
        {
            FActorSpawnParameters GateSpawn;
            GateSpawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            LabChamberGate = GetWorld()->SpawnActor<ASwarmArenaGate>(FVector(3550.f, 0.f, 125.f),
                FRotator::ZeroRotator, GateSpawn);
        }
        RunStartedAt = GetWorld()->TimeSeconds;
#if WITH_EDITOR
        if (FParse::Param(FCommandLine::Get(), TEXT("SwarmAudioSettingsSmokeTest")))
        {
            FTimerHandle AudioSettingsTimer;
            GetWorldTimerManager().SetTimer(AudioSettingsTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
            {
                APlayerController* Player = GetWorld()->GetFirstPlayerController();
                ACombatHUD* Hud = Player ? Cast<ACombatHUD>(Player->GetHUD()) : nullptr;
                if (!Hud) { return; }
                const float Music = CombatAudio::GetMusicVolume();
                const float Effects = CombatAudio::GetEffectsVolume();
                Hud->NotifyHitBoxClick(TEXT("Settings"));
                CombatAudio::SetMusicVolume(.5f);
                CombatAudio::SetEffectsVolume(.5f);
                Hud->NotifyHitBoxClick(TEXT("Audio00"));
                Hud->NotifyHitBoxClick(TEXT("Audio11"));
                const bool bButtons = FMath::IsNearlyEqual(CombatAudio::GetMusicVolume(), .4f)
                    && FMath::IsNearlyEqual(CombatAudio::GetEffectsVolume(), .6f);
                FConfigFile SavedSettings;
                SavedSettings.Read(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config/Audio.ini")));
                float SavedMusic = -1.f;
                float SavedEffects = -1.f;
                SavedSettings.GetFloat(TEXT("TroubleSquad.Audio"), TEXT("Music"), SavedMusic);
                SavedSettings.GetFloat(TEXT("TroubleSquad.Audio"), TEXT("Effects"), SavedEffects);
                const bool bSaved = FMath::IsNearlyEqual(SavedMusic, .4f) && FMath::IsNearlyEqual(SavedEffects, .6f);
                CombatAudio::SetMusicVolume(Music);
                CombatAudio::SetEffectsVolume(Effects);
                UE_LOG(LogTemp, Display, TEXT("Audio settings smoke: buttons=%d saved=%d restored=%d diskMusic=%.2f diskEffects=%.2f"),
                    bButtons, bSaved, FMath::IsNearlyEqual(CombatAudio::GetMusicVolume(), Music)
                        && FMath::IsNearlyEqual(CombatAudio::GetEffectsVolume(), Effects), SavedMusic, SavedEffects);
            }), 4.f, false);
        }
        if (FParse::Param(FCommandLine::Get(), TEXT("SwarmAudioReadSmokeTest")))
        {
            UE_LOG(LogTemp, Display, TEXT("Audio startup values: music=%.2f effects=%.2f"),
                CombatAudio::GetMusicVolume(), CombatAudio::GetEffectsVolume());
        }
        int32 VisualCharacter = INDEX_NONE;
        if (FParse::Value(FCommandLine::Get(), TEXT("SwarmSkillVisual="), VisualCharacter))
        {
            float ReviewDelay=5.f;
            FParse::Value(FCommandLine::Get(),TEXT("VfxReviewDelay="),ReviewDelay);
            FTimerHandle SkillTimer;
            GetWorldTimerManager().SetTimer(SkillTimer, FTimerDelegate::CreateWeakLambda(this, [this, VisualCharacter]()
            {
                if (APlayerController* Player = GetWorld()->GetFirstPlayerController())
                {
                    if (auto* Character = Cast<AAnimaPreviewCharacter>(Player->GetPawn()))
                    {
                        Character->RunSkillVisualSmokeTest(VisualCharacter);
                    }
                }
            }), FMath::Clamp(ReviewDelay,1.f,60.f), false);
        }
        if (FParse::Param(FCommandLine::Get(), TEXT("SwarmRenderSmokeTest")))
        {
            FTimerHandle CaptureTimer;
            GetWorldTimerManager().SetTimer(CaptureTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
            {
                if (APlayerController* Player = GetWorld()->GetFirstPlayerController())
                {
                    Player->ConsoleCommand(TEXT("HighResShot 1"), true);
                }
            }), 6.f, false);
        }
        if (FParse::Param(FCommandLine::Get(), TEXT("SwarmBossSmokeTest"))) { RunStartedAt -= SwarmRound::BossAt-2.f; }
#endif
        GetWorldTimerManager().SetTimer(EnemyWaveTimer, this, &AAnimaPreviewMode::SpawnWaveEnemy, .25f, true, 2.f);
    }
}

void AAnimaPreviewMode::SetLevelChoicePending(AAnimaPreviewCharacter* Character,bool bPending)
{
    if(bPending) { PendingChoicePlayers.Add(Character); }
    else { PendingChoicePlayers.Remove(Character); }
    if(!PendingChoicePlayers.IsEmpty()&&!TimeDilationBeforeLevelChoice.IsSet())
    {
        TimeDilationBeforeLevelChoice=UGameplayStatics::GetGlobalTimeDilation(this);
        UGameplayStatics::SetGlobalTimeDilation(this,.0001f);GetWorldSettings()->ForceNetUpdate();
    }
    else if(PendingChoicePlayers.IsEmpty()&&TimeDilationBeforeLevelChoice.IsSet())
    {
        UGameplayStatics::SetGlobalTimeDilation(this,TimeDilationBeforeLevelChoice.GetValue());
        TimeDilationBeforeLevelChoice.Reset();GetWorldSettings()->ForceNetUpdate();
    }
}

void AAnimaPreviewMode::CheckDistrictDefeat()
{
    if (!GetWorldTimerManager().IsTimerActive(EnemyWaveTimer)) { return; }
    for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
    {
        if (It->GetController() && It->GetCombatHealth() > 0.f) { return; }
    }
    CompleteDistrictEncounter(false);
}

void AAnimaPreviewMode::CompleteDistrictEncounter(bool bVictory)
{
    ClearYuumiTokens();
    GetWorldTimerManager().ClearTimer(EnemyWaveTimer);
    if (LabChamberGate.IsValid()) { LabChamberGate->Destroy(); }
    for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
    {
        if (It->GetController()) { It->CompleteDistrictRun(bVictory); }
    }
    for (TActorIterator<ASwarmEnemy> It(GetWorld()); It; ++It)
    {
        if (It->GetHealth() > 0.f) { It->Destroy(); }
    }
}

void AAnimaPreviewCharacter::ShowLabFreezePulse(int32 Station)
{
    LabFreezePulseStation = Station;
    LabFreezePulseUntil = GetWorld()->GetTimeSeconds() + 3.0;
    ForceNetUpdate();
}

bool AAnimaPreviewCharacter::GetLabFreezePulse(double ServerTime, FVector& Center, float& Opacity) const
{
    if (LabFreezePulseStation == INDEX_NONE || ServerTime >= LabFreezePulseUntil) { return false; }
    Center = SwarmArenas::LabFreezeStations[LabFreezePulseStation] + FVector::UpVector * 20.f;
    Opacity = static_cast<float>(FMath::Clamp((LabFreezePulseUntil - ServerTime) / 3.0, 0.0, 1.0));
    return true;
}

void AAnimaPreviewMode::TickArenaGimmick(float Now)
{
    const int32 Arena = SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this));
    const FVector Battery(1800.f, 1850.f, 0.f);
    const FVector& Cannon = SwarmArenas::OutskirtsCannon;
    const auto& FreezeStations = SwarmArenas::LabFreezeStations;
    if (Arena == 1 && BatteryCarrier.IsValid() &&
        (!BatteryCarrier->GetController() || BatteryCarrier->GetCombatHealth() <= 0.f))
    {
        BatteryCarrier->SetCarryingArenaBattery(false);
        BatteryCarrier.Reset();
    }
#if WITH_EDITOR
    if (Arena == 1 && Now < 3.f && FParse::Param(FCommandLine::Get(), TEXT("SwarmBatterySmokeTest")))
    {
        for (TActorIterator<AAnimaPreviewCharacter> Player(GetWorld()); Player; ++Player)
        {
            if (!Player->GetController()) { continue; }
            const FVector Destination = Player->IsCarryingArenaBattery() ? Cannon : Battery;
            Player->SetActorLocation(FVector(Destination.X, Destination.Y, 110.f), false, nullptr,
                ETeleportType::TeleportPhysics);
            break;
        }
    }
#endif
    for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
    {
        if (!It->GetController() || It->GetCombatHealth() <= 0.f) { continue; }
        const FVector Position = It->GetActorLocation();
        if (Arena == 1 && Now >= ArenaGimmickReadyAt)
        {
            if (!BatteryCarrier.IsValid() && FVector::DistSquared2D(Position, Battery) <= FMath::Square(150.f))
            {
                BatteryCarrier = *It;
                It->SetCarryingArenaBattery(true);
                It->ClientPlayCombatCue(6);
            }
            else if (BatteryCarrier.Get() == *It && FVector::DistSquared2D(Position, Cannon) <= FMath::Square(345.f))
            {
                BatteryCarrier.Reset();
                It->SetCarryingArenaBattery(false);
                It->ClientPlayCombatCue(2);
                ArenaGimmickReadyAt = Now + 45.f;
                UE_LOG(LogTemp, Display, TEXT("Outskirts cannon fired: battery released=%d cooldown=%.1f"),
                    !It->IsCarryingArenaBattery() && !BatteryCarrier.IsValid(), ArenaGimmickReadyAt - Now);
                for (TActorIterator<ASwarmEnemy> Enemy(GetWorld()); Enemy; ++Enemy)
                {
                    if (Enemy->Kind < 3) { UGameplayStatics::ApplyDamage(*Enemy, 350.f, It->GetController(), *It, UDamageType::StaticClass()); }
                }
            }
        }
        if (Arena == 2)
        {
            for (int32 Index = 0; Index < UE_ARRAY_COUNT(FreezeStations); ++Index)
            {
                float& ReadyAt = Index == 0 ? ArenaGimmickReadyAt : SecondFreezeReadyAt;
                if (Now < ReadyAt || FVector::DistSquared2D(Position, FreezeStations[Index]) > FMath::Square(170.f)) { continue; }
                ReadyAt = Now + 45.f;
                for (TActorIterator<ASwarmEnemy> Enemy(GetWorld()); Enemy; ++Enemy)
                {
                    if (FVector::DistSquared2D(FreezeStations[Index], Enemy->GetActorLocation()) <= FMath::Square(SwarmArenas::LabFreezeRadius)) { Enemy->Freeze(3.f); }
                }
                It->ShowLabFreezePulse(Index);
                It->ClientPlayCombatCue(2);
            }
        }
    }
}

void AAnimaPreviewMode::SpawnWaveEnemy()
{
    const float Now = GetWorld()->TimeSeconds - RunStartedAt;
    TickYuumiQuest(Now);
    TickBoonSpire(Now);
    TickLootCrates(Now);
    const int32 GimmickArena = SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this));
    if (GimmickArena == 0 || UGameplayStatics::GetCurrentLevelName(this) == TEXT("L_Outskirts_Rebuilt")) { TickHealingStation(Now); }
    if (GimmickArena != 0) { TickArenaGimmick(Now); }
    const auto& Events = SwarmRound::Events;
    constexpr int32 FinalEvent = UE_ARRAY_COUNT(Events) - 1;
    if (NextEncounterEvent < FinalEvent && Now >= Events[FinalEvent])
    {
        NextEncounterEvent = FinalEvent;
    }
    const bool bTimedEvent = NextEncounterEvent <= FinalEvent && Now >= Events[NextEncounterEvent];
    if (!bTimedEvent && (NextEncounterEvent >= UE_ARRAY_COUNT(Events) || Now < NextRegularSpawn)) { return; }

    const int32 Arena = SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this));
    if (bTimedEvent && NextEncounterEvent == FinalEvent && (Arena == 1 || Arena == 2 || Arena == 3))
    {
        const bool bLab = Arena == 2;
        const FTransform Transform(bLab ? FVector(3850.f, -1150.f, 100.f) : Arena == 3 ? FVector(3240.f, 0.f, 100.f) : FVector(1450.f, -700.f, 100.f));
        auto* Boss = GetWorld()->SpawnActorDeferred<ASwarmEnemy>(ASwarmEnemy::StaticClass(),
            Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (Boss)
        {
            Boss->Kind = 3;
            Boss->FinishSpawning(Transform);
            if (bLab)
            {
                Boss->StartLabChamber();
                if (LabChamberGate.IsValid()) { LabChamberGate->Destroy(); }
            }
            else if (Arena == 1) { Boss->StartOutskirtsField(); }
            int32 Slot = 0;
            for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
            {
                if (It->GetController() && It->GetCombatHealth() > 0.f)
                {
                    const float Offset = 360.f * Slot++;
                    if (bLab) { It->BeginLabEvacuation(FVector(-4250.f, -560.f + Offset, 110.f)); }
                    else { It->TeleportTo(Arena == 3 ? FVector(2390.f, -660.f + Offset, 110.f)
                        : FVector(460.f, -1260.f + Offset, 110.f), FRotator::ZeroRotator); }
                }
            }
#if WITH_EDITOR
            if (Arena == 2 && FParse::Param(FCommandLine::Get(), TEXT("SwarmLabSmokeTest")))
            {
                const float MaximumHealth = Boss->GetMaxHealth();
                UGameplayStatics::ApplyDamage(Boss, MaximumHealth, nullptr, nullptr, UDamageType::StaticClass());
                const bool bStarted = Boss->LabCoralsRemaining == 4 && Boss->GetHealth() == MaximumHealth * .5f;
                const bool bInvulnerable = UGameplayStatics::ApplyDamage(Boss, 1.f, nullptr, nullptr,
                    UDamageType::StaticClass()) == 0.f;
                FTimerHandle VerifyCorals;
                GetWorldTimerManager().SetTimer(VerifyCorals, FTimerDelegate::CreateWeakLambda(this,
                    [WeakBoss = TWeakObjectPtr<ASwarmEnemy>(Boss), bStarted, bInvulnerable]()
                    {
                        ASwarmEnemy* LabBoss = WeakBoss.Get();
                        const bool bEvolved = LabBoss && LabBoss->IsSecondPhase() && LabBoss->LabCoralsRemaining == 0;
                        const bool bAcceptsDamage = bEvolved && UGameplayStatics::ApplyDamage(LabBoss, 1.f,
                            nullptr, nullptr, UDamageType::StaticClass()) == 1.f;
                        UE_LOG(LogTemp, Display, TEXT("Lab smoke: started=%d invulnerable=%d evolved=%d damage=%d"),
                            bStarted, bInvulnerable, bEvolved, bAcceptsDamage);
                    }), 6.f, false);
            }
            if (Arena == 3 && FParse::Param(FCommandLine::Get(), TEXT("SwarmBeachheadSmokeTest")))
            {
                const auto Strike = [](ASwarmEnemy* Enemy)
                {
                    return UGameplayStatics::ApplyDamage(Enemy, Enemy->GetMaxHealth(), nullptr, nullptr,
                        UDamageType::StaticClass());
                };
                Strike(Boss);
                const bool bPhaseChanged = Boss->IsSecondPhase() && Boss->bBeachheadTetherActive;
                const bool bBlocked = Strike(Boss) == 0.f;
                if (ASwarmEnemy* Pillar = Boss->BeachheadPillar.Get()) { Strike(Pillar); }
                FTimerHandle VerifyTransition;
                GetWorldTimerManager().SetTimer(VerifyTransition, FTimerDelegate::CreateWeakLambda(this,
                    [this, WeakBoss = TWeakObjectPtr<ASwarmEnemy>(Boss), Strike, bPhaseChanged, bBlocked]()
                    {
                        ASwarmEnemy* FinalBoss = WeakBoss.Get();
                        const bool bReleased = FinalBoss && !FinalBoss->IsBeachheadTetherActive()
                            && !FinalBoss->BeachheadGate.IsValid();
                        const bool bDiveLanded = bReleased && !FinalBoss->bAatroxPillarDive
                            && FVector::Dist2D(FinalBoss->GetActorLocation(), FinalBoss->BeachheadPillarPosition) < 100.f;
                        if (bDiveLanded) { Strike(FinalBoss); }
                        UE_LOG(LogTemp, Display, TEXT("Beachhead smoke: phase=%d blocked=%d released=%d dive=%d victory=%d"),
                            bPhaseChanged, bBlocked, bReleased, bDiveLanded, bDiveLanded && !GetWorldTimerManager().IsTimerActive(EnemyWaveTimer));
                    }), 1.75f, false);;
            }
#endif
            UE_LOG(LogTemp, Display, TEXT("Swarm final boss spawned: arena=%d time=%.1f"), Arena, Now);
            ++NextEncounterEvent;
        }
        return;
    }

    int32 Alive = 0;
    for (TActorIterator<ASwarmEnemy> It(GetWorld()); It; ++It) { ++Alive; }
    if (!bTimedEvent && Alive >= GetSwarmEncounterStage(Now).PopulationCap) { return; }
    TArray<AAnimaPreviewCharacter*> Players;
    for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
    { if(It->GetController()&&It->GetCombatHealth()>0.f) { Players.Add(*It); } }
    if(Players.IsEmpty()) { return; }
    const auto Stage=GetSwarmEncounterStage(Now);
    const int32 Burst=bTimedEvent?1:FMath::Min(Stage.Batch+FMath::Max(0,Players.Num()-1),Stage.PopulationCap-Alive);
    const float WaveAngle=FMath::FRandRange(-180.f,180.f);
    for(int32 Spawn=0;Spawn<Burst;++Spawn)
    {
    for(int32 Attempt=0;Attempt<FMath::Max(1,Players.Num())*5;++Attempt)
    {
        AAnimaPreviewCharacter* It=Players[(EnemiesSpawned+Attempt)%Players.Num()];
        if (!It->GetController() || It->GetCombatHealth() <= 0.f) { continue; }
        const FVector Origin = It->GetActorLocation();
        const FVector Candidate = Origin + FRotator(0, FMath::FRandRange(-180.f, 180.f), 0).Vector() * FMath::FRandRange(760.f,1040.f);
        FHitResult Ground;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(SwarmSpawn), false, It);
        if (!GetWorld()->LineTraceSingleByChannel(Ground, Candidate + FVector(0, 0, 100),
            Candidate - FVector(0, 0, 150), ECC_WorldStatic, Query) ||
            FMath::Abs(Ground.ImpactPoint.Z - (Origin.Z - It->GetCapsuleComponent()->GetScaledCapsuleHalfHeight())) > 50.f)
        {
            continue;
        }
        const FTransform Transform(Ground.ImpactPoint + FVector(0, 0, 69));
        auto* Enemy = GetWorld()->SpawnActorDeferred<ASwarmEnemy>(ASwarmEnemy::StaticClass(), Transform,
            nullptr, nullptr, ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
        if (Enemy)
        {
            ++EnemiesSpawned;
            Enemy->Kind = bTimedEvent ? (NextEncounterEvent == FinalEvent ? 3 : 2)
                : (Now >= 45.f && EnemiesSpawned % (Now >= 240.f ? 3 : 5) == 0 ? 1 : 0);
            const int32 ArenaSeed = FMath::Max(0, SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this)));
            Enemy->ModelVariant = static_cast<uint8>(Enemy->Kind == 2 ? NextEncounterEvent + ArenaSeed * 2
                : Enemy->Kind == 1 ? EnemiesSpawned / 3 + ArenaSeed : EnemiesSpawned - 1 + ArenaSeed);
            if (bTimedEvent && Enemy->Kind == 2)
            {
                Enemy->ModelVariant = GetSwarmTimedMiniBossVariant(NextEncounterEvent,ArenaSeed);
            }
            if (!bTimedEvent && Now >= 40.f && EnemiesSpawned % 16 < 3)
            {
                const int32 Group = EnemiesSpawned / 16;
                Enemy->Kind = 2;
                Enemy->ModelVariant = UE_ARRAY_COUNT(SpecialEnemyMeshes) + (Group + ArenaSeed) % 2;
            }
            Enemy->FinishSpawning(Transform);
#if WITH_EDITOR
            if (Arena == 0 && Enemy->Kind == 3 && FParse::Param(FCommandLine::Get(), TEXT("SwarmReksaiSmokeTest")))
            {
                const auto Strike = [](ASwarmEnemy* Target, float Damage)
                {
                    return UGameplayStatics::ApplyDamage(Target, Damage, nullptr, nullptr, UDamageType::StaticClass());
                };
                ASwarmEnemy* Entrance = Enemy->Tunnel.Get();
                const bool bProtected = Entrance && Enemy->IsBurrowed() && Strike(Enemy, 1.f) == 0.f;
                if (Entrance) { Strike(Entrance, Entrance->GetMaxHealth()); }
                const TWeakObjectPtr<ASwarmEnemy> WeakBoss(Enemy);
                FTimerHandle CheckExposure;
                GetWorldTimerManager().SetTimer(CheckExposure, FTimerDelegate::CreateWeakLambda(this,
                    [WeakBoss, Strike, bProtected]()
                    {
                        ASwarmEnemy* Boss = WeakBoss.Get();
                        const bool bExposed = Boss && !Boss->IsBurrowed() && Strike(Boss, 1.f) == 1.f;
                        UE_LOG(LogTemp, Display, TEXT("Reksai smoke: protected=%d exposed=%d"), bProtected, bExposed);
                    }), .25f, false);
                FTimerHandle CheckReburrow;
                GetWorldTimerManager().SetTimer(CheckReburrow, FTimerDelegate::CreateWeakLambda(this,
                    [WeakBoss, Strike]()
                    {
                        ASwarmEnemy* Boss = WeakBoss.Get();
                        const bool bReburrowed = Boss && Boss->IsBurrowed() && Boss->Tunnel.IsValid()
                            && Strike(Boss, 1.f) == 0.f;
                        UE_LOG(LogTemp, Display, TEXT("Reksai smoke: reburrowed=%d"), bReburrowed);
                    }), 12.5f, false);
            }
#endif
            if (EnemiesSpawned == 1)
            {
                UE_LOG(LogTemp, Display, TEXT("Swarm first enemy spawned: arena=%d time=%.1f"), ArenaSeed, Now);
            }
            if (bTimedEvent)
            {
                UE_LOG(LogTemp, Display, TEXT("Swarm timed enemy spawned: arena=%d time=%.1f kind=%d"), Arena, Now, Enemy->Kind);
                ++NextEncounterEvent;
            }
            NextRegularSpawn = Now + GetSwarmEncounterStage(Now).Interval;
        }
        if(Enemy) { break; }
    }
    }
    NextRegularSpawn=Now+Stage.Interval;
}

void AAnimaPreviewMode::TickHealingStation(float Now)
{
    if (Now < HealingReadyAt)
    {
        return;
    }

    const bool bOutskirts = UGameplayStatics::GetCurrentLevelName(this) == TEXT("L_Outskirts_Rebuilt");
    const FVector Center = bOutskirts ? FVector(3400,3400,70) : FVector(-8,-200,70);
    const float StationRadius = bOutskirts ? 300.f : 900.f;
    TArray<AAnimaPreviewCharacter*> Living;
    int32 Charging = 0;
    for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
    {
        if (!It->GetController() || It->GetCombatHealth() <= 0.f) { continue; }
        Living.Add(*It);
        Charging += FVector::DistSquared2D(It->GetActorLocation(), Center) <= FMath::Square(StationRadius) ? 1 : 0;
    }
    HealingCharge = Charging > 0 ? HealingCharge + .25f * Charging : 0.f;
    if (GetNetMode() != NM_DedicatedServer)
    {
        DrawDebugCircle(GetWorld(), Center, StationRadius, 48, Charging ? FColor::Green : FColor::Cyan,
            false, .30f, 0, 2.f, FVector::ForwardVector, FVector::RightVector, false);
    }
    if (HealingCharge >= 3.f)
    {
        for (auto* Player : Living) { Player->HealForEncounter(.50f); }
        HealingCharge = 0.f;
        HealingReadyAt = Now + 90.f;
        UE_LOG(LogTemp, Display, TEXT("Coastal encounter: healing station restored %d players"), Living.Num());
    }
}

void AAnimaPreviewCharacter::HealForEncounter(float Fraction)
{
    if (HasAuthority() && CombatHealth > 0.f)
    {
        const float PreviousHealth = CombatHealth;
        CombatHealth = FMath::Min(MaxCombatHealth, CombatHealth + MaxCombatHealth * Fraction);
        if (CombatHealth > PreviousHealth) { ClientPlayCombatCue(2); }
    }
}

int32 AAnimaPreviewCharacter::GetExperienceRequired() const
{ return GetSwarmExperienceRequired(CombatLevel); }

void AAnimaPreviewCharacter::CollectExperience(int32 Amount,FVector Origin,bool bCollectedLocally)
{
    if(!HasAuthority() || bDistrictFinished || Amount<=0 || CombatHealth<=0.f) { return; }
    if(auto* Mode=GetWorld()->GetAuthGameMode<AAnimaPreviewMode>())
    { Mode->AddTeamExperience(Amount,Origin,this,bCollectedLocally); }
}

void AAnimaPreviewMode::SyncTeamProgress(AAnimaPreviewCharacter* Character)
{
    if(!IsValid(Character) || !Character->GetController() || Character->bDistrictFinished) { return; }
    const int32 Gained=FMath::Max(0,TeamLevel-Character->CombatLevel);
    if(Character->CombatLevel>TeamLevel) { Character->CombatLevel=TeamLevel; }
    else { Character->SetCombatLevel(TeamLevel); }
    Character->Experience=TeamExperience;
    if(Gained>0)
    {
        Character->PendingLevelChoices+=Gained;Character->RunGold+=10*Gained;
        if(Character->bDistrictCamera) { Character->ClientRecordDistrictProgress(TeamLevel,INDEX_NONE,0,FMath::FloorToInt(Character->MaxCombatHealth)); }
    }
    if(Character->CombatHealth>0.f && Character->LevelChoices.IsEmpty()
        && (Character->PendingLevelChoices>0||Character->PendingAugmentRewards>0||Character->PendingAccessCards>0)) { Character->RollLevelChoices(); }
    Character->ForceNetUpdate();
}

void AAnimaPreviewMode::AddTeamExperience(int32 Amount,FVector Origin,AAnimaPreviewCharacter* Collector,bool bCollectedLocally)
{
    if(!HasAuthority() || Amount<=0 || !IsValid(Collector) || Collector->bDistrictFinished || Collector->CombatHealth<=0.f) { return; }
    float BestMultiplier=0.f;
    for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
    {
        if(!It->GetController() || It->bDistrictFinished) { continue; }
        // Highest team bonus applies once, independently of who picks up the orb.
        const float Bonus=It->PermanentExperienceMultiplier*(1.f+(It->HasAugment(ESwarmAugment::Magnet)?.15f:0.f)
            +(It->HasAugment(ESwarmAugment::Experienced)?.15f:0.f))*(1.f+.10f*(It->RunWeaponStats.IsValidIndex(7)?It->RunWeaponStats[7]:0));
        if(FMath::IsFinite(Bonus)) { BestMultiplier=FMath::Max(BestMultiplier,Bonus); }
    }
    int64 Total=TeamExperience+FMath::RoundToInt64(FMath::Clamp(double(Amount)*BestMultiplier,0.0,1.e9));
    while(Total>=GetSwarmExperienceRequired(TeamLevel)) { Total-=GetSwarmExperienceRequired(TeamLevel);++TeamLevel; }
    TeamExperience=int32(Total);
    if(bCollectedLocally) { Collector->RegisterLootPickup(Origin,true); }
    for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
    {
        if(!It->GetController() || It->bDistrictFinished) { continue; }
        if(It->CombatHealth>0.f) { It->NotifyAugmentExperience(Amount); }
        SyncTeamProgress(*It);
    }
}

#if WITH_EDITOR
void AAnimaPreviewMode::SetTeamProgressForReview(int32 Level,int32 XP)
{ TeamLevel=FMath::Max(1,Level);TeamExperience=FMath::Clamp(XP,0,GetSwarmExperienceRequired(TeamLevel)-1); }
#endif

void AAnimaPreviewCharacter::CompleteDistrictRun(bool bVictory)
{
    if (bDistrictFinished) { return; }
    FlushCareerProgress();
    if(bVictory) { ClientRecordDifficultyClear(SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this)),GameDifficulty); }
    bDistrictFinished = true;
    bDistrictCleared = bVictory;
    LevelChoices.Reset();
    LevelChoiceDeadline = 0.f;
    PendingLevelChoices = 0;
    PendingAugmentRewards=0; bAugmentRewardOpen=false; YuumiQuestKills=-1; YuumiQuestRemaining=0.f;
    BoonSpireKills=-1; BoonSpireRemaining=0.f; WorldBoonEnd=0.f;
    PendingAccessCards = 0;
    bAccessRewardOpen = false;
    EndLevelChoiceSelection();
    ClientRecordDistrictProgress(CombatLevel, bVictory ? SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this)) : INDEX_NONE, RunGold, FMath::FloorToInt(MaxCombatHealth));
    ForceNetUpdate();
}

void AAnimaPreviewCharacter::ClientRecordDistrictProgress_Implementation(int32 Level, int32 ClearedArena, int32 EarnedGold, int32 MaximumHealth)
{
#if WITH_EDITOR
    int32 DiagnosticCharacter=0;
    if(FParse::Value(FCommandLine::Get(),TEXT("SwarmSkillVisual="),DiagnosticCharacter) || FParse::Param(FCommandLine::Get(),TEXT("SwarmGrowthNetworkReview")) || FParse::Param(FCommandLine::Get(), TEXT("SwarmBeachheadSmokeTest"))) { return; }
#endif
    UDistrictProgress::Record(Level, ClearedArena, EarnedGold, MaximumHealth);

}

void AAnimaPreviewCharacter::ClientPlayCombatCue_Implementation(uint8 Cue)
{
    static const TCHAR* Cues[] = { TEXT("Pickups/Experience"), TEXT("Pickups/Gold"), TEXT("Pickups/Heal"),
        TEXT("UI/LevelUp"), TEXT("UI/CardSelect"), TEXT("UI/Evolution"), TEXT("Pickups/AccessCard") };
    if (Cue >= UE_ARRAY_COUNT(Cues)) { return; }
    if (Cue == 0)
    {
        const float Now = GetWorld()->GetRealTimeSeconds();
        if (Now < NextExperienceSoundAt) { return; }
        NextExperienceSoundAt = Now + .09f;
    }
    CombatAudio::PlayLocal(GetWorld(), Cues[Cue], Cue < 3 ? .40f : .62f);
}

void AAnimaPreviewCharacter::CollectAccessCard()
{
    if (!HasAuthority() || bDistrictFinished || CombatHealth <= 0.f) { return; }
    ++PendingAccessCards;
    if(HasAugment(ESwarmAugment::CardCollector)) { ++AugmentCardStacks; }
    ClientPlayCombatCue(6);
    if (LevelChoices.IsEmpty()) { RollLevelChoices(); }
}

int32 AAnimaPreviewCharacter::GetRunStatRank(uint8 Choice) const
{
    if (Choice == 1) { return FMath::RoundToInt(RunHealthBonus / 150.f); }
    if (Choice == 2) { return FMath::RoundToInt(RunArmorBonus / 8.f); }
    if (Choice == 3) { return FMath::RoundToInt(RunMoveBonus / .08f); }
    if (Choice == 4) { return FMath::RoundToInt(RunRegen / 4.f); }
    const int32 Index = Choice == PickupRangeChoice ? 6 : Choice == ExperienceChoice ? 7 : Choice - 5;
    return RunWeaponStats.IsValidIndex(Index) ? RunWeaponStats[Index] : 0;
}

int32 AAnimaPreviewCharacter::GetRunStatCount() const
{
    int32 Count = 0;
    for (uint8 Choice : {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, static_cast<int>(PickupRangeChoice), static_cast<int>(ExperienceChoice)})
    {
        Count += GetRunStatRank(Choice) > 0 ? 1 : 0;
    }
    return Count;
}

bool AAnimaPreviewCharacter::CanEvolveSignatureWeapon() const
{
    static const uint8 Requirements[] = {6, 7, 2, 8, 1, 10, 3, ExperienceChoice, PickupRangeChoice};
    return SignatureWeaponLevel == 5 && !IsSignatureWeaponEvolved()
        && CharacterChoice < UE_ARRAY_COUNT(Requirements) && GetRunStatRank(Requirements[CharacterChoice]) > 0;
}

void AAnimaPreviewCharacter::RollLevelChoices()
{
    ON_SCOPE_EXIT { PublishLevelChoices(); };
    ++LevelChoiceSequence;
    LevelChoiceDeadline = GetLevelChoiceClock() + 5.f;
    bChoicePresentationConfirmed=IsLocallyControlled();NextChoiceResendAt=GetLevelChoiceClock()+.75f;
    BeginLevelChoiceSelection();
    bAugmentRewardOpen=PendingAugmentRewards>0;
    if(bAugmentRewardOpen)
    {
        bAccessRewardOpen=false; LevelChoices.Reset(); TArray<uint8> Pool;
        Pool=GetAvailableAugmentChoices();
        while(LevelChoices.Num()<3 && !Pool.IsEmpty()) { const int32 I=FMath::RandRange(0,Pool.Num()-1); LevelChoices.Add(Pool[I]); Pool.RemoveAtSwap(I); }
        return;
    }
    bAccessRewardOpen = PendingAccessCards > 0;
    if (bAccessRewardOpen)
    {
        TArray<uint8> Evolutions;
        if (CanEvolveSignatureWeapon()) { Evolutions.Add(SignatureEvolutionChoice); }
        for (uint8 Weapon : GeneralWeapons)
        {
            if (CanEvolveGeneralWeapon(Weapon)) { Evolutions.Add(FirstEvolutionChoice + Weapon); }
        }
        if (!Evolutions.IsEmpty())
        {
            LevelChoices = { Evolutions[FMath::RandRange(0, Evolutions.Num() - 1)] };
            return;
        }
    }
    else { ClientPlayCombatCue(3); }
    TArray<uint8> Pool = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, PickupRangeChoice, ExperienceChoice};
    const bool bStatSlotsFull = GetRunStatCount() >= 6;
    Pool.RemoveAll([this, bStatSlotsFull](uint8 Choice)
    {
        const int32 Rank = GetRunStatRank(Choice);
        return Rank >= 5 || (bStatSlotsFull && Rank == 0);
    });
    for (uint8 Weapon = 0; Weapon < GeneralWeaponCount; ++Weapon)
    {
        const int32 Rank = GetGeneralWeaponRank(Weapon);
        if (Rank < 5 && (Rank > 0 || GeneralWeapons.Num() < 4))
        {
            Pool.Add(FirstGeneralWeaponChoice + Weapon);
        }
    }
    if (SignatureWeaponLevel < 5) { Pool.Add(0); }
    LevelChoices.Reset();
    if (Pool.IsEmpty()) { Pool.Add(RunGoldRewardChoice); }
    const int32 ChoiceCount = FMath::Min(Pool.Num(), bAccessRewardOpen ? 1 : 3);
    while (LevelChoices.Num() < ChoiceCount)
    {
        const int32 Index = FMath::RandRange(0, Pool.Num() - 1);
        LevelChoices.Add(Pool[Index]);
        Pool.RemoveAtSwap(Index);
    }
}

void AAnimaPreviewCharacter::ServerSelectLevelChoice_Implementation(int32 Slot, int32 Sequence)
{
    if (Sequence != LevelChoiceSequence || !LevelChoices.IsValidIndex(Slot) || (bAugmentRewardOpen?PendingAugmentRewards:(bAccessRewardOpen ? PendingAccessCards : PendingLevelChoices)) <= 0 || CombatHealth <= 0.f) { return; }
    const auto CountEvolutions = [this]()
    {
        int32 Count = IsSignatureWeaponEvolved() ? 1 : 0;
        for (uint8 Weapon : GeneralWeapons) { Count += IsGeneralWeaponEvolved(Weapon) ? 1 : 0; }
        return Count;
    };
    const int32 PreviousEvolutions = CountEvolutions();
    const uint8 Choice = LevelChoices[Slot];
    if (((Choice >= 1 && Choice <= 10) || Choice == PickupRangeChoice || Choice == ExperienceChoice)
        && (GetRunStatRank(Choice) >= 5 || (GetRunStatRank(Choice) == 0 && GetRunStatCount() >= 6))) { return; }
    if (Choice>=FirstAugmentChoice)
    {
        const uint8 Id=Choice-FirstAugmentChoice;
        if(!bAugmentRewardOpen || Id>=static_cast<uint8>(ESwarmAugment::Count) || OwnedAugments.Contains(Id)) { return; }
        ApplySwarmAugment(Id);
    }
    else if (Choice == RunGoldRewardChoice)
    {
        RunGold += 25;
    }
    else if (Choice == SignatureEvolutionChoice)
    {
        if (!bAccessRewardOpen || !CanEvolveSignatureWeapon()) { return; }
        bSignatureWeaponEvolved = true;
    }
    else if (Choice >= FirstEvolutionChoice)
    {
        const uint8 Weapon = Choice - FirstEvolutionChoice;
        if (!bAccessRewardOpen || !CanEvolveGeneralWeapon(Weapon)) { return; }
        EvolvedGeneralWeapons.Add(Weapon);
    }
    else if (Choice == PickupRangeChoice || Choice == ExperienceChoice)
    {
        RunWeaponStats.SetNum(8);
        ++RunWeaponStats[Choice == PickupRangeChoice ? 6 : 7];
    }
    else if (Choice >= FirstGeneralWeaponChoice)
    {
        if (!UpgradeGeneralWeapon(Choice - FirstGeneralWeaponChoice)) { return; }
    }
    else if (Choice >= 5)
    {
        RunWeaponStats.SetNum(8);
        ++RunWeaponStats[Choice - 5];
    }
    switch (Choice)
    {
    case 0: UpgradeSignatureWeapon(); break;
    case 1: RunHealthBonus += 150.f; MaxCombatHealth += 150.f*GetAugmentHealthMultiplier(); CombatHealth += 150.f*GetAugmentHealthMultiplier(); ClientRecordDistrictProgress(CombatLevel, INDEX_NONE, 0, FMath::FloorToInt(MaxCombatHealth)); break;
    case 2: RunArmorBonus += 8.f; break;
    case 3: RunMoveBonus += .08f; break;
    case 4: RunRegen += 4.f; break;
    }
    LevelChoices.Reset();
    LevelChoiceDeadline = 0.f;
    ClientPlayCombatCue(CountEvolutions() > PreviousEvolutions ? 5 : 4);
    if(bAugmentRewardOpen) { --PendingAugmentRewards; }
    else if (bAccessRewardOpen) { --PendingAccessCards; }
    else { --PendingLevelChoices; }
    bAccessRewardOpen = false;
    bAugmentRewardOpen=false;
    if (PendingAugmentRewards>0 || PendingAccessCards > 0 || PendingLevelChoices > 0) { RollLevelChoices(); }
    else { EndLevelChoiceSelection(); }
}

namespace
{
constexpr float PoisonPoolRadius = 215.f;
constexpr float PoisonPoolDuration = 3.5f;
constexpr float PoisonPoolInterval = .5f;
}

ASwarmPoisonPool::ASwarmPoisonPool()
{
    bReplicates = true;
    Surface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PoisonSurface"));
    SetRootComponent(Surface);
    Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Surface->SetCastShadow(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    Surface->SetStaticMesh(Cylinder.Object);
}

void ASwarmPoisonPool::BeginPlay()
{
    Super::BeginPlay();
    Surface->SetRelativeScale3D(FVector(PoisonPoolRadius / 50.f, PoisonPoolRadius / 50.f, .045f));
    if (GetNetMode() != NM_DedicatedServer)
    {
        ColorBody(Surface, FLinearColor(.36f, .78f, .08f));
    }
    if (HasAuthority())
    {
        SetLifeSpan(PoisonPoolDuration);
        FTimerHandle DamageTimer;
        GetWorldTimerManager().SetTimer(DamageTimer, this, &ASwarmPoisonPool::DamagePlayers, PoisonPoolInterval, true);
    }
}

void ASwarmPoisonPool::DamagePlayers()
{
    for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
    {
        const FVector Offset = It->GetActorLocation() - GetActorLocation();
        if (It->GetController() && It->GetCombatHealth() > 0.f && FMath::Abs(Offset.Z) <= 145.f
            && Offset.SizeSquared2D() <= FMath::Square(PoisonPoolRadius))
        {
            UGameplayStatics::ApplyDamage(*It, 18.f, nullptr, this, UDamageType::StaticClass());
        }
    }
}

void ASwarmEnemy::ShowDamageNumber_Implementation(FVector_NetQuantize Position,float Amount)
{
    if(auto* View=UGameplayStatics::GetPlayerController(this,0))
    { if(auto* Hud=Cast<ACombatHUD>(View->GetHUD())) { Hud->AddDamageNumber(Position,Amount); }
      if(auto* Hero=Cast<AAnimaPreviewCharacter>(View->GetPawn())) { Hero->RenderEnemyBurst(Position,30.f); } }
}

#if WITH_EDITOR
void AAnimaPreviewMode::RunDensityAcceptanceTest()
{
    for(TActorIterator<ASwarmEnemy> It(GetWorld());It;++It){It->Destroy();}
    NextRegularSpawn=0.f;const int32 Before=EnemiesSpawned;SpawnWaveEnemy();const int32 Batch=EnemiesSpawned-Before;
    const bool Scheduled=NextRegularSpawn>GetWorld()->TimeSeconds-RunStartedAt;
    for(int I=0;I<100;++I){NextRegularSpawn=0.f;SpawnWaveEnemy();}
    int32 Alive=0;for(TActorIterator<ASwarmEnemy> It(GetWorld());It;++It){if(It->GetHealth()>0.f){++Alive;}}
    UE_LOG(LogTemp,Display,TEXT("Density acceptance: batch=%d scheduled=%d population=%d cap=%d filled=%d bounded=%d"),Batch,Scheduled,Alive,GetSwarmEncounterStage(0).PopulationCap,Alive>=75,Alive<=80);
}
#endif

bool ASwarmEnemy::IsBossEnraged() const
{ return Kind==3&&(bSecondPhase||Health<=GetMaxHealth()*.4f); }
void ASwarmEnemy::SpawnBossPressurePattern()
{
    if(!HasAuthority()||Kind!=3||Health<=0.f||LastPressureAttack==BossAttack) { return; }
    LastPressureAttack=BossAttack;
    const int32 Arena=GetArenaIndex();int32 Count=0;
    const auto SpawnMark=[&](FVector Position,float Delay,float Radius,float Duration)
    {
        Position.Z-=80.f;const FTransform T(Position);
        auto* Mark=GetWorld()->SpawnActorDeferred<ASwarmBossHazard>(ASwarmBossHazard::StaticClass(),T,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if(Mark) {Mark->ActivateAt=GetWorld()->GetTimeSeconds()+Delay;Mark->Radius=Radius;Mark->Damage=GetSpawnDamage()*(Arena==1?.65f:.85f);Mark->ActiveFor=Duration;Mark->Style=Arena;Mark->FinishSpawning(T);++Count;}
    };
    for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
    {
        if(!It->GetController()||It->GetCombatHealth()<=0.f||(IsLabChamberActive()&&It->GetActorLocation().X<1750.f)) { continue; }
        if(Arena==0)
        {
            const FVector D=(It->GetActorLocation()-GetActorLocation()).GetSafeNormal2D();
            for(int32 I=0;I<3;++I) { SpawnMark(It->GetActorLocation()+D*(I-1)*220.f,1.1f+.18f*I,135.f,0.f); }
        }
        else if(Arena==1) { SpawnMark(It->GetActorLocation(),1.2f,165.f,3.f); }
        else if(Arena==2)
        {
            SpawnMark(It->GetActorLocation(),1.15f,155.f,0.f);
            const FVector Side=(It->GetActorLocation()-GetActorLocation()).GetSafeNormal2D().RotateAngleAxis(90,FVector::UpVector)*250.f;
            SpawnMark(It->GetActorLocation()+Side,1.45f,145.f,0.f);SpawnMark(It->GetActorLocation()-Side,1.45f,145.f,0.f);
        }
        else { SpawnMark(It->GetActorLocation(),1.25f,IsBossEnraged()?230.f:190.f,0.f); }
    }
    UE_LOG(LogTemp,Display,TEXT("Boss pressure: arena=%d marks=%d enraged=%d"),Arena,Count,IsBossEnraged());
}
ASwarmBossHazard::ASwarmBossHazard()
{
    bReplicates=true;PrimaryActorTick.bCanEverTick=true;SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("HazardRoot")));SetNetUpdateFrequency(10.f);
}
void ASwarmBossHazard::BeginPlay()
{ Super::BeginPlay();if(HasAuthority()) { SetLifeSpan(FMath::Max(0.f,ActivateAt-GetWorld()->GetTimeSeconds())+ActiveFor+.65f); } }
void ASwarmBossHazard::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ASwarmBossHazard,ActivateAt);DOREPLIFETIME(ASwarmBossHazard,Radius);DOREPLIFETIME(ASwarmBossHazard,Damage);DOREPLIFETIME(ASwarmBossHazard,ActiveFor);DOREPLIFETIME(ASwarmBossHazard,Style);
}
void ASwarmBossHazard::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);const auto* State=GetWorld()->GetGameState();const float Now=State?State->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds();
    if(HasAuthority()) {const auto* Boss=Cast<ASwarmEnemy>(GetOwner());if(!Boss||Boss->GetHealth()<=0.f) { Destroy();return; }}
    const bool Active=Now>=ActivateAt;
    if(GetNetMode()!=NM_DedicatedServer)
    {
        const FColor Color=Active?FColor::Red:Style==1?FColor(255,100,140):FColor(255,185,50);
        DrawDebugCircle(GetWorld(),GetActorLocation(),Radius,64,Color,false,-1.f,0,5.f,FVector::ForwardVector,FVector::RightVector,false);
        DrawDebugCircle(GetWorld(),GetActorLocation()+FVector(0,0,2),Radius*FMath::Clamp(1.f-(ActivateAt-Now)/1.5f,0.f,1.f),48,Color,false,-1.f,0,3.f,FVector::ForwardVector,FVector::RightVector,false);
    }
    if(!Active) { return; }
    if(!bDetonated)
    {
        if(auto* PC=UGameplayStatics::GetPlayerController(this,0)) {if(auto* Hero=Cast<AAnimaPreviewCharacter>(PC->GetPawn())) { Hero->RenderEnemyBurst(GetActorLocation(),Radius); }}
    }
    if(HasAuthority()&&(!bDetonated||(ActiveFor>0.f&&Now<=ActivateAt+ActiveFor&&Now>=NextPulseAt)))
    {
        for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
        { if(It->GetController()&&It->GetCombatHealth()>0.f&&FVector::DistSquared2D(It->GetActorLocation(),GetActorLocation())<=FMath::Square(Radius)) { UGameplayStatics::ApplyDamage(*It,bDetonated?Damage*.2f:Damage,nullptr,GetOwner(),UDamageType::StaticClass()); } }
        NextPulseAt=Now+.5f;
    }
    bDetonated=true;
}

void ASwarmEnemy::TriggerAttackPose(float ImpactAt,float Windup)
{
    if(!HasAuthority()) {return;}AttackMotionStartAt=ImpactAt-FMath::Max(.02f,Windup);AttackMotionImpactAt=ImpactAt;ForceNetUpdate();
}
