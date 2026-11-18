#include "SwarmEncounter.h"
#include "SwarmBolt.h"
#include "SwarmArenaGate.h"
#include "SwarmArenas.h"
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
    {460.f, 150.f, 48.f, 28, FLinearColor(.83f,.31f,1.f)},
    {5400.f, 285.f, 78.f, 0, FLinearColor(.42f,.07f,.59f)},
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
    }
    Super::EndPlay(Reason);
}

void ASwarmEnemy::PlayEnemyCue_Implementation(uint8 Cue, FVector_NetQuantize Location)
{
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
    if (Cue < UE_ARRAY_COUNT(Cues)) { CombatAudio::PlayWorld(GetWorld(), Cues[Cue], Location, Cue < 2 ? .24f : .61f); }
}

void ASwarmEnemy::OnRep_Kind()
{
    GetCharacterMovement()->MaxWalkSpeed = Stats[Kind].Speed;
    BodyScale = 0.f;
    Body->SetRelativeRotation(FRotator::ZeroRotator);
    static constexpr const TCHAR* BossMeshes[] = { TEXT("Reksai"), TEXT("Briar"), TEXT("Belveth"), TEXT("Aatrox") };
    const TCHAR* Name = Kind == 3 && !bBurrowed ? BossMeshes[GetArenaIndex()] : EnemyMeshName(Kind, ModelVariant);
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
            const FBox Bounds = EnemyMesh->GetBoundingBox();
            const float Scale = (Kind == 3 ? 390.f : Kind == 2 ? 177.f : Kind == 1 ? 108.f : 76.f) /
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
    DOREPLIFETIME(ASwarmEnemy, bBurrowed);
}

int32 ASwarmEnemy::GetArenaIndex() const
{
    return FMath::Max(0, SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this)));
}

float ASwarmEnemy::GetMaxHealth() const
{
    if (Kind == 2 && ModelVariant >= UE_ARRAY_COUNT(SpecialEnemyMeshes)) { return Stats[1].Health; }
    return Kind == 3 ? Stats[Kind].Health * (1.f + .22f * GetArenaIndex()) : Stats[Kind].Health;
}

float ASwarmEnemy::GetMissingHealthFraction_Implementation() const
{
    return FMath::Clamp(1.f - Health / GetMaxHealth(), 0.f, 1.f);
}

float ASwarmEnemy::TakeDamage(float Damage, const FDamageEvent& Event, AController* InstigatorController, AActor* Causer)
{
    if (!HasAuthority() || Health <= 0.f || Damage <= 0.f || bBurrowed) { return 0.f; }
    if (Kind == 3 && IsLabChamberActive() && Causer && Causer->GetActorLocation().X < 1750.f) { return 0.f; }
    if (bBeachheadTetherActive && BeachheadPillar.IsValid()) { return 0.f; }
    const auto* Attacker = Cast<AAnimaPreviewCharacter>(Causer);
    if (LabCoralsRemaining > 0) { return 0.f; }
    const bool bLabTransition = Kind == 3 && GetArenaIndex() == 2 && !bSecondPhase;
    const float DamageLimit = bLabTransition ? FMath::Max(0.f, Health - GetMaxHealth() * .5f) : Health;
    const float Applied = FMath::Min(DamageLimit, Damage * (Attacker ? Attacker->GetCombatDamageMultiplier() : 1.f));
    Health -= Applied;
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
        if (Kind < 3) { PlayEnemyCue(1, GetActorLocation()); }
        auto* Killer = Cast<AAnimaPreviewCharacter>(Causer);
        if (!Killer && InstigatorController) { Killer = Cast<AAnimaPreviewCharacter>(InstigatorController->GetPawn()); }
        if (Killer && Kind != 4) { Killer->RegisterEnemyKill(); }
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
        if (Stats[Kind].Experience > 0)
        {
            const FTransform Transform(GetActorLocation() - FVector(0, 0, 38));
            auto* Orb = GetWorld()->SpawnActorDeferred<ASwarmExperienceOrb>(ASwarmExperienceOrb::StaticClass(), Transform,
                nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            if (Orb)
            {
                Orb->Kind = Kind == 2 && ModelVariant >= UE_ARRAY_COUNT(SpecialEnemyMeshes) ? 0 : Kind;
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
        const float Applied = UGameplayStatics::ApplyDamage(*Player, Stats[Kind].Damage * (bSecondPhase ? 1.35f : 1.f),
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
    if (UsesDashAttack()) { return Kind == 2 ? 95.f : 145.f; }
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
    bDash = UsesDashAttack() || UsesAatroxLineSlam() || (bBurrowed && BossAttack % 2 != 0) || (Kind == 2 && ModelVariant % UE_ARRAY_COUNT(SpecialEnemyMeshes) == 0);
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
            UGameplayStatics::ApplyDamage(*It, Stats[Kind].Damage * (bSecondPhase ? 1.35f : 1.f), nullptr, this, UDamageType::StaticClass());
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
    FActorSpawnParameters Params;
    Params.Owner = this;
    const FVector Muzzle = GetActorLocation() + Direction * 55.f - FVector(0, 0, 22.f);
    if (auto* Bolt = GetWorld()->SpawnActor<ASwarmBolt>(Muzzle, Direction.Rotation(), Params))
    {
        Bolt->Damage = Damage;
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
    TelegraphUntil = 0.f;
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

void ASwarmEnemy::TickSeahorseCharge(AAnimaPreviewCharacter* NearestTarget, float Now, float DeltaTime)
{
    if (Now < NextAttack) { return; }
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    const FVector Position = GetActorLocation();
    AAnimaPreviewCharacter* Target = ChargeTarget.Get();
    if (!Target || !Target->GetController() || Target->GetCombatHealth() <= 0.f)
    {
        Target = NearestTarget;
        ChargeTarget = Target;
        PreviousChargePosition = Position;
        ChargeSpeed = Stats[Kind].Speed;
        PlayEnemyCue(2, Position);
    }

    AAnimaPreviewCharacter* HitPlayer = nullptr;
    double FirstContact = TNumericLimits<double>::Max();
    constexpr float ContactRadius = 105.f;
    for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
    {
        const FVector Offset = It->GetActorLocation() - Position;
        if (!It->GetController() || It->GetCombatHealth() <= 0.f || FMath::Abs(Offset.Z) > 110.f) { continue; }
        const FVector Point(Position.X + Offset.X, Position.Y + Offset.Y, Position.Z);
        const FVector Start(PreviousChargePosition.X, PreviousChargePosition.Y, Position.Z);
        const FVector Contact = FMath::ClosestPointOnSegment(Point, Start, Position);
        const double AlongPath = FVector::DistSquared(Start, Contact);
        if (FVector::DistSquared(Point, Contact) <= FMath::Square(ContactRadius) && AlongPath < FirstContact)
        {
            HitPlayer = *It;
            FirstContact = AlongPath;
        }
    }
    if (HitPlayer)
    {
        const float Applied = UGameplayStatics::ApplyDamage(HitPlayer, Stats[Kind].Damage * 1.65f,
            nullptr, this, UDamageType::StaticClass());
        if (Applied > 0.f && HitPlayer->GetCombatHealth() > 0.f)
        {
            HitPlayer->LaunchCharacter(GetActorForwardVector() * 720.f + FVector::UpVector * 315.f, true, true);
        }
        PlayEnemyCue(3, HitPlayer->GetActorLocation());
        Movement->StopMovementImmediately();
        ChargeTarget.Reset();
        NextAttack = Now + 1.45f;
        return;
    }

    PreviousChargePosition = Position;
    ChargeSpeed = FMath::Min(825.f, ChargeSpeed + 330.f * DeltaTime);
    if (!Movement->ComponentHasTag(TEXT("SeraphineWaveSlow"))) { Movement->MaxWalkSpeed = ChargeSpeed; }
    const FVector Direction = (Target->GetActorLocation() - Position).GetSafeNormal2D();
    SetActorRotation(Direction.Rotation());
    AddMovementInput(Direction, 1.f, true);
}

FVector ASwarmEnemy::GetLabCoralGroundPosition(int32 Index) const
{
    return GetActorLocation() + FRotator(0.f, Index * 90.f, 0.f).Vector() * 390.f - FVector::UpVector * 55.f;
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
    const double Phase = Now * (Kind == 0 ? 17.0 : Kind == 3 ? 8.0 : 12.0) + GetActorLocation().Y * .012;
    const double Sway = FMath::Sin(Phase);
    const double Bounce = FMath::Abs(Sway) * Stride * (Kind == 3 ? 9.0 : 7.0);
    const double Compression = bWindingUp ? .13 : -.025 * Sway * Stride;
    const double Recoil = FMath::Clamp((HitPoseUntil - GetWorld()->GetTimeSeconds()) / .16, 0.0, 1.0);
    const double HeightScale = BodyScale * (1.0 - Compression);
    Body->SetRelativeScale3D(FVector(BodyScale * (1.0 + Compression * .45), BodyScale, HeightScale));
    Body->SetRelativeRotation(FRotator((bWindingUp ? -13.0 : 4.0 * Stride) - 12.0 * Recoil, 0.0, Sway * Stride * 7.0));
    FVector BodyPosition(0.0, 0.0,
        -GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - BodyMinZ * HeightScale + Bounce);
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
            ResolveAreaAttack(GetAttackRadius(), Stats[Kind].Damage);
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

    if (TelegraphUntil > 0.f)
    {
        const float Radius = GetAttackRadius();
        if (Now < TelegraphUntil) { return; }
        TelegraphUntil = 0.f;
        PlayEnemyCue(bAatroxPillarDive ? 7 : Kind == 3 ? 5 : 3, TelegraphCenter);
        if (bAatroxPillarDive)
        {
            TeleportTo(BeachheadPillarPosition, GetActorRotation());
            ResolveAreaAttack(Radius, Stats[Kind].Damage * 1.35f);
            bAatroxPillarDive = false;
            ForceNetUpdate();
        }
        else if (bBurrowed && BossAttack % 2 != 0)
        {
            FireBolt((TelegraphCenter - GetActorLocation()).GetSafeNormal2D(), Stats[Kind].Damage);
        }
        else if (UsesDashAttack()) { ResolveDashAttack(Radius); }
        else if (UsesAatroxLineSlam()) { ResolveAatroxLineSlam(Radius); }
        else if (Kind == 2 && ModelVariant % UE_ARRAY_COUNT(SpecialEnemyMeshes) == 0)
        {
            const FVector Direction = (TelegraphCenter - GetActorLocation()).GetSafeNormal2D();
            for (int32 Shot = 0; Shot < 3; ++Shot)
            {
                const auto Fire = [this, Direction]() { FireBolt(Direction, Stats[Kind].Damage * .65f); };
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
                FireBolt(Direction, Stats[Kind].Damage * .60f);
            }
        }
        else { ResolveAreaAttack(Radius, Stats[Kind].Damage * (bSecondPhase ? 1.35f : 1.f)); }
        if (AatroxSlamsRemaining > 0 && --AatroxSlamsRemaining > 0)
        {
            const FVector Direction = (TelegraphCenter - AttackStart).GetSafeNormal2D();
            TelegraphCenter += Direction * 115.f;
            TelegraphUntil = Now + (bSecondPhase ? .43f : .68f);
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
    const FVector Direction = (PursuitPoint - GetActorLocation()).GetSafeNormal2D();
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
        UGameplayStatics::ApplyDamage(Target, Stats[Kind].Damage, nullptr, this, UDamageType::StaticClass());
    }
    const bool bRangedSpecial = SpecialVariant == 0 || SpecialVariant == 2 || SpecialVariant == 3 || SpecialVariant == 5;
    if (Kind >= 2 && Now >= NextAttack && Best <= FMath::Square(Kind == 3 ? (GetArenaIndex() == 2 ? 1050.f : 690.f) : bRangedSpecial ? 780.f : SpecialVariant == 4 ? 290.f : 220.f))
    {
        NextAttack = Now + (bBriarFrenzy ? 1.85f : Kind == 3 ? 2.8f : 3.2f);
        ++BossAttack;
        if (Kind == 3 && GetArenaIndex() == 3 && BossAttack % 2 != 0)
        {
            AatroxSlamsRemaining = 3;
        }
        AttackStart = GetActorLocation();
        TelegraphUntil = Now + (SpecialVariant == 5 ? 1.25f : SpecialVariant == 3 ? 1.05f : bSecondPhase ? .60f : .85f);
        if (Kind != 3) { PlayEnemyCue(2, GetActorLocation()); }
        TelegraphCenter = (Kind == 3 && !(GetArenaIndex() == 2 && !UsesDashAttack())) || (bRangedSpecial && SpecialVariant != 2) ? PursuitPoint : GetActorLocation();
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
        UGameplayStatics::ApplyDamage(Target, Stats[Kind].Damage, nullptr, this, UDamageType::StaticClass());
    }
}

ASwarmExperienceOrb::ASwarmExperienceOrb()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = true;
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExperienceSphere"));
    SetRootComponent(Body);
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
    const FTransform Transform(FRotator::ZeroRotator, Position - FVector(0, 0, 38));
    auto* Orb = World->SpawnActorDeferred<ASwarmExperienceOrb>(StaticClass(), Transform);
    if (Orb)
    {
        Orb->Kind = PickupKind;
        Orb->FinishSpawning(Transform);
    }
}

void ASwarmExperienceOrb::OnRep_Kind()
{
    SetActorScale3D(FVector(Kind >= 5 ? .19 : .13 + Kind * .055));
    ColorBody(Body, Kind == 7 ? FLinearColor(.95f, .20f, .95f) : Kind == 6 ? FLinearColor(.18f, 1.f, .35f) : Kind == 5 ? FLinearColor(1.f, .67f, .08f) : Stats[Kind].Color);
}

void ASwarmExperienceOrb::BeginPlay()
{
    Super::BeginPlay();
    OnRep_Kind();
    SetLifeSpan(65.f);
}

void ASwarmExperienceOrb::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASwarmExperienceOrb, Kind);
}

void ASwarmExperienceOrb::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!HasAuthority()) { return; }
    AAnimaPreviewCharacter* Collector = nullptr;
    double Nearest = TNumericLimits<double>::Max();
    for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
    {
        const double Distance = FVector::DistSquared(GetActorLocation(), It->GetActorLocation());
        if (It->GetController() && It->GetCombatHealth() > 0.f &&
            Distance < FMath::Square(145.f * It->GetLootPickupRadiusMultiplier()) && Distance < Nearest)
        {
            Nearest = Distance;
            Collector = *It;
        }
    }
    if (Collector)
    {
        const FVector Destination = Collector->GetActorLocation() - FVector(0, 0, 38.f);
        const FVector NextPosition = FMath::VInterpConstantTo(GetActorLocation(), Destination, DeltaTime, 1150.f);
        SetActorLocation(NextPosition);
        if (FVector::DistSquared(NextPosition, Destination) <= FMath::Square(24.f))
        {
            CollectBy(Collector);
        }
    }
}

void ASwarmExperienceOrb::CollectBy(AAnimaPreviewCharacter* Collector)
{
    if (!HasAuthority() || IsActorBeingDestroyed() || !IsValid(Collector)) { return; }
    if (Kind == 7)
    {
        for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
        {
            if (It->GetController() && It->GetCombatHealth() > 0.f) { It->CollectAccessCard(); }
        }
        Destroy();
        return;
    }
    Collector->ClientPlayCombatCue(Kind == 5 ? 1 : Kind == 6 ? 2 : 0);
    if (Kind == 5) { Collector->CollectRunGold(3, GetActorLocation()); }
    else if (Kind == 6) { Collector->CollectWeaponHealth(); }
    else
    {
        for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
        {
            if (It->GetController())
            {
                It->CollectExperience(Stats[Kind].Experience, GetActorLocation(), *It == Collector);
            }
        }
    }
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
        if (FParse::Param(FCommandLine::Get(), TEXT("SwarmBossSmokeTest"))) { RunStartedAt -= 898.f; }
#endif
        GetWorldTimerManager().SetTimer(EnemyWaveTimer, this, &AAnimaPreviewMode::SpawnWaveEnemy, .25f, true, 2.f);
    }
}

void AAnimaPreviewMode::SetLevelChoicePending(AAnimaPreviewCharacter* Character, bool bPending)
{
    if (bPending) { PendingChoicePlayers.Add(Character); }
    else { PendingChoicePlayers.Remove(Character); }

    if (!PendingChoicePlayers.IsEmpty() && !TimeDilationBeforeLevelChoice.IsSet())
    {
        TimeDilationBeforeLevelChoice = UGameplayStatics::GetGlobalTimeDilation(this);
        UGameplayStatics::SetGlobalTimeDilation(this, .0001f);
        GetWorldSettings()->ForceNetUpdate();
    }
    else if (PendingChoicePlayers.IsEmpty() && TimeDilationBeforeLevelChoice.IsSet())
    {
        UGameplayStatics::SetGlobalTimeDilation(this, TimeDilationBeforeLevelChoice.GetValue());
        GetWorldSettings()->ForceNetUpdate();
        TimeDilationBeforeLevelChoice.Reset();
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
    if (SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this)) == 0) { TickHealingStation(Now); }
    else { TickArenaGimmick(Now); }
    static constexpr float Events[] = {240.f, 390.f, 600.f, 750.f, 900.f};
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
    if (!bTimedEvent && Alive >= 35) { return; }
    for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
    {
        if (!It->GetController() || It->GetCombatHealth() <= 0.f) { continue; }
        const FVector Origin = It->GetActorLocation();
        const FVector Candidate = Origin + FRotator(0, FMath::FRandRange(-180.f, 180.f), 0).Vector() * 525.f;
        FHitResult Ground;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(SwarmSpawn), false, *It);
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
                : (Now >= 45.f && EnemiesSpawned % (Now >= 360.f ? 3 : 5) == 0 ? 1 : 0);
            const int32 ArenaSeed = FMath::Max(0, SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this)));
            Enemy->ModelVariant = static_cast<uint8>(Enemy->Kind == 2 ? NextEncounterEvent + ArenaSeed * 2
                : Enemy->Kind == 1 ? EnemiesSpawned / 3 + ArenaSeed : EnemiesSpawned - 1 + ArenaSeed);
            if (bTimedEvent && Enemy->Kind == 2)
            {
                Enemy->ModelVariant %= UE_ARRAY_COUNT(SpecialEnemyMeshes);
            }
            if (!bTimedEvent && Now >= 60.f && EnemiesSpawned % 16 < 3)
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
            NextRegularSpawn = Now + (Now < 120.f ? 1.35f : Now < 360.f ? 1.f : .75f);
        }
        break;
    }
}

void AAnimaPreviewMode::TickHealingStation(float Now)
{
    if (Now < HealingReadyAt)
    {
        return;
    }

    const FVector Center(-8, -200, 70);
    TArray<AAnimaPreviewCharacter*> Living;
    int32 Charging = 0;
    for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
    {
        if (!It->GetController() || It->GetCombatHealth() <= 0.f) { continue; }
        Living.Add(*It);
        Charging += FVector::DistSquared2D(It->GetActorLocation(), Center) <= FMath::Square(900.f) ? 1 : 0;
    }
    HealingCharge = Charging > 0 ? HealingCharge + .25f * Charging : 0.f;
    if (GetNetMode() != NM_DedicatedServer)
    {
        DrawDebugCircle(GetWorld(), Center, 900.f, 48, Charging ? FColor::Green : FColor::Cyan,
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
{
    return 48 + 12 * (CombatLevel - 1);
}

void AAnimaPreviewCharacter::CollectExperience(int32 Amount, FVector Origin, bool bCollectedLocally)
{
    if (!HasAuthority() || bDistrictFinished || Amount <= 0 || CombatHealth <= 0.f) { return; }
    if (bCollectedLocally) { RegisterLootPickup(Origin, true); }
    Experience += FMath::RoundToInt(Amount * PermanentExperienceMultiplier * (1.f + .10f * (RunWeaponStats.IsValidIndex(7) ? RunWeaponStats[7] : 0)));
    const int32 PreviousLevel = CombatLevel;
    while (Experience >= GetExperienceRequired())
    {
        Experience -= GetExperienceRequired();
        SetCombatLevel(CombatLevel + 1);
        RunGold += 10;
        ++PendingLevelChoices;
    }
    if (bDistrictCamera && CombatLevel > PreviousLevel) { ClientRecordDistrictProgress(CombatLevel, INDEX_NONE, 0, FMath::FloorToInt(MaxCombatHealth)); }
    if (PendingLevelChoices > 0 && LevelChoices.IsEmpty()) { RollLevelChoices(); }
}

void AAnimaPreviewCharacter::CompleteDistrictRun(bool bVictory)
{
    if (bDistrictFinished) { return; }
    bDistrictFinished = true;
    bDistrictCleared = bVictory;
    EndLevelChoiceFreeze();
    LevelChoices.Reset();
    LevelChoiceDeadline = 0.f;
    PendingLevelChoices = 0;
    PendingAccessCards = 0;
    bAccessRewardOpen = false;
    ClientRecordDistrictProgress(CombatLevel, bVictory ? SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this)) : INDEX_NONE, RunGold, FMath::FloorToInt(MaxCombatHealth));
    ForceNetUpdate();
}

void AAnimaPreviewCharacter::ClientRecordDistrictProgress_Implementation(int32 Level, int32 ClearedArena, int32 EarnedGold, int32 MaximumHealth)
{
#if WITH_EDITOR
    if (FParse::Param(FCommandLine::Get(), TEXT("SwarmBeachheadSmokeTest"))) { return; }
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
    ++LevelChoiceSequence;
    LevelChoiceDeadline = GetLevelChoiceClock() + 30.f;
    BeginLevelChoiceFreeze();
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
    if (Sequence != LevelChoiceSequence || !LevelChoices.IsValidIndex(Slot) || (bAccessRewardOpen ? PendingAccessCards : PendingLevelChoices) <= 0 || CombatHealth <= 0.f) { return; }
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
    if (Choice == RunGoldRewardChoice)
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
    case 1: RunHealthBonus += 150.f; MaxCombatHealth += 150.f; CombatHealth += 150.f; ClientRecordDistrictProgress(CombatLevel, INDEX_NONE, 0, FMath::FloorToInt(MaxCombatHealth)); break;
    case 2: RunArmorBonus += 8.f; break;
    case 3: RunMoveBonus += .08f; break;
    case 4: RunRegen += 4.f; break;
    }
    LevelChoices.Reset();
    LevelChoiceDeadline = 0.f;
    ClientPlayCombatCue(CountEvolutions() > PreviousEvolutions ? 5 : 4);
    if (bAccessRewardOpen) { --PendingAccessCards; }
    else { --PendingLevelChoices; }
    bAccessRewardOpen = false;
    if (PendingAccessCards > 0 || PendingLevelChoices > 0) { RollLevelChoices(); }
    else { EndLevelChoiceFreeze(); }
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
