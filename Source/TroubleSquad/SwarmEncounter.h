#pragma once

#include "GameFramework/Character.h"
#include "CombatHealth.h"
#include "Engine/NetSerialization.h"
#include "SwarmEncounter.generated.h"

class AAnimaPreviewCharacter;

UCLASS()
class ASwarmEnemy : public ACharacter, public ICombatHealth
{
    GENERATED_BODY()

public:
    ASwarmEnemy();
    void BeginPlay() override;
    void Tick(float DeltaTime) override;
    void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    float TakeDamage(float Damage, const FDamageEvent& Event, AController* InstigatorController, AActor* Causer) override;
    float GetMissingHealthFraction_Implementation() const override;
    void Freeze(float Duration);
    float GetHealth() const { return Health; }
    float GetMaxHealth() const;
    int32 GetArenaIndex() const;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void StartLabChamber();
    void StartOutskirtsField();
    bool GetOutskirtsField(FVector& Center, float& Radius) const;
    bool IsLabChamberActive() const { return LabChamberStartedAt > 0.f; }
    bool GetArenaHazardBoundary(double ServerTime, float& Boundary, bool& bDangerEast) const;
    bool IsBeachheadTetherActive() const { return bBeachheadTetherActive; }
    float GetArtilleryFlightProgress(double ServerTime) const;
    bool GetAttackWarning(double ServerTime, FVector& Start, FVector& End, float& Radius, bool& bDash) const;
    uint8 GetLabCoralsRemaining() const { return LabCoralsRemaining; }
    bool IsSecondPhase() const { return bSecondPhase; }
    bool IsBurrowed() const { return bBurrowed; }
    UPROPERTY(ReplicatedUsing=OnRep_Kind) uint8 Kind = 0;
    UPROPERTY(ReplicatedUsing=OnRep_Kind) uint8 ModelVariant = 0;

private:
    friend class AAnimaPreviewMode;
    friend class AAnimaPreviewCharacter;
    UFUNCTION() void OnRep_Kind();
    void UpdateBodyPose();
    void UpdateLabCoralVisuals();
    FVector GetLabCoralGroundPosition(int32 Index) const;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> LabCoralVisuals;
    float BodyScale = 0.f;
    bool bAnimatedLocomotion = false;
    UPROPERTY() TObjectPtr<UAnimSequence> WalkSequence;
    UPROPERTY() TObjectPtr<UAnimSequence> AttackSequence;
    UPROPERTY(Replicated) float AttackMotionStartAt=0.f;
    UPROPERTY(Replicated) float AttackMotionImpactAt=0.f;
    void TriggerAttackPose(float ImpactAt,float Windup);
    float BodyMinZ = 0.f;
    FRotator BodyUprightRotation = FRotator(0.f,-90.f,90.f);
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY() TObjectPtr<UStaticMesh> PrototypeBody;
    UFUNCTION(NetMulticast, Unreliable) void PlayEnemyCue(uint8 Cue, FVector_NetQuantize Location);
    float NextHitFeedbackAt = 0.f;
    float PendingDamageNumber = 0.f;
    float NextDamageNumberAt = 0.f;
    UFUNCTION(NetMulticast, Unreliable) void ShowDamageNumber(FVector_NetQuantize Position,float Amount);
    double HitPoseUntil = 0.0;
    float FrozenUntil = 0.f;
    UPROPERTY(Replicated) float Health = 60.f;
    UPROPERTY(Replicated) float SpawnMaximumHealth = 0.f;
    float SpawnDamageMultiplier = 1.f;
    float GetSpawnDamage() const;
    UPROPERTY(ReplicatedUsing=OnRep_Kind) bool bBurrowed = false;
    TWeakObjectPtr<ASwarmEnemy> Tunnel;
    float ExposedUntil = 0.f;
    float NextBurrowContactAt = 0.f;
    UPROPERTY(Replicated) float TelegraphUntil = 0.f;
    UPROPERTY(Replicated) FVector TelegraphCenter = FVector::ZeroVector;
    void EnterBurrow();
    void ResolveAreaAttack(float Radius, float Damage);
    void FireBolt(FVector Direction, float Damage);
    float NextAttack = 0.f;
    float ChargeSpeed = 0.f;
    FVector PreviousChargePosition = FVector::ZeroVector;
    TWeakObjectPtr<AAnimaPreviewCharacter> ChargeTarget;
    void TickSeahorseCharge(AAnimaPreviewCharacter* NearestTarget, float Now, float DeltaTime);
    UPROPERTY(Replicated) FVector AttackStart = FVector::ZeroVector;
    UPROPERTY(Replicated) int32 BossAttack = 0;
    UPROPERTY(Replicated) uint8 AatroxSlamsRemaining = 0;
    UPROPERTY(Replicated) bool bBeachheadTetherActive = false;
    TWeakObjectPtr<ASwarmEnemy> BeachheadPillar;
    FVector BeachheadPillarPosition = FVector::ZeroVector;
    UPROPERTY(Replicated) bool bAatroxPillarDive = false;
    TWeakObjectPtr<AActor> BeachheadGate;
    UPROPERTY(Replicated) bool bSecondPhase = false;
    UPROPERTY(Replicated) bool bOutskirtsFieldActive = false;
    UPROPERTY(Replicated) float LabChamberStartedAt = 0.f;
    UPROPERTY(Replicated) float PhaseStartedAt = 0.f;
    float NextArenaHazardAt = 0.f;
    UPROPERTY(Replicated) uint8 LabCoralsRemaining = 0;
    bool UsesDashAttack() const;
    bool UsesAatroxLineSlam() const;
    void ResolveAatroxLineSlam(float HalfWidth);
    float GetAttackRadius() const;
    void ResolveDashAttack(float Radius);
    void SpawnBossPressurePattern();
    int32 LastPressureAttack=INDEX_NONE;
    bool IsBossEnraged() const;
    UPROPERTY(Replicated) float SeahorseChargeEnd=0.f;
    FVector SeahorseChargeDirection=FVector::ZeroVector;
    int32 GetExperienceReward() const;
    void SpawnOffspring(int32 Count);
    int32 OffspringWaves=0;
    float NextOffspringAt=0.f;
};

UCLASS()
class ASwarmExperienceOrb : public AActor
{
    GENERATED_BODY()

public:
    ASwarmExperienceOrb();
    static void SpawnPickup(UWorld* World, FVector Position, uint8 PickupKind);
    void CollectBy(AAnimaPreviewCharacter* Collector);
    bool CanBeCollectedByDrone() const { return Kind <= 5; }
    bool CanBeVacuumed() const { return Kind <= 5; }
    void AttractTo(AAnimaPreviewCharacter* Collector, bool bVacuum = false);
    static void SpawnGold(UWorld* World, FVector Position);
    void BeginPlay() override;
    void Tick(float DeltaTime) override;
    void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UPROPERTY(ReplicatedUsing=OnRep_Kind) uint8 Kind = 0;
    UPROPERTY(ReplicatedUsing=OnRep_Kind) int32 ExperienceAmount=0;

private:
    UFUNCTION() void OnRep_Kind();
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
    TWeakObjectPtr<AAnimaPreviewCharacter> MagnetCollector;
    bool bVacuumAttraction = false;
    bool bCollectionCommitted = false;
};

UCLASS()
class ASwarmPoisonPool : public AActor
{
    GENERATED_BODY()

public:
    ASwarmPoisonPool();
    void BeginPlay() override;

private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Surface;
    void DamagePlayers();
};

UCLASS()
class ASwarmBossHazard : public AActor
{
    GENERATED_BODY()
public:
    ASwarmBossHazard();
    void BeginPlay() override;
    void Tick(float DeltaTime) override;
    void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(Replicated) float ActivateAt=0.f;
    UPROPERTY(Replicated) float Radius=150.f;
    UPROPERTY(Replicated) float Damage=100.f;
    UPROPERTY(Replicated) float ActiveFor=0.f;
    UPROPERTY(Replicated) uint8 Style=0;
private:
    bool bDetonated=false;
    float NextPulseAt=0.f;
};
