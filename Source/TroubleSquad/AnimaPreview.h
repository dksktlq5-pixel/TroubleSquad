#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "AnimaPreview.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UAnimSequence;

UCLASS()
class TROUBLESQUAD_API AAnimaPreviewCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AAnimaPreviewCharacter();
    int32 GetSelectedCharacter() const { return CharacterChoice; }
    virtual void BeginPlay() override;
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
    float GetBaseCriticalChance() const { return CharacterChoice == 5 ? .20f : .05f; }
    virtual float TakeDamage(float Damage, const FDamageEvent& Event, AController* InstigatorController, AActor* Causer) override;
    float GetJinxSkillRemaining(bool bUltimate) const;
    int32 GetExtraSkillCharges() const { return ExtraSkillCharges; }
    float GetYasuoFlow() const { return YasuoFlow; }
    float GetRivenCharge() const { return RivenCharge; }
    int32 GetSignatureWeaponLevel() const { return SignatureWeaponLevel; }
    bool IsSignatureWeaponEvolved() const { return SignatureWeaponLevel == 5; }

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
    };
    TArray<FXayahFeather> XayahFeathers;
    UPROPERTY(Replicated) int32 XayahLootCount = 0;
    UPROPERTY(Replicated) float XayahBuffEnd = 0.f;
    UPROPERTY(Replicated) float XayahPickupEnd = 0.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat|Xayah", meta=(ClampMin="0.1", ClampMax="5.0"))
    float XayahDurationMultiplier = 1.f;
    void CastAuroraSkill(bool bUltimate, float Now);
    void TickAuroraCombat(float Now, float DeltaTime);
    void CurseAuroraEnemy(AActor* Target, float Damage, float Now);
    UFUNCTION(NetMulticast, Unreliable) void PlayCombatMotion(uint8 Motion);
    void UpdateCombatMotion(float DeltaTime, bool bMoving);

    struct FAuroraBolt
    {
        FVector Position;
        FVector Direction;
        float Remaining;
        float Damage;
        bool bHoming;
    };
    TArray<FAuroraBolt> AuroraBolts;
    TMap<TWeakObjectPtr<AActor>, float> AuroraCurses;
    TSet<TWeakObjectPtr<AActor>> AuroraContactHits;
    UPROPERTY(Replicated) float AuroraVeilEnd = 0.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat|Aurora", meta=(ClampMin="0.1", ClampMax="5.0"))
    float AuroraDurationMultiplier = 1.f;
    FVector AuroraAimPoint = FVector::ZeroVector;
    FVector AuroraRealmCenter = FVector::ZeroVector;
    float AuroraRealmEnd = 0.f;
    float AuroraCrossingReady = 0.f;
    int32 AuroraWeaponCasts = 0;
    int32 CombatLevel = 1;
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
    float RivenAfterimageAt = 0.f;
    FVector RivenAfterimageLocation = FVector::ZeroVector;
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
    FVector WindWallCenter = FVector::ZeroVector;
    FVector WindWallNormal = FVector::ForwardVector;
    float WindWallEnd = 0.f;
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
    UPROPERTY(EditDefaultsOnly, Category="Combat|Leona") float BaseArmor = 0.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat|Leona") float LeonaBonusArmor = 0.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat|Leona") float LeonaBonusRegen = 0.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat|Leona") float LeonaHealthFromStatPassives = 0.f;
    float LeonaEclipseEnd = 0.f;
    float LeonaShield = 0.f;
    void FireSeraphineWeapon(float Now);
    void CastSeraphineSkill(bool bUltimate, float Now);
    void TickSeraphineWaves(float DeltaTime);
    UFUNCTION(NetMulticast, Unreliable) void ShowSeraphineEffect(FVector_NetQuantize Center, float Radius, bool bUltimate);
    struct FSeraphineWave
    {
        FVector Position;
        FVector Destination;
        FVector Direction;
        TSet<TWeakObjectPtr<AActor>> HitEnemies;
        bool bEvolved;
        bool bReturning = false;
        float Damage;
    };
    TArray<FSeraphineWave> SeraphineWaves;
    UPROPERTY(EditDefaultsOnly, Category="Combat") float MaxCombatHealth = 1100.f;
    UPROPERTY(Replicated) float CombatHealth = 1100.f;
    UPROPERTY(Replicated) float SeraphineShield = 0.f;
    UPROPERTY(Replicated) float SeraphineShieldEnd = 0.f;
    UPROPERTY(Replicated) float InvulnerableEnd = 0.f;
    UFUNCTION(Server, Reliable) void ServerCastCharacterSkill(bool bUltimate, FVector_NetQuantize Aim);
    UFUNCTION(NetMulticast, Unreliable) void ShowJinxExplosion(FVector_NetQuantize Position, float Radius, bool bUltimate);
    void TickJinxRockets(float DeltaTime);
    void ExplodeJinxRocket(FVector Position, bool bUltimate, AActor* DirectTarget);

    struct FJinxRocket
    {
        FVector Position;
        FVector Destination;
        bool bUltimate;
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
    UFUNCTION(NetMulticast, Unreliable) void ShowJinxTracer(FVector_NetQuantize Start, FVector_NetQuantize End);
    float GetCombatTime() const;
    UPROPERTY(Replicated) int32 EnemyKills = 0;
    UPROPERTY(Replicated) float JinxPassiveEnd = 0.f;
    UPROPERTY(Replicated) float JinxWeaponReadyAt = 0.f;
    UPROPERTY(Replicated) int32 SignatureWeaponLevel = 1;
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

    virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
        const FString& Options, const FString& Portal) override;
    virtual void PreLogin(const FString& Options, const FString& Address,
        const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual void Logout(AController* Exiting) override;
    int32 GetCharacterChoice(const AController* Controller) const;

private:
    TMap<const AController*, int32> CharacterChoices;

};

