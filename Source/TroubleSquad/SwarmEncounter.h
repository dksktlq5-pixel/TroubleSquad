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
    UFUNCTION() void OnRep_Kind();
    void UpdateBodyPose();
    void UpdateLabCoralVisuals();
    FVector GetLabCoralGroundPosition(int32 Index) const;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> LabCoralVisuals;
    float BodyScale = 0.f;
    float BodyMinZ = 0.f;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY() TObjectPtr<UStaticMesh> PrototypeBody;
    UFUNCTION(NetMulticast, Unreliable) void PlayEnemyCue(uint8 Cue, FVector_NetQuantize Location);
    float NextHitFeedbackAt = 0.f;
    double HitPoseUntil = 0.0;
    float FrozenUntil = 0.f;
    UPROPERTY(Replicated) float Health = 60.f;
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
    static void SpawnGold(UWorld* World, FVector Position);
    void BeginPlay() override;
    void Tick(float DeltaTime) override;
    void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UPROPERTY(ReplicatedUsing=OnRep_Kind) uint8 Kind = 0;

private:
    UFUNCTION() void OnRep_Kind();
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
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
