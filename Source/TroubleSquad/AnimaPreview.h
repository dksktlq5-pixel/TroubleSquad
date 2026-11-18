#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GeneralWeapons.h"
#include "AnimaPreview.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UAnimSequence;
class UStaticMeshComponent;
class UStaticMesh;
class ACombatVfx;

USTRUCT()
struct FCombatVfxEvent
{
    GENERATED_BODY()
    UPROPERTY() uint8 Kind=0;
    UPROPERTY() uint8 Weapon=0;
    UPROPERTY() uint8 Variant=0;
    UPROPERTY() uint8 ReviewToken=0;
    UPROPERTY() bool bEnhanced=false;
    UPROPERTY() FVector_NetQuantize Start=FVector::ZeroVector;
    UPROPERTY() FVector_NetQuantize End=FVector::ZeroVector;
    UPROPERTY() FVector_NetQuantizeNormal Direction=FVector::ForwardVector;
    UPROPERTY() float Radius=0.f;
    float CreatedAt=0.f;
};

USTRUCT()
struct FGeneralSummonVisual
{
    GENERATED_BODY()
    UPROPERTY() uint8 Weapon=0;
    UPROPERTY() FVector_NetQuantize Position=FVector::ZeroVector;
    UPROPERTY() FVector_NetQuantizeNormal Facing=FVector::ForwardVector;
    UPROPERTY() float Size=1.f;
    UPROPERTY() float EndAt=0.f;
};

USTRUCT()
struct FLandedXayahFeather
{
    GENERATED_BODY()

    UPROPERTY() FVector_NetQuantize Position;
    UPROPERTY() FVector_NetQuantizeNormal Direction;
};

USTRUCT()
struct FIllaoTentacleVisual
{
    GENERATED_BODY()

    UPROPERTY() FVector_NetQuantize Position = FVector::ZeroVector;
    UPROPERTY() float AttackRadius = 0.f;
    UPROPERTY() float NextAttackAt = 0.f;
    UPROPERTY() bool bGiant = false;
};

UCLASS()
class TROUBLESQUAD_API AAnimaPreviewCharacter : public ACharacter
{
    GENERATED_BODY()

public:
#if WITH_EDITOR
    void RunSkillVisualSmokeTest(int32 Index);
    void ReviewNiagaraAssets();
#endif
    AAnimaPreviewCharacter();
    int32 GetSelectedCharacter() const { return CharacterChoice; }
    virtual void BeginPlay() override;
    void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void PawnClientRestart() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
    void RegisterEnemyKill();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
    bool UpgradeSignatureWeapon();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
    void SetCombatLevel(int32 NewLevel);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
    void RegisterLootPickup(FVector LootOrigin, bool bExperienceOrGold);
    UFUNCTION(BlueprintPure, Category="Combat")
    float GetLootPickupRadiusMultiplier() const;
    int32 GetXayahLootRemaining() const { return 200 - XayahLootCount % 200; }
    float GetXayahBuffRemaining() const;
    int32 GetEnemyKills() const { return EnemyKills; }
    float GetJinxPassiveRemaining() const;
    int32 GetJinxKillsUntilPassive() const;
    float GetJinxWeaponRemaining() const;
    float GetCombatHealth() const { return CombatHealth; }
    float GetMaxCombatHealth() const { return MaxCombatHealth; }
    float GetCombatArmor() const;
    int32 GetCombatLevel() const { return CombatLevel; }
    const TArray<uint8>& GetGeneralWeapons() const { return GeneralWeapons; }
    int32 GetGeneralWeaponRank(uint8 Weapon) const;
    bool IsGeneralWeaponEvolved(uint8 Weapon) const;
    float GetCombatDamageMultiplier() const;
    int32 GetRunStatCount() const;
    static constexpr float WindWallHalfWidth = 360.f;
    static constexpr float WindWallHalfDepth = 40.f;
    static constexpr float WindWallHalfHeight = 135.f;
    bool GetWindWallBoundary(double ServerTime, FVector& Center, FVector& Normal) const
    {
        if (CharacterChoice != 5 || CombatHealth <= 0.f || ServerTime >= WindWallEnd || bDistrictFinished) { return false; }
        Center = WindWallCenter;
        Normal = WindWallNormal;
        return true;
    }
    bool TraceWindWall(FVector Start, FVector End, float ProjectileRadius, float& HitTime) const;
    int32 GetRunGold() const { return RunGold; }
    void NotifyGeneralWeaponEnemyDeath(FVector Position);
    UFUNCTION(Client, Unreliable) void ClientPlayCombatCue(uint8 Cue);
    void CollectWeaponHealth() { CombatHealth = FMath::Min(MaxCombatHealth, CombatHealth + MaxCombatHealth * .15f); }
    void CollectRunGold(int32 Amount, FVector Origin);
    int32 GetExperience() const { return Experience; }
    int32 GetExperienceRequired() const;
    void SelectLevelCard(int32 Slot) { ServerSelectLevelChoice(Slot, LevelChoiceSequence); }
    float GetLevelChoiceTimeRemaining() const;
    void CollectAccessCard();
    void BeginLabEvacuation(FVector Destination);
    bool HasFinishedDistrict() const { return bDistrictFinished; }
    bool HasTimedInvulnerability(double ServerTime) const
    {
        return CombatHealth > 0.f && !bDistrictFinished && ServerTime < InvulnerableEnd;
    }
    bool HasSeraphineShield(double ServerTime) const
    {
        return CombatHealth > 0.f && !bDistrictFinished && SeraphineShield > 0.f && ServerTime < SeraphineShieldEnd;
    }
    bool GetLeonaEclipseZone(double ServerTime, FVector& Center, float& Radius) const
    {
        if (CharacterChoice != 2 || ServerTime >= LeonaEclipseZoneEnd || bDistrictFinished) { return false; }
        Center = GetActorLocation() - FVector::UpVector * 55.f;
        Radius = LeonaEclipseZoneRadius;
        return true;
    }
    bool GetLeonaSolarZone(double ServerTime, FVector& Center, float& Radius) const
    {
        if (CharacterChoice != 2 || ServerTime >= LeonaSolarZoneEnd || bDistrictFinished) { return false; }
        Center = LeonaSolarZoneCenter + FVector::UpVector * 6.f;
        Radius = LeonaSolarZoneRadius;
        return true;
    }
    static constexpr float RivenAfterimageRadius = 250.f;
    bool GetRivenAfterimage(double ServerTime, FVector& Center) const
    {
        if (CharacterChoice != 6 || ServerTime >= RivenAfterimageAt || CombatHealth <= 0.f || bDistrictFinished) { return false; }
        Center = RivenAfterimageLocation - FVector::UpVector * 55.f;
        return true;
    }
    static constexpr float AuroraRealmRadius = 650.f;
    bool GetAuroraRealmBoundary(double ServerTime, FVector& Center) const
    {
        if (CharacterChoice != 7 || ServerTime >= AuroraRealmEnd || CombatHealth <= 0.f || bDistrictFinished) { return false; }
        Center = AuroraRealmCenter - FVector::UpVector * 55.f;
        return true;
    }
    const TArray<FLandedXayahFeather>& GetLandedXayahFeathers() const { return LandedXayahFeathers; }
    const TArray<FIllaoTentacleVisual>& GetTentacleVisuals() const { return TentacleVisuals; }
    void ShowLabFreezePulse(int32 Station);
    bool GetLabFreezePulse(double ServerTime, FVector& Center, float& Opacity) const;
    bool IsCarryingArenaBattery() const { return bCarryingArenaBattery; }
    void SetCarryingArenaBattery(bool bCarrying) { bCarryingArenaBattery = bCarrying; ForceNetUpdate(); }
    bool HasClearedDistrict() const { return bDistrictCleared; }
    void ReturnToSoloMenu();
    void CompleteDistrictRun(bool bVictory);
    UFUNCTION(Client, Reliable) void ClientRecordDistrictProgress(int32 Level, int32 ClearedArena, int32 EarnedGold, int32 MaximumHealth);
    bool IsAccessRewardOpen() const { return bAccessRewardOpen; }
    const TArray<uint8>& GetLevelChoices() const { return LevelChoices; }
    void CollectExperience(int32 Amount, FVector Origin, bool bCollectedLocally = true);
    void HealForEncounter(float Fraction);
    float GetBaseCriticalChance() const { return CharacterChoice == 5 ? .20f : .05f; }
    virtual float TakeDamage(float Damage, const FDamageEvent& Event, AController* InstigatorController, AActor* Causer) override;
    float GetJinxSkillRemaining(bool bUltimate) const;
    float GetCombatCooldown(float BaseCooldown) const;
    float GetCombatDurationMultiplier() const;
    float GetCombatAreaMultiplier() const;
    int32 GetCombatBonusProjectiles() const;
    int32 GetRunStatRank(uint8 Choice) const;
    float GetBriarFrenzyRemaining() const;
    int32 GetExtraSkillCharges() const { return ExtraSkillCharges; }
    float GetYasuoFlow() const { return YasuoFlow; }
    float GetRivenCharge() const { return RivenCharge; }
    int32 GetSignatureWeaponLevel() const { return SignatureWeaponLevel; }
    bool IsSignatureWeaponEvolved() const { return bSignatureWeaponEvolved; }
    bool CanEvolveSignatureWeapon() const;

private:
    void SelectCharacter(int32 Index);
    void ApplyCharacterChoice(int32 Index);

    UFUNCTION() void OnRep_CharacterChoice();
    UFUNCTION(Server, Reliable) void ServerChooseCharacter(int32 Index);
    UFUNCTION(Server, Unreliable) void ServerUpdateAim(float Yaw, FVector_NetQuantize AimPoint);
    UPROPERTY(ReplicatedUsing=OnRep_CharacterChoice) int32 CharacterChoice = 0;
    void FireJinxWeapon(float Now);
    void FireLeonaWeapon(float Now);
    void CastAdditionalSkill(bool bUltimate, float Now, FVector Aim);
    void TickAdditionalCombat(float Now, float DeltaTime);
    void FireAdditionalWeapon(float Now);
    void FireAuroraWeapon(float Now);
    void FireXayahWeapon(float Now);
    void CastXayahSkill(bool bUltimate, float Now);
    void TickXayahFeathers(float Now, float DeltaTime);
    void ThrowXayahFeather(FVector Direction, float Damage, float Now);
    void HitXayahPath(FVector Start, FVector End, float Damage, bool bDecay);
    float GetXayahCooldown(float BaseCooldown) const;
    struct FXayahFeather
    {
        FVector Position;
        FVector Destination;
        TSet<TWeakObjectPtr<AActor>> HitActors;
        float Damage;
        float ExpiresAt;
        bool bReturning = false;
        bool bLanded = false;
        FVector LaunchDirection = FVector::ZeroVector;
        float NextVisualAt = 0.f;
        FVector LastVisualPosition = FVector::ZeroVector;
    };
    TArray<FXayahFeather> XayahFeathers;
    UPROPERTY(Replicated) TArray<FLandedXayahFeather> LandedXayahFeathers;
    UPROPERTY(Replicated) int32 XayahLootCount = 0;
    UPROPERTY(Replicated) float XayahBuffEnd = 0.f;
    UPROPERTY(Replicated) float XayahPickupEnd = 0.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat|Xayah", meta=(ClampMin="0.1", ClampMax="5.0"))
    float XayahDurationMultiplier = 1.f;
    void CastAuroraSkill(bool bUltimate, float Now);
    void TickAuroraCombat(float Now, float DeltaTime);
    void CurseAuroraEnemy(AActor* Target, float Damage, float Now);
    UFUNCTION(NetMulticast, Unreliable) void PlayCombatMotion(uint8 Motion);
    void SpawnCombatParticle(uint8 Effect, FVector Location, FRotator Rotation, FVector Scale);
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> CombatMusic;
    float NextExperienceSoundAt = 0.f;
    void SpawnCombatCastEffect(uint8 Motion);
    ACombatVfx* GetCombatVfx();
    void PreloadCombatVfxAssets();
    UPROPERTY(Transient) TArray<TObjectPtr<UObject>> CombatVfxAssets;
    void UpdateSkillVfx();
    void UpdateGeneralSummonMeshes();
    void PublishGeneralSummons(float Now);
    UPROPERTY(Replicated) TArray<FGeneralSummonVisual> GeneralSummonVisuals;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> GeneralSummonMeshes;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> TibberSummonAsset;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> YuumiSummonAsset;
    float NextSummonSnapshotAt=0.f;
    UPROPERTY(Transient) TObjectPtr<ACombatVfx> CombatVfx;
    void ShowWeaponVfx(uint8 Weapon, FVector_NetQuantize Start,
        FVector_NetQuantize End, float Radius, uint8 Phase, bool bEvolved);
    void RenderWeaponVfx(uint8 Weapon,FVector_NetQuantize Start,FVector_NetQuantize End,float Radius,uint8 Phase,bool bEvolved);
    void RenderJinxTracer(FVector_NetQuantize Start,FVector_NetQuantize End,uint8 Variant);
    void RenderJinxExplosion(FVector_NetQuantize Position,float Radius,bool bUltimate);
    void RenderSeraphineEffect(FVector_NetQuantize Center,float Radius,bool bUltimate);
    void RenderLeonaShieldStrike(FVector_NetQuantize Center,FVector_NetQuantizeNormal Direction,float Radius,bool bEvolved);
    void QueueCombatVfx(FCombatVfxEvent Event);
    void DispatchCombatVfx(const FCombatVfxEvent& Event);
    void FlushCombatVfx(float Now);
    TArray<FCombatVfxEvent> PendingCombatVfx;
    float NextVfxBatchAt=0.f;
    UFUNCTION(NetMulticast,Unreliable) void MulticastCombatVfxBatch(const TArray<FCombatVfxEvent>& Events);
    void UpdateCombatMotion(float DeltaTime, bool bMoving);

    struct FAuroraBolt
    {
        FVector Position;
        FVector Direction;
        float Remaining;
        float Damage;
        bool bHoming;
        float NextVisualAt = 0.f;
        FVector LastVisualPosition = FVector::ZeroVector;
    };
    TArray<FAuroraBolt> AuroraBolts;
    TMap<TWeakObjectPtr<AActor>, float> AuroraCurses;
    TSet<TWeakObjectPtr<AActor>> AuroraContactHits;
    UPROPERTY(Replicated) float AuroraVeilEnd = 0.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat|Aurora", meta=(ClampMin="0.1", ClampMax="5.0"))
    float AuroraDurationMultiplier = 1.f;
    FVector AuroraAimPoint = FVector::ZeroVector;
    UPROPERTY(Replicated) FVector AuroraRealmCenter = FVector::ZeroVector;
    UPROPERTY(Replicated) float AuroraRealmEnd = 0.f;
    float AuroraCrossingReady = 0.f;
    int32 AuroraWeaponCasts = 0;
    UPROPERTY(Replicated) int32 CombatLevel = 1;
    UPROPERTY(Replicated) int32 Experience = 0;
    UPROPERTY(Replicated) TArray<uint8> LevelChoices;
    UPROPERTY(ReplicatedUsing=OnRep_LevelChoiceDeadline) float LevelChoiceDeadline = 0.f;
    UPROPERTY(Replicated) int32 LevelChoiceSequence = 0;
    UPROPERTY(Replicated) float RunMoveBonus = 0.f;
    UPROPERTY(Replicated) float RunArmorBonus = 0.f;
    UPROPERTY(Replicated) float RunRegen = 0.f;
    UPROPERTY(Replicated) float RunHealthBonus = 0.f;
    UPROPERTY(Replicated) float LabSprintEnd = 0.f;
    UPROPERTY(Replicated) bool bDistrictFinished = false;
    bool bResultAudioHandled = false;
    UPROPERTY(Replicated) bool bDistrictCleared = false;
    UPROPERTY(Replicated) bool bCarryingArenaBattery = false;
    UPROPERTY(Replicated) double LabFreezePulseUntil = 0.0;
    UPROPERTY(Replicated) int32 LabFreezePulseStation = INDEX_NONE;
    int32 PendingLevelChoices = 0;
    int32 PendingAccessCards = 0;
    UPROPERTY(Replicated) bool bAccessRewardOpen = false;
    UPROPERTY(Replicated) TArray<uint8> EvolvedGeneralWeapons;
    bool CanEvolveGeneralWeapon(uint8 Weapon) const;
    UPROPERTY(Replicated) TArray<uint8> GeneralWeapons;
    UPROPERTY(Replicated) TArray<uint8> GeneralWeaponRanks;
    UPROPERTY(Replicated) TArray<uint8> RunWeaponStats;
    UPROPERTY(Replicated) float PermanentExperienceMultiplier = 1.f;
    UPROPERTY(Replicated) float PermanentDamageMultiplier = 1.f;
    UPROPERTY(Replicated) float PermanentAbilityHaste = 0.f;
    UPROPERTY(Replicated) float PermanentDurationMultiplier = 1.f;
    UPROPERTY(Replicated) float PermanentAreaMultiplier = 1.f;
    UPROPERTY(Replicated) int32 RunGold = 0;
    float GeneralWeaponReadyAt[GeneralWeaponCount] = {};
    float NextWeaponImpactVfxAt[GeneralWeaponCount] = {};
    UPROPERTY(Replicated) float TrailBoostEnd = 0.f;
    UPROPERTY(Replicated) int32 TrailBoostStacks = 0;
    struct FWeaponZone
    {
        uint8 Weapon = 0;
        FVector Position = FVector::ZeroVector;
        FVector Direction = FVector::ForwardVector;
        float Radius = 0.f;
        float Damage = 0.f;
        float NextTick = 0.f;
        float EndAt = 0.f;
        float Interval = 0.f;
        int32 Ticks = 0;
        bool bEvolved = false;
    };
    TArray<FWeaponZone> WeaponZones;
    float IceShieldRemaining = 0.f;
    float IceShieldEnd = 0.f;
    float YuumiDamage = 0.f;
    TArray<FVector> RadianceExplosions;
    void TriggerIceBurst(float Now);
    void TickGeneralWeapons(float Now, float DeltaTime);
    bool UpgradeGeneralWeapon(uint8 Weapon);
    void FireGeneralWeapon(uint8 Weapon, float Now);
    struct FGeneralProjectile
    {
        uint8 Weapon=0;
        FVector Position,Direction,LastVisual;
        float Remaining=0.f,Damage=0.f,Radius=0.f,Speed=0.f,NextVisualAt=0.f;
        int32 Bounces=0;
        bool bEvolved=false,bFirstOnly=false;
        TSet<TWeakObjectPtr<AActor>> HitActors;
    };
    TArray<FGeneralProjectile> GeneralProjectiles;
    void LaunchGeneralProjectile(uint8 Weapon,FVector Start,FVector End,float Width,float Damage,bool bFirstOnly);
    void TickGeneralProjectiles(float Now,float DeltaTime);
    bool ApplyGeneralWeaponDamage(AActor* Target, float Damage, uint8 Weapon, bool bCrit, bool bGold);
    void RollLevelChoices();
    float GetLevelChoiceClock() const;
    void BeginLevelChoiceFreeze();
    void EndLevelChoiceFreeze();
    UFUNCTION() void OnRep_LevelChoiceDeadline();
    float LocalLevelChoiceDeadline = 0.f;
    UFUNCTION(Server, Reliable) void ServerSelectLevelChoice(int32 Slot, int32 Sequence);
    bool bAuroraAutoAim = false;
    float CombatMotionTime = 0.f;
    uint8 CurrentCombatMotion = 0;
    FVector BaseMeshLocation = FVector::ZeroVector;
    FRotator BaseMeshRotation = FRotator::ZeroRotator;
    UFUNCTION(Server, Reliable) void ServerToggleAuroraAutoAim();

    void CastRivenSkill(bool bUltimate, float Now);
    void TickRivenCombat(float Now, float DeltaTime);
    void BeginRivenDash(float Now, bool bSkill);
    void FinishRivenDash(float Now);
    void HitRivenEnemy(AActor* Target, float Damage, float Now);

    UPROPERTY(Replicated) float RivenCharge = 0.f;
    UPROPERTY(Replicated) float RivenUltimateEnd = 0.f;
    UPROPERTY(Replicated) bool bRivenDashing = false;
    UPROPERTY(EditDefaultsOnly, Category="Combat|Riven", meta=(ClampMin="0.1", ClampMax="5.0"))
    float RivenDurationMultiplier = 1.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat|Riven", meta=(ClampMin="0", ClampMax="12"))
    int32 RivenBonusProjectiles = 0;
    FVector RivenDashDirection = FVector::ForwardVector;
    float RivenDashDistance = 0.f;
    UPROPERTY(Replicated) float RivenAfterimageAt = 0.f;
    UPROPERTY(Replicated) FVector RivenAfterimageLocation = FVector::ZeroVector;
    float RivenShieldEnd = 0.f;
    float RivenShield = 0.f;
    int32 RivenLeapCount = 0;
    bool bRivenSkillDash = false;
    bool SpendExtraCharge(float Now, float Recovery);
    void SpawnTentacle(FVector Position, float Now);
    void FinishBriarFrenzy(float Now);
    TArray<AActor*> FindCombatTargets(FVector Center, float Radius) const;
    void HitAdditionalEnemy(AActor* Target, float Damage);

    struct FCombatHazard
    {
        FVector Position;
        float Radius;
        float Damage;
        float NextTick;
        float End;
        float Interval;
    };
    TArray<FCombatHazard> CombatHazards;
    UPROPERTY(Replicated) TArray<FIllaoTentacleVisual> TentacleVisuals;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> TentacleMeshes;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> TentacleMeshAsset;
    void UpdateTentacleMeshes();
    void UpdateWindWallVisual();
    void UpdateXayahFeatherMeshes();
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> XayahFeatherMeshes;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> XayahFeatherMeshAsset;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WindWallVisual;
    struct FBriarBleed
    {
        TArray<float> Expirations;
        float NextTick = 0.f;
    };
    TMap<TWeakObjectPtr<AActor>, FBriarBleed> BriarBleeds;
    UPROPERTY(Replicated) int32 ExtraSkillCharges = 0;
    UPROPERTY(Replicated) float BriarFrenzyEnd = 0.f;
    UPROPERTY(Replicated) float YasuoFlow = 0.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat|Illaoi", meta=(ClampMin="0", ClampMax="12"))
    int32 IllaoiBonusProjectiles = 0;
    float ExtraChargeReadyAt = 0.f;
    bool bBriarUltimateFrenzy = false;
    UPROPERTY(Replicated) FVector WindWallCenter = FVector::ZeroVector;
    UPROPERTY(Replicated) FVector WindWallNormal = FVector::ForwardVector;
    UPROPERTY(Replicated) float WindWallEnd = 0.f;
    void CastLeonaSkill(bool bUltimate, float Now, FVector Aim);
    void TickLeonaZones(float Now);
    void DamageLeonaArea(FVector Position, float Radius, float Damage, bool bStun, bool bShieldStrike);
    float GetLeonaArmor(float Now) const;
    float GetLeonaAreaMultiplier() const;
    float ConsumeSunlight(AActor* Target) const;

    struct FLeonaZone
    {
        FVector Position;
        float Radius;
        float NextTick;
        float EndsAt;
        bool bFollowsLeona;
    };
    TArray<FLeonaZone> LeonaZones;
    UPROPERTY(Replicated) FVector LeonaSolarZoneCenter = FVector::ZeroVector;
    UPROPERTY(Replicated) float LeonaSolarZoneRadius = 0.f;
    UPROPERTY(Replicated) float LeonaSolarZoneEnd = 0.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat|Leona") float BaseArmor = 0.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat|Leona") float LeonaBonusArmor = 0.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat|Leona") float LeonaBonusRegen = 0.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat|Leona") float LeonaHealthFromStatPassives = 0.f;
    float LeonaEclipseEnd = 0.f;
    UPROPERTY(Replicated) float LeonaEclipseZoneEnd = 0.f;
    UPROPERTY(Replicated) float LeonaEclipseZoneRadius = 0.f;
    float LeonaShield = 0.f;
    void FireSeraphineWeapon(float Now);
    void CastSeraphineSkill(bool bUltimate, float Now);
    void TickSeraphineWaves(float DeltaTime);
    static constexpr float LeonaShieldHalfAngle = 60.f;
    void ShowLeonaShieldStrike(FVector_NetQuantize Center, FVector_NetQuantizeNormal Direction, float Radius, bool bEvolved);
    void ShowSeraphineEffect(FVector_NetQuantize Center, float Radius, bool bUltimate);
    struct FSeraphineWave
    {
        FVector Position;
        FVector Destination;
        FVector Direction;
        TSet<TWeakObjectPtr<AActor>> HitEnemies;
        bool bEvolved;
        bool bReturning = false;
        float Damage;
        float NextVisualAt = 0.f;
        FVector LastVisualPosition = FVector::ZeroVector;
    };
    TArray<FSeraphineWave> SeraphineWaves;
    UPROPERTY(Replicated) float MaxCombatHealth = 1100.f;
    UPROPERTY(Replicated) float CombatHealth = 1100.f;
    UPROPERTY(Replicated) float SeraphineShield = 0.f;
    UPROPERTY(Replicated) float SeraphineShieldEnd = 0.f;
    UPROPERTY(Replicated) float InvulnerableEnd = 0.f;
    UFUNCTION(Server, Reliable) void ServerCastCharacterSkill(bool bUltimate, FVector_NetQuantize Aim);
    void ShowJinxExplosion(FVector_NetQuantize Position, float Radius, bool bUltimate);
    void TickJinxRockets(float DeltaTime);
    void ExplodeJinxRocket(FVector Position, bool bUltimate, AActor* DirectTarget);

    struct FJinxRocket
    {
        FVector Position;
        FVector Destination;
        bool bUltimate;
        float NextVisualAt = 0.f;
        FVector LastVisualPosition = FVector::ZeroVector;
    };
    TArray<FJinxRocket> JinxRockets;
    UPROPERTY(Replicated) float JinxEReadyAt = 0.f;
    UPROPERTY(Replicated) float JinxRReadyAt = 0.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float JinxERadius = 145.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float JinxRRadius = 300.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float JinxEDamage = 35.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat")
    float JinxRDamage = 120.f;
    void ShowJinxTracer(FVector_NetQuantize Start, FVector_NetQuantize End, uint8 Variant = 0);
    float GetCombatTime() const;
    UPROPERTY(Replicated) int32 EnemyKills = 0;
    UPROPERTY(Replicated) float JinxPassiveEnd = 0.f;
    UPROPERTY(Replicated) float JinxWeaponReadyAt = 0.f;
    UPROPERTY(Replicated) int32 SignatureWeaponLevel = 1;
    UPROPERTY(Replicated) bool bSignatureWeaponEvolved = false;
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="0.0"))
    float JinxInitialSpeedBonus = 0.75f;
    float BaseMoveSpeed = 450.f;
    float LastAimSendTime = -1.f;

    UPROPERTY() TObjectPtr<USpringArmComponent> CameraArm;
    UPROPERTY() TObjectPtr<UCameraComponent> Camera;
    UPROPERTY() TObjectPtr<UAnimSequence> Idle;
    UPROPERTY() TObjectPtr<UAnimSequence> Run;
    int32 SelectedIndex = INDEX_NONE;
    bool bWasMoving = false;
    bool bDistrictCamera = false;
    float DesiredCameraDistance = 0.f;
};

UCLASS()
class TROUBLESQUAD_API AAnimaPreviewMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AAnimaPreviewMode();
    void BeginPlay() override;

    virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
        const FString& Options, const FString& Portal) override;
    virtual void PreLogin(const FString& Options, const FString& Address,
        const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual void Logout(AController* Exiting) override;
    int32 GetCharacterChoice(const AController* Controller) const;

private:
    TMap<const AController*, int32> CharacterChoices;
    FTimerHandle EnemyWaveTimer;
    TWeakObjectPtr<AActor> LabChamberGate;
    int32 EnemiesSpawned = 0;
    int32 NextEncounterEvent = 0;
    float RunStartedAt = 0.f;
    float NextRegularSpawn = 0.f;
    float HealingCharge = 0.f;
    float HealingReadyAt = 120.f;
    void TickHealingStation(float Now);
    void TickArenaGimmick(float Now);
    TWeakObjectPtr<AAnimaPreviewCharacter> BatteryCarrier;
    float ArenaGimmickReadyAt = 0.f;
    float SecondFreezeReadyAt = 0.f;
    void SpawnWaveEnemy();
public:
    void CompleteDistrictEncounter(bool bVictory);
    void CheckDistrictDefeat();
    void SetLevelChoicePending(AAnimaPreviewCharacter* Character, bool bPending);
private:
    TSet<TWeakObjectPtr<AAnimaPreviewCharacter>> PendingChoicePlayers;
    TOptional<float> TimeDilationBeforeLevelChoice;

};

