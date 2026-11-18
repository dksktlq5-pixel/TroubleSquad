#include "AnimaPreview.h"
#include "CombatVfx.h"
#include "SwarmArenas.h"
#include "SwarmEncounter.h"
#include "CombatAudio.h"
#include "DistrictProgress.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "CombatHUD.h"
#include "CombatHealth.h"

#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/GameStateBase.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

namespace
{
constexpr float DistrictDefaultZoom = 1025.f;
constexpr float DistrictClosestZoom = 725.f;
constexpr float DistrictZoomStep = 100.f;
constexpr float DistrictCameraYaw = -90.f;
constexpr float JinxPassiveDuration = 8.f;
const FName SunlightTag(TEXT("LeonaSunlight"));
constexpr int32 JinxKillsPerActivation = 70;

const TCHAR* const Roster[] = {TEXT("Jinx"), TEXT("Seraphine"), TEXT("Leona"),
    TEXT("Illaoi"), TEXT("Briar"), TEXT("Yasuo"), TEXT("Riven"), TEXT("Aurora"), TEXT("Xayah")};
struct FChampionBaseStats
{
    float Health;
    float Armor;
    float MoveSpeed;
};
constexpr FChampionBaseStats ChampionBaseStats[] = {
    {1100.f, 8.f, 450.f},   // Jinx
    {1150.f, 10.f, 430.f},  // Seraphine
    {1500.f, 25.f, 405.f},  // Leona
    {1450.f, 20.f, 410.f},  // Illaoi
    {1350.f, 15.f, 435.f},  // Briar
    {1250.f, 12.f, 460.f},  // Yasuo
    {1300.f, 15.f, 470.f},  // Riven
    {1100.f, 8.f, 455.f},   // Aurora
    {1150.f, 10.f, 445.f},  // Xayah
};
static_assert(UE_ARRAY_COUNT(ChampionBaseStats) == UE_ARRAY_COUNT(Roster));
const FKey CharacterKeys[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
    EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
}

AAnimaPreviewCharacter::AAnimaPreviewCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(36.f, 88.f);
    GetMesh()->SetRelativeLocation(FVector(0, 0, -88));
    GetCharacterMovement()->MaxWalkSpeed = 450.f;
    bUseControllerRotationYaw = false;
    bReplicates = true;

    CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
    CameraArm->SetupAttachment(GetRootComponent());
    CameraArm->SetUsingAbsoluteRotation(true);
    CameraArm->SetRelativeRotation(FRotator(-55.f, 0.f, 0.f));
    CameraArm->TargetArmLength = 660.f;
    CameraArm->bDoCollisionTest = false;
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(CameraArm, USpringArmComponent::SocketName);
}

void AAnimaPreviewCharacter::BeginPlay()
{
    PreloadCombatVfxAssets();
#if WITH_EDITOR
    if(FParse::Param(FCommandLine::Get(),TEXT("VfxNetworkReview")))
    { InvulnerableEnd=GetCombatTime()+120.f; JinxWeaponReadyAt=GetCombatTime()+120.f; }
#endif
    Super::BeginPlay();

    bDistrictCamera = SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this)) != INDEX_NONE;
    if (bDistrictCamera)
    {
        GetCapsuleComponent()->SetCapsuleSize(31.f, 88.f);
        GetMesh()->SetRelativeScale3D(FVector(.95f));
        GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -79.f));
        DesiredCameraDistance = DistrictDefaultZoom;
        CameraArm->TargetArmLength = DesiredCameraDistance;
        CameraArm->SetRelativeRotation(FRotator(-61.f, DistrictCameraYaw, 0.f));
    }

    if (HasAuthority() && GetController())
    {
        if (const AAnimaPreviewMode* Mode = GetWorld()->GetAuthGameMode<AAnimaPreviewMode>())
        {
            ApplyCharacterChoice(Mode->GetCharacterChoice(GetController()));
        }
    }
    SelectCharacter(CharacterChoice);
}

void AAnimaPreviewCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    if (const AAnimaPreviewMode* Mode = GetWorld()->GetAuthGameMode<AAnimaPreviewMode>())
    {
        ApplyCharacterChoice(Mode->GetCharacterChoice(NewController));
    }
}

void AAnimaPreviewCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AAnimaPreviewCharacter, CharacterChoice);
    DOREPLIFETIME(AAnimaPreviewCharacter, CombatLevel);
    DOREPLIFETIME(AAnimaPreviewCharacter, Experience);
    DOREPLIFETIME(AAnimaPreviewCharacter, LevelChoices);
    DOREPLIFETIME(AAnimaPreviewCharacter, bDistrictFinished);
    DOREPLIFETIME(AAnimaPreviewCharacter, LabSprintEnd);
    DOREPLIFETIME(AAnimaPreviewCharacter, bDistrictCleared);
    DOREPLIFETIME(AAnimaPreviewCharacter, bCarryingArenaBattery);
    DOREPLIFETIME(AAnimaPreviewCharacter, LabFreezePulseUntil);
    DOREPLIFETIME(AAnimaPreviewCharacter, AuroraRealmCenter);
    DOREPLIFETIME(AAnimaPreviewCharacter, AuroraRealmEnd);
    DOREPLIFETIME(AAnimaPreviewCharacter, WindWallCenter);
    DOREPLIFETIME(AAnimaPreviewCharacter, WindWallNormal);
    DOREPLIFETIME(AAnimaPreviewCharacter, WindWallEnd);
    DOREPLIFETIME(AAnimaPreviewCharacter, RivenAfterimageAt);
    DOREPLIFETIME(AAnimaPreviewCharacter, LeonaSolarZoneCenter);
    DOREPLIFETIME(AAnimaPreviewCharacter, LeonaEclipseZoneEnd);
    DOREPLIFETIME(AAnimaPreviewCharacter, LeonaEclipseZoneRadius);
    DOREPLIFETIME(AAnimaPreviewCharacter, LeonaSolarZoneRadius);
    DOREPLIFETIME(AAnimaPreviewCharacter, LeonaSolarZoneEnd);
    DOREPLIFETIME(AAnimaPreviewCharacter, RivenAfterimageLocation);
    DOREPLIFETIME(AAnimaPreviewCharacter, LandedXayahFeathers);
    DOREPLIFETIME(AAnimaPreviewCharacter, TentacleVisuals);
    DOREPLIFETIME(AAnimaPreviewCharacter, LabFreezePulseStation);
    DOREPLIFETIME(AAnimaPreviewCharacter, LevelChoiceDeadline);
    DOREPLIFETIME(AAnimaPreviewCharacter, LevelChoiceSequence);
    DOREPLIFETIME(AAnimaPreviewCharacter, EvolvedGeneralWeapons);
    DOREPLIFETIME(AAnimaPreviewCharacter, bAccessRewardOpen);
    DOREPLIFETIME(AAnimaPreviewCharacter, MaxCombatHealth);
    DOREPLIFETIME(AAnimaPreviewCharacter, RunMoveBonus);
    DOREPLIFETIME(AAnimaPreviewCharacter, RunArmorBonus);
    DOREPLIFETIME(AAnimaPreviewCharacter, RunHealthBonus);
    DOREPLIFETIME(AAnimaPreviewCharacter, RunRegen);
    DOREPLIFETIME(AAnimaPreviewCharacter, GeneralWeapons);
    DOREPLIFETIME(AAnimaPreviewCharacter, GeneralSummonVisuals);
    DOREPLIFETIME(AAnimaPreviewCharacter, GeneralWeaponRanks);
    DOREPLIFETIME(AAnimaPreviewCharacter, RunWeaponStats);
    DOREPLIFETIME(AAnimaPreviewCharacter, PermanentExperienceMultiplier);
    DOREPLIFETIME(AAnimaPreviewCharacter, PermanentDamageMultiplier);
    DOREPLIFETIME(AAnimaPreviewCharacter, PermanentAbilityHaste);
    DOREPLIFETIME(AAnimaPreviewCharacter, PermanentDurationMultiplier);
    DOREPLIFETIME(AAnimaPreviewCharacter, PermanentAreaMultiplier);
    DOREPLIFETIME(AAnimaPreviewCharacter, RunGold);
    DOREPLIFETIME(AAnimaPreviewCharacter, TrailBoostEnd);
    DOREPLIFETIME(AAnimaPreviewCharacter, TrailBoostStacks);
    DOREPLIFETIME(AAnimaPreviewCharacter, EnemyKills);
    DOREPLIFETIME(AAnimaPreviewCharacter, JinxPassiveEnd);
    DOREPLIFETIME(AAnimaPreviewCharacter, JinxWeaponReadyAt);
    DOREPLIFETIME(AAnimaPreviewCharacter, JinxEReadyAt);
    DOREPLIFETIME(AAnimaPreviewCharacter, JinxRReadyAt);
    DOREPLIFETIME(AAnimaPreviewCharacter, CombatHealth);
    DOREPLIFETIME(AAnimaPreviewCharacter, SeraphineShield);
    DOREPLIFETIME(AAnimaPreviewCharacter, SeraphineShieldEnd);
    DOREPLIFETIME(AAnimaPreviewCharacter, InvulnerableEnd);
    DOREPLIFETIME(AAnimaPreviewCharacter, ExtraSkillCharges);
    DOREPLIFETIME(AAnimaPreviewCharacter, BriarFrenzyEnd);
    DOREPLIFETIME(AAnimaPreviewCharacter, YasuoFlow);
    DOREPLIFETIME(AAnimaPreviewCharacter, AuroraVeilEnd);
    DOREPLIFETIME(AAnimaPreviewCharacter, XayahLootCount);
    DOREPLIFETIME(AAnimaPreviewCharacter, XayahBuffEnd);
    DOREPLIFETIME(AAnimaPreviewCharacter, XayahPickupEnd);
    DOREPLIFETIME(AAnimaPreviewCharacter, RivenCharge);
    DOREPLIFETIME(AAnimaPreviewCharacter, RivenUltimateEnd);
    DOREPLIFETIME(AAnimaPreviewCharacter, bRivenDashing);
    DOREPLIFETIME(AAnimaPreviewCharacter, SignatureWeaponLevel);
    DOREPLIFETIME(AAnimaPreviewCharacter, bSignatureWeaponEvolved);
}

void AAnimaPreviewCharacter::OnRep_CharacterChoice()
{
    SelectCharacter(CharacterChoice);
}

void AAnimaPreviewCharacter::ApplyCharacterChoice(int32 Index)
{
    if (Index < 0 || Index >= UE_ARRAY_COUNT(Roster))
    {
        return;
    }
    if (CharacterChoice != Index)
    {
        SignatureWeaponLevel = 1;
        bSignatureWeaponEvolved = false;
        JinxWeaponReadyAt = 0.f;
        JinxPassiveEnd = 0.f;
        JinxEReadyAt = JinxRReadyAt = 0.f;
        LeonaZones.Reset();
        LeonaSolarZoneEnd = LeonaEclipseZoneEnd = 0.f;
        LeonaShield = LeonaEclipseEnd = 0.f;
        CombatHazards.Reset();
        TentacleVisuals.Reset();
        XayahFeathers.Reset();
        LandedXayahFeathers.Reset();
        XayahLootCount = 0;
        XayahBuffEnd = XayahPickupEnd = 0.f;
        AuroraBolts.Reset();
        AuroraCurses.Reset();
        AuroraContactHits.Reset();
        AuroraVeilEnd = AuroraRealmEnd = AuroraCrossingReady = 0.f;
        AuroraWeaponCasts = 0;
        AuroraAimPoint = FVector::ZeroVector;
        bAuroraAutoAim = false;
        RivenCharge = RivenUltimateEnd = RivenDashDistance = RivenAfterimageAt = RivenShieldEnd = RivenShield = 0.f;
        RivenLeapCount = 0;
        bRivenDashing = false;
        BriarBleeds.Reset();
        BriarFrenzyEnd = YasuoFlow = ExtraChargeReadyAt = WindWallEnd = 0.f;
        ExtraSkillCharges = Index == 3 ? 2 : Index == 5 ? 3 : 0;
    }
    CharacterChoice = Index;
    SelectCharacter(CharacterChoice);
}

void AAnimaPreviewCharacter::ServerChooseCharacter_Implementation(int32 Index)
{
    ApplyCharacterChoice(Index);
}

void AAnimaPreviewCharacter::ServerUpdateAim_Implementation(float Yaw, FVector_NetQuantize AimPoint)
{
    if (FMath::IsFinite(Yaw))
    {
        SetActorRotation(FRotator(0.f, FRotator::NormalizeAxis(Yaw), 0.f));
        if (CharacterChoice == 7 && !AimPoint.ContainsNaN()) { AuroraAimPoint = AimPoint; }
    }
}


void AAnimaPreviewCharacter::PawnClientRestart()
{
    Super::PawnClientRestart();

    if (APlayerController* Player = Cast<APlayerController>(GetController());
        Player && Player->IsLocalController())
    {
        Player->bShowMouseCursor = true;
        Player->bEnableClickEvents = true;
        Player->bEnableClickEvents = true;
        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Player->SetInputMode(InputMode);
    }
}

void AAnimaPreviewCharacter::SelectCharacter(int32 Index)
{
    if (Index == SelectedIndex)
    {
        return;
    }

    const FChampionBaseStats& Stats = ChampionBaseStats[Index];
    const UDistrictProgress* Progress = IsLocallyControlled() && HasAuthority() &&
        SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this)) != INDEX_NONE ? UDistrictProgress::Load() : nullptr;
    const float BaseHealth = Stats.Health * (1.f + (Progress ? .10f * Progress->GetUpgradeRank(2) : 0.f));
    PermanentExperienceMultiplier = 1.f + (Progress ? .05f * Progress->GetUpgradeRank(10) : 0.f);
    if (HasAuthority())
    {
        const float HealthFraction = SelectedIndex == INDEX_NONE
            ? 1.f : FMath::Clamp(CombatHealth / FMath::Max(1.f, MaxCombatHealth), 0.f, 1.f);
        CombatHealth = (BaseHealth + RunHealthBonus) * HealthFraction;
    }
    if (HasAuthority()) { MaxCombatHealth = BaseHealth + RunHealthBonus; }
    BaseArmor = Stats.Armor;
    BaseMoveSpeed = Stats.MoveSpeed;
    PermanentDamageMultiplier = 1.f;
    PermanentAbilityHaste = Progress ? 5.f * Progress->GetUpgradeRank(9) + Progress->GetUpgradeRank(14) : 0.f;
    PermanentDurationMultiplier = 1.f + (Progress ? .05f * Progress->GetUpgradeRank(7) : 0.f);
    PermanentAreaMultiplier = 1.f + (Progress ? .05f * Progress->GetUpgradeRank(6) : 0.f);
    if (Progress)
    {
        PermanentDamageMultiplier += .10f * Progress->GetUpgradeRank(0);
        BaseArmor += 5.f * Progress->GetUpgradeRank(1);
        BaseMoveSpeed *= 1.f + .05f * Progress->GetUpgradeRank(4);
    }

    const FString Name(Roster[Index]);
    const FString Folder = TEXT("/Game/TroubleSquad/AnimaSquad/") + Name + TEXT("/");
    const FString MeshName = TEXT("SK_") + Name;
    USkeletalMesh* CharacterMesh = LoadObject<USkeletalMesh>(nullptr, *(Folder + MeshName + TEXT(".") + MeshName));
    if (!CharacterMesh)
    {
        return;
    }
    const FString IdleName = TEXT("AN_") + Name + TEXT("_Idle");
    const FString RunName = TEXT("AN_") + Name + TEXT("_Run");
    Idle = LoadObject<UAnimSequence>(nullptr, *(Folder + IdleName + TEXT(".") + IdleName));
    Run = LoadObject<UAnimSequence>(nullptr, *(Folder + RunName + TEXT(".") + RunName));
    GetMesh()->SetSkeletalMesh(CharacterMesh);
    // Jinx's imported mesh faces local +Y. The pawn aims along local +X.
    GetMesh()->SetRelativeRotation(FRotator(0.f, Index == 0 ? -90.f : 0.f, 0.f));
    BaseMeshLocation = GetMesh()->GetRelativeLocation();
    BaseMeshRotation = GetMesh()->GetRelativeRotation();
    CombatMotionTime = 0.f;
    GetMesh()->PlayAnimation(Idle, true);
    SelectedIndex = Index;
    bWasMoving = false;
    if (GEngine && IsLocallyControlled())
    {
        GEngine->AddOnScreenDebugMessage(27, 4.f, FColor::Cyan,
            FString::Printf(TEXT("%s   |   WASD move   |   Mouse aim   |   1-9 switch"), *Name));
    }
}

float AAnimaPreviewCharacter::GetCombatTime() const
{
    const AGameStateBase* State = GetWorld()->GetGameState();
    return State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

float AAnimaPreviewCharacter::GetJinxPassiveRemaining() const
{
    return CharacterChoice == 0 ? FMath::Max(0.f, JinxPassiveEnd - GetCombatTime()) : 0.f;
}

int32 AAnimaPreviewCharacter::GetJinxKillsUntilPassive() const
{
    return JinxKillsPerActivation - EnemyKills % JinxKillsPerActivation;
}

float AAnimaPreviewCharacter::GetJinxWeaponRemaining() const
{
    return FMath::Max(0.f, JinxWeaponReadyAt - GetCombatTime());
}

void AAnimaPreviewCharacter::RegisterEnemyKill()
{
    if (!HasAuthority())
    {
        return;
    }

    ++EnemyKills;
    if (CharacterChoice == 0 && EnemyKills % JinxKillsPerActivation == 0)
    {
        JinxPassiveEnd = GetCombatTime() + JinxPassiveDuration;
    }
}

bool AAnimaPreviewCharacter::UpgradeSignatureWeapon()
{
    if (!HasAuthority() || SignatureWeaponLevel >= 5)
    {
        return false;
    }
    ++SignatureWeaponLevel;
    return true;
}

void AAnimaPreviewCharacter::FireJinxWeapon(float Now)
{
    const float Cooldown = IsSignatureWeaponEvolved() ? 1.25f : 2.5f;

    JinxWeaponReadyAt = Now + GetCombatCooldown(Cooldown);
    const int32 BulletCount = SignatureWeaponLevel + 2 + GetCombatBonusProjectiles();
    const FVector Start = GetActorLocation() - FVector(0, 0, 25);
    for (int32 Bullet = 0; Bullet < BulletCount; ++Bullet)
    {
        const float Offset = (Bullet - (BulletCount - 1) * .5f) * 2.f;
        const FVector End = Start + FRotator(0, GetActorRotation().Yaw + Offset, 0).Vector() * 1500.f;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(JinxWeapon), false, this);
        FVector TracerEnd = End;
        bool bHitEnemy = false;
        FHitResult Hit;
        while (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Pawn, Query))
        {
            AActor* Target = Hit.GetActor();
            if (!IsValid(Target))
            {
                TracerEnd = Hit.ImpactPoint;
                break;
            }
            Query.AddIgnoredActor(Target);
            if (Target->IsA<AAnimaPreviewCharacter>())
            {
                continue;
            }
            if (!Target->ActorHasTag(TEXT("Enemy")))
            {
                TracerEnd = Hit.ImpactPoint;
                break;
            }
            UGameplayStatics::ApplyPointDamage(Target, (bHitEnemy ? 8.f : 20.f) + ConsumeSunlight(Target),
                (End - Start).GetSafeNormal(), Hit, GetController(), this, UDamageType::StaticClass());
            bHitEnemy = true;
            if (!IsSignatureWeaponEvolved())
            {
                TracerEnd = Hit.ImpactPoint;
                break;
            }
        }
        ShowJinxTracer(Start, TracerEnd);
    }
}

float AAnimaPreviewCharacter::GetBriarFrenzyRemaining() const
{
    return CharacterChoice == 4 && CombatHealth > 0.f && !bDistrictFinished
        ? FMath::Max(0.f, BriarFrenzyEnd - GetCombatTime()) : 0.f;
}

float AAnimaPreviewCharacter::GetCombatDamageMultiplier() const
{
    return PermanentDamageMultiplier * (1.f + .10f * (RunWeaponStats.IsValidIndex(0) ? RunWeaponStats[0] : 0));
}

float AAnimaPreviewCharacter::GetCombatCooldown(float BaseCooldown) const
{
    const float RunHaste = RunWeaponStats.IsValidIndex(1) ? 10.f * RunWeaponStats[1] : 0.f;
    const float Haste = PermanentAbilityHaste + RunHaste + (GetJinxPassiveRemaining() > 0.f ? 150.f : 0.f)
        + (GetXayahBuffRemaining() > 0.f ? 80.f : 0.f);
    return FMath::Max(0.f, BaseCooldown) / (1.f + FMath::Max(0.f, Haste) * .01f);
}

float AAnimaPreviewCharacter::GetCombatDurationMultiplier() const
{
    return PermanentDurationMultiplier + .12f * (RunWeaponStats.IsValidIndex(3) ? RunWeaponStats[3] : 0);
}

float AAnimaPreviewCharacter::GetCombatAreaMultiplier() const
{
    return PermanentAreaMultiplier + .11f * (RunWeaponStats.IsValidIndex(4) ? RunWeaponStats[4] : 0);
}

int32 AAnimaPreviewCharacter::GetCombatBonusProjectiles() const
{
    const int32 Rank = RunWeaponStats.IsValidIndex(2) ? FMath::Clamp<int32>(RunWeaponStats[2], 0, 5) : 0;
    return (Rank + 1) / 2;
}

float AAnimaPreviewCharacter::GetJinxSkillRemaining(bool bUltimate) const
{
    return FMath::Max(0.f, (bUltimate ? JinxRReadyAt : JinxEReadyAt) - GetCombatTime());
}

void AAnimaPreviewCharacter::ServerCastCharacterSkill_Implementation(bool bUltimate, FVector_NetQuantize Aim)
{
    if (CharacterChoice > 8 || !bDistrictCamera || bDistrictFinished || CombatHealth <= 0.f || Aim.ContainsNaN())
    {
        return;
    }
    const float Now = GetCombatTime();
    float& ReadyAt = bUltimate ? JinxRReadyAt : JinxEReadyAt;
    if (CharacterChoice == 4 && !bUltimate && BriarFrenzyEnd > Now)
    {
        FinishBriarFrenzy(Now);
        return;
    }
    if (CharacterChoice == 7 && bUltimate && AuroraRealmEnd > Now)
    {
        AuroraRealmEnd = 0.f;
        return;
    }
    if (Now < ReadyAt)
    {
        return;
    }

    PlayCombatMotion(bUltimate ? 2 : 1);
    if (CharacterChoice == 8)
    {
        CastXayahSkill(bUltimate, Now);
        return;
    }
    if (CharacterChoice == 7)
    {
        CastAuroraSkill(bUltimate, Now);
        return;
    }
    if (CharacterChoice == 1)
    {
        CastSeraphineSkill(bUltimate, Now);
        return;
    }

    if (CharacterChoice == 2)
    {
        CastLeonaSkill(bUltimate, Now, Aim);
        return;
    }

    if (CharacterChoice == 6)
    {
        CastRivenSkill(bUltimate, Now);
        return;
    }

    if (CharacterChoice >= 3)
    {
        CastAdditionalSkill(bUltimate, Now, Aim);
        return;
    }

    const FVector Start = GetActorLocation() - FVector(0, 0, 25);
    const FVector Offset(Aim.X - Start.X, Aim.Y - Start.Y, 0);
    const float Reach = bUltimate ? 3200.f : 1500.f;
    if (Offset.IsNearlyZero(1.f))
    {
        return;
    }

    ReadyAt = Now + GetCombatCooldown(bUltimate ? 50.f : 8.f);
    if (bUltimate)
    {
        JinxRockets.Add({Start, Start + Offset.GetClampedToMaxSize(Reach), true});
        return;
    }
    for (int32 Rocket = -2; Rocket <= 2; ++Rocket)
    {
        const FVector Direction = Offset.GetSafeNormal().RotateAngleAxis(Rocket * 7.f, FVector::UpVector);
        JinxRockets.Add({Start, Start + Direction * Reach, false});
    }
}

void AAnimaPreviewCharacter::TickJinxRockets(float DeltaTime)
{
    for (int32 Index = JinxRockets.Num() - 1; Index >= 0; --Index)
    {
        FJinxRocket& Rocket = JinxRockets[Index];
        const FVector Next = FMath::VInterpConstantTo(Rocket.Position, Rocket.Destination, DeltaTime,
            Rocket.bUltimate ? 2300.f : 1750.f);
        FHitResult Hit;
        if (!Rocket.bUltimate)
        {
            FCollisionQueryParams Query(SCENE_QUERY_STAT(JinxRocket), false, this);
            while (GetWorld()->LineTraceSingleByChannel(Hit, Rocket.Position, Next, ECC_Pawn, Query))
            {
                AActor* Target = Hit.GetActor();
                if (!Target || !Target->IsA<AAnimaPreviewCharacter>())
                {
                    break;
                }
                Query.AddIgnoredActor(Target);
                Hit = FHitResult();
            }
        }
        const FVector End = Hit.bBlockingHit ? Hit.ImpactPoint : Next;
        if (GetCombatTime() >= Rocket.NextVisualAt || Hit.bBlockingHit || End.Equals(Rocket.Destination, 1.f))
        {
            const FVector TrailStart = Rocket.NextVisualAt > 0.f ? Rocket.LastVisualPosition : Rocket.Position;
            ShowJinxTracer(TrailStart, End, Rocket.bUltimate ? 2 : 0);
            Rocket.LastVisualPosition = End;
            Rocket.NextVisualAt = GetCombatTime() + .08f;
        }
        Rocket.Position = End;
        if (Hit.bBlockingHit || End.Equals(Rocket.Destination, 1.f))
        {
            ExplodeJinxRocket(End, Rocket.bUltimate, Hit.GetActor());
            JinxRockets.RemoveAtSwap(Index);
        }
    }
}

void AAnimaPreviewCharacter::ExplodeJinxRocket(FVector Position, bool bUltimate, AActor* DirectTarget)
{
    const float Radius = bUltimate ? JinxRRadius : JinxERadius;
    const float Damage = bUltimate ? JinxRDamage : JinxEDamage;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(JinxExplosion), false, this);
    TArray<FOverlapResult> Hits;
    GetWorld()->OverlapMultiByObjectType(Hits, Position, FQuat::Identity,
        FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius), Query);
    TSet<AActor*> Targets;
    if (IsValid(DirectTarget))
    {
        Targets.Add(DirectTarget);
    }
    for (const FOverlapResult& Hit : Hits)
    {
        Targets.Add(Hit.GetActor());
    }
    for (AActor* Target : Targets)
    {
        if (!IsValid(Target) || Target->IsA<AAnimaPreviewCharacter>() || !Target->ActorHasTag(TEXT("Enemy")))
        {
            continue;
        }
        const float MissingHealth = bUltimate && Target->Implements<UCombatHealth>()
            ? FMath::Clamp(ICombatHealth::Execute_GetMissingHealthFraction(Target), 0.f, 1.f) : 0.f;
        UGameplayStatics::ApplyDamage(Target, Damage * (1.f + 2.f * MissingHealth) + ConsumeSunlight(Target), GetController(),
            this, UDamageType::StaticClass());
    }
    ShowJinxExplosion(Position, Radius, bUltimate);
}

float AAnimaPreviewCharacter::TakeDamage(float Damage, const FDamageEvent& Event,
    AController* InstigatorController, AActor* Causer)
{
    if (!HasAuthority() || Damage <= 0.f || CombatHealth <= 0.f || GetCombatTime() < InvulnerableEnd)
    {
        return 0.f;
    }
    if (CharacterChoice == 4 && GetCombatTime() < BriarFrenzyEnd) { Damage *= .75f; }
    Damage *= 100.f / (100.f + GetCombatArmor());
    if (CharacterChoice == 2)
    {
        const float Now = GetCombatTime();
        if (Now < LeonaEclipseEnd)
        {
            const float Absorbed = FMath::Min(Damage, LeonaShield);
            LeonaShield -= Absorbed;
            Damage -= Absorbed;
        }
    }
    if (GetCombatTime() < SeraphineShieldEnd)
    {
        const float Absorbed = FMath::Min(Damage, SeraphineShield);
        SeraphineShield -= Absorbed;
        Damage -= Absorbed;
    }
    if (GetCombatTime() < RivenShieldEnd)
    {
        const float Absorbed = FMath::Min(Damage, RivenShield);
        Damage -= Absorbed;
        RivenShield -= Absorbed;
    }
    const float Now = GetCombatTime();
    const uint8 IceArmor = static_cast<uint8>(EGeneralWeapon::IceArmor);
    if (IceShieldEnd > Now && IceShieldRemaining > 0.f)
    {
        const float Absorbed = FMath::Min(Damage, IceShieldRemaining);
        IceShieldRemaining -= Absorbed;
        Damage -= Absorbed;
        if (IceShieldRemaining <= 0.f) { TriggerIceBurst(Now); }
    }
    else if (Damage > 0.f && GetGeneralWeaponRank(IceArmor) > 0 && !IsGeneralWeaponEvolved(IceArmor)
        && Now >= GeneralWeaponReadyAt[IceArmor])
    {
        Damage = 0.f;
        TriggerIceBurst(Now);
    }
    const float Applied = FMath::Min(CombatHealth, Damage);
    CombatHealth -= Applied;
    if (CombatHealth <= 0.f && bDistrictCamera)
    {
        if (auto* Mode = GetWorld()->GetAuthGameMode<AAnimaPreviewMode>()) { Mode->CheckDistrictDefeat(); }
    }
    return Applied > 0.f ? Super::TakeDamage(Applied, Event, InstigatorController, Causer) : 0.f;
}

void AAnimaPreviewCharacter::CastSeraphineSkill(bool bUltimate, float Now)
{
    const float Radius = bUltimate ? 1050.f : 575.f;
    float& ReadyAt = bUltimate ? JinxRReadyAt : JinxEReadyAt;
    ReadyAt = Now + GetCombatCooldown(bUltimate ? 90.f : 16.f);
    const FVector Origin = GetActorLocation();
    for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
    {
        if (!It->GetController() || It->CombatHealth <= 0.f || FVector::DistSquared2D(Origin, It->GetActorLocation()) > FMath::Square(Radius))
        {
            continue;
        }
        It->ShowSeraphineEffect(It->GetActorLocation(), 100.f, false);
        if (bUltimate)
        {
            It->InvulnerableEnd = FMath::Max(It->InvulnerableEnd, Now + 3.f);
        }
        else
        {
            It->SeraphineShield = FMath::Max(Now < It->SeraphineShieldEnd ? It->SeraphineShield : 0.f,
                100.f + .25f * It->MaxCombatHealth);
            It->SeraphineShieldEnd = Now + 5.f;
        }
    }
    if (bUltimate)
    {
        TArray<FOverlapResult> Hits;
        GetWorld()->OverlapMultiByObjectType(Hits, Origin, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
            FCollisionShape::MakeSphere(Radius));
        TSet<AActor*> Affected;
        for (const FOverlapResult& Hit : Hits)
        {
            AActor* Target = Hit.GetActor();
            if (!IsValid(Target) || !Target->ActorHasTag(TEXT("Enemy")) || Affected.Contains(Target))
            {
                continue;
            }
            Affected.Add(Target);
            UGameplayStatics::ApplyDamage(Target, 160.f + ConsumeSunlight(Target), GetController(), this, UDamageType::StaticClass());
            if (ACharacter* Enemy = Cast<ACharacter>(Target); IsValid(Enemy))
            {
                Enemy->GetCharacterMovement()->StopMovementImmediately();
                Enemy->LaunchCharacter((Enemy->GetActorLocation() - Origin).GetSafeNormal2D() * 500.f, true, true);
            }
        }
    }
    ShowSeraphineEffect(Origin, Radius, bUltimate);
}

void AAnimaPreviewCharacter::FireSeraphineWeapon(float Now)
{
    JinxWeaponReadyAt = Now + GetCombatCooldown(3.f - .25f * (SignatureWeaponLevel - 1));
    const bool bEvolved = IsSignatureWeaponEvolved();
    const int32 Count = (bEvolved ? 6 : SignatureWeaponLevel) + GetCombatBonusProjectiles();
    const float Damage = bEvolved ? 190.f : 70.f + 20.f * SignatureWeaponLevel;
    const FVector Origin = GetActorLocation() - FVector(0, 0, 25);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const float Angle = GetActorRotation().Yaw + (Index - (Count - 1) * .5f) * 8.f;
        const FVector Direction = FRotator(0, Angle, 0).Vector();
        SeraphineWaves.Add({Origin, Origin + Direction * 900.f, Direction, {}, bEvolved, false, Damage});
        if (FMath::FRand() < .25f)
        {
            const FVector EchoOffset = FVector::CrossProduct(FVector::UpVector, Direction) * 24.f;
            SeraphineWaves.Add({Origin + EchoOffset, Origin + EchoOffset + Direction * 900.f,
                Direction, {}, bEvolved, false, Damage});
        }
    }
}

void AAnimaPreviewCharacter::TickSeraphineWaves(float DeltaTime)
{
    for (int32 Index = SeraphineWaves.Num() - 1; Index >= 0; --Index)
    {
        FSeraphineWave& Wave = SeraphineWaves[Index];
        const FVector Next = FMath::VInterpConstantTo(Wave.Position, Wave.Destination, DeltaTime, 1500.f);
        TArray<FHitResult> Hits;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(SeraphineWave), false, this);
        GetWorld()->SweepMultiByObjectType(Hits, Wave.Position, Next, FQuat::Identity,
            FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(43.f), Query);
        for (const FHitResult& Hit : Hits)
        {
            AActor* Target = Hit.GetActor();
            if (!IsValid(Target) || !Target->ActorHasTag(TEXT("Enemy")) || Wave.HitEnemies.Contains(Target))
            {
                continue;
            }
            Wave.HitEnemies.Add(Target);
            if (ACharacter* Enemy = Cast<ACharacter>(Target))
            {
                UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement();
                static const FName SlowTag(TEXT("SeraphineWaveSlow"));
                if (!Movement->ComponentHasTag(SlowTag))
                {
                    const float PreviousSpeed = Movement->MaxWalkSpeed;
                    const float Slowed = PreviousSpeed * .65f;
                    Movement->ComponentTags.Add(SlowTag);
                    Movement->MaxWalkSpeed = Slowed;
                    FTimerHandle SlowTimer;
                    GetWorldTimerManager().SetTimer(SlowTimer, FTimerDelegate::CreateWeakLambda(Movement,
                        [Movement, PreviousSpeed, Slowed]()
                        {
                            Movement->ComponentTags.Remove(SlowTag);
                            if (FMath::IsNearlyEqual(Movement->MaxWalkSpeed, Slowed))
                            {
                                Movement->MaxWalkSpeed = PreviousSpeed;
                            }
                        }), .75f, false);
                }
            }
            UGameplayStatics::ApplyDamage(Target, Wave.Damage + ConsumeSunlight(Target), GetController(), this, UDamageType::StaticClass());
        }
        if (GetCombatTime() >= Wave.NextVisualAt || Next.Equals(Wave.Destination, 1.f))
        {
            const FVector TrailStart = Wave.NextVisualAt > 0.f ? Wave.LastVisualPosition : Wave.Position;
            ShowJinxTracer(TrailStart, Next, Wave.bReturning ? 1 : 0);
            Wave.LastVisualPosition = Next;
            Wave.NextVisualAt = GetCombatTime() + .10f;
        }
        Wave.Position = Next;
        if (Next.Equals(Wave.Destination, 1.f))
        {
            if (Wave.bEvolved && !Wave.bReturning)
            {
                Wave.bReturning = true;
                Wave.Destination = Next - Wave.Direction * 2000.f;
                Wave.HitEnemies.Reset();
            }
            else
            {
                SeraphineWaves.RemoveAtSwap(Index);
            }
        }
    }
}

float AAnimaPreviewCharacter::GetCombatArmor() const
{
    return CharacterChoice == 2 ? GetLeonaArmor(GetCombatTime()) : BaseArmor + RunArmorBonus;
}

float AAnimaPreviewCharacter::GetLeonaArmor(float Now) const
{
    return FMath::Max(0.f, BaseArmor + LeonaBonusArmor + RunArmorBonus) * (Now < LeonaEclipseEnd ? 2.f : 1.f);
}

float AAnimaPreviewCharacter::GetLeonaAreaMultiplier() const
{
    const float BonusPercent = .30f * (LeonaBonusArmor + RunArmorBonus) + .75f * (LeonaBonusRegen + RunRegen)
        + .02f * (LeonaHealthFromStatPassives + RunHealthBonus);
    return GetCombatAreaMultiplier() + .01f * FMath::Max(0.f, BonusPercent);
}

float AAnimaPreviewCharacter::ConsumeSunlight(AActor* Target) const
{
    return Target->Tags.Remove(SunlightTag) > 0 ? 300.f : 0.f;
}

void AAnimaPreviewCharacter::FireLeonaWeapon(float Now)
{
    const bool bEvolved = IsSignatureWeaponEvolved();
    JinxWeaponReadyAt = Now + GetCombatCooldown(bEvolved ? 1.5f : 2.75f - .25f * (SignatureWeaponLevel - 1));
    const float Radius = (bEvolved ? 850.f : 550.f) * GetLeonaAreaMultiplier();
    const float Damage = (bEvolved ? 175.f : 25.f + 25.f * SignatureWeaponLevel) + 3.f * GetLeonaArmor(Now);
    DamageLeonaArea(GetActorLocation(), Radius, Damage, false, true);
    ShowLeonaShieldStrike(GetActorLocation(), GetActorForwardVector(), Radius, bEvolved);
}

void AAnimaPreviewCharacter::CastLeonaSkill(bool bUltimate, float Now, FVector Aim)
{
    if (bUltimate)
    {
        JinxRReadyAt = Now + GetCombatCooldown(80.f);
        Aim.Z = GetActorLocation().Z;
        const float Radius = 520.f * GetLeonaAreaMultiplier();
        DamageLeonaArea(Aim, Radius, 160.f, true, false);
        LeonaSolarZoneCenter = Aim;
        LeonaSolarZoneRadius = Radius;
        LeonaSolarZoneEnd = Now + 4.f;
        LeonaZones.Add({Aim, Radius, Now + .5f, LeonaSolarZoneEnd, false});
        ForceNetUpdate();
        ShowSeraphineEffect(Aim, Radius, true);
        return;
    }

    JinxEReadyAt = Now + GetCombatCooldown(17.f);
    LeonaEclipseEnd = Now + 5.f;
    LeonaShield = .25f * MaxCombatHealth;
    const float Radius = 350.f * GetLeonaAreaMultiplier();
    LeonaEclipseZoneEnd = Now + 3.f;
    LeonaEclipseZoneRadius = Radius;
    LeonaZones.Add({GetActorLocation(), Radius, Now + .5f, LeonaEclipseZoneEnd, true});
    ForceNetUpdate();
    ShowSeraphineEffect(GetActorLocation(), Radius, false);
}

void AAnimaPreviewCharacter::TickLeonaZones(float Now)
{
    for (int32 Index = LeonaZones.Num() - 1; Index >= 0; --Index)
    {
        FLeonaZone& Zone = LeonaZones[Index];
        const FVector Center = Zone.bFollowsLeona ? GetActorLocation() : Zone.Position;
        if (Now >= Zone.EndsAt)
        {
            if (Zone.bFollowsLeona)
            {
                DamageLeonaArea(Center, Zone.Radius, 110.f, false, false);
                ShowSeraphineEffect(Center, Zone.Radius, true);
            }
            LeonaZones.RemoveAtSwap(Index);
        }
        else if (Now >= Zone.NextTick)
        {
            DamageLeonaArea(Center, Zone.Radius, 20.f, false, false);
            Zone.NextTick = Now + .5f;
        }
    }
}

void AAnimaPreviewCharacter::DamageLeonaArea(FVector Position, float Radius, float Damage, bool bStun, bool bShieldStrike)
{
    TArray<FOverlapResult> Hits;
    GetWorld()->OverlapMultiByObjectType(Hits, Position, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
        FCollisionShape::MakeSphere(Radius));
    TSet<AActor*> Affected;
    for (const FOverlapResult& Hit : Hits)
    {
        AActor* Target = Hit.GetActor();
        if (!IsValid(Target) || !Target->ActorHasTag(TEXT("Enemy")) || Affected.Contains(Target))
        {
            continue;
        }
        if (bShieldStrike && !IsSignatureWeaponEvolved() &&
            FVector::DotProduct(GetActorForwardVector(), (Target->GetActorLocation() - Position).GetSafeNormal2D()) < FMath::Cos(FMath::DegreesToRadians(LeonaShieldHalfAngle)))
        {
            continue;
        }
        Affected.Add(Target);
        UGameplayStatics::ApplyDamage(Target, Damage + (bShieldStrike ? 0.f : ConsumeSunlight(Target)),
            GetController(), this, UDamageType::StaticClass());
        if (!IsValid(Target))
        {
            continue;
        }
        if (bShieldStrike && IsSignatureWeaponEvolved())
        {
            Target->Tags.AddUnique(SunlightTag);
        }
        if (bStun)
        {
            if (ACharacter* Enemy = Cast<ACharacter>(Target))
            {
                UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement();
                const EMovementMode Previous = Movement->MovementMode;
                const uint8 Custom = Movement->CustomMovementMode;
                if (Previous != MOVE_None)
                {
                    Movement->StopMovementImmediately();
                    Movement->DisableMovement();
                    FTimerHandle StunTimer;
                    GetWorldTimerManager().SetTimer(StunTimer, FTimerDelegate::CreateWeakLambda(Movement,
                        [Movement, Previous, Custom]()
                        {
                            if (Movement->MovementMode == MOVE_None)
                            {
                                Movement->SetMovementMode(Previous, Custom);
                            }
                        }), 1.25f, false);
                }
            }
        }
    }
}

void AAnimaPreviewCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
    if (CombatVfx) { CombatVfx->Destroy(); CombatVfx = nullptr; }
    EndLevelChoiceFreeze();
    if (CombatMusic) { CombatMusic->Stop(); }
    Super::EndPlay(Reason);
}

void AAnimaPreviewCharacter::ReturnToSoloMenu()
{
    if (IsLocallyControlled())
    {
        EndLevelChoiceFreeze();
        UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/TroubleSquad/Maps/L_SoloMenu")));
    }
}

float AAnimaPreviewCharacter::GetLevelChoiceClock() const
{
    return GetWorld()->GetRealTimeSeconds();
}

void AAnimaPreviewCharacter::OnRep_LevelChoiceDeadline()
{
    LocalLevelChoiceDeadline = LevelChoiceDeadline > 0.f ? GetLevelChoiceClock() + 30.f : 0.f;
}

void AAnimaPreviewCharacter::BeginLevelChoiceFreeze()
{
    if (bDistrictCamera)
    {
        if (auto* Mode = GetWorld()->GetAuthGameMode<AAnimaPreviewMode>())
        {
            Mode->SetLevelChoicePending(this, true);
        }
    }
}

void AAnimaPreviewCharacter::EndLevelChoiceFreeze()
{
    if (auto* Mode = GetWorld()->GetAuthGameMode<AAnimaPreviewMode>())
    {
        Mode->SetLevelChoicePending(this, false);
    }
}

float AAnimaPreviewCharacter::GetLevelChoiceTimeRemaining() const
{
    const float Deadline = HasAuthority() ? LevelChoiceDeadline : LocalLevelChoiceDeadline;
    return FMath::Max(0.f, Deadline - GetLevelChoiceClock());
}

void AAnimaPreviewCharacter::Tick(float DeltaTime)
{
    if(HasAuthority() && (CombatHealth<=0.f || bDistrictFinished)) { GeneralProjectiles.Reset(); PendingCombatVfx.Reset(); }
    UpdateTentacleMeshes();
    UpdateGeneralSummonMeshes();
    UpdateWindWallVisual();
    UpdateXayahFeatherMeshes();
    if (HasAuthority() && !LevelChoices.IsEmpty() && CombatHealth <= 0.f)
    {
        LevelChoices.Reset();
        LevelChoiceDeadline = 0.f;
        EndLevelChoiceFreeze();
    }
    if (HasAuthority() && !LevelChoices.IsEmpty() && LevelChoiceDeadline > 0.f && GetLevelChoiceTimeRemaining() <= 0.f)
    {
        ServerSelectLevelChoice_Implementation(FMath::RandRange(0, LevelChoices.Num() - 1), LevelChoiceSequence);
    }
    if (bDistrictFinished)
    {
        if (IsLocallyControlled() && !bResultAudioHandled)
        {
            bResultAudioHandled = true;
            if (CombatMusic) { CombatMusic->FadeOut(.65f, 0.f); }
            if (bDistrictCleared) { CombatAudio::PlayLocal(GetWorld(), TEXT("UI/Evolution"), .48f); }
        }
        Super::Tick(DeltaTime);
        if (auto* Player = Cast<APlayerController>(GetController());
            IsLocallyControlled() && Player && Player->WasInputKeyJustPressed(EKeys::Escape))
        {
            ReturnToSoloMenu();
        }
        return;
    }
    if (bDistrictCamera && IsLocallyControlled() && GetNetMode() != NM_DedicatedServer && !CombatMusic)
    {
        if (USoundWave* Music = CombatAudio::Load(TEXT("Music/Combat_Loop")))
        {
            Music->bLooping = true;
            CombatMusic = UGameplayStatics::SpawnSound2D(this, Music, .20f * CombatAudio::GetMusicVolume(), 1.f, 0.f, nullptr, false, false);
        }
    }
    if (CombatMusic) { CombatMusic->SetVolumeMultiplier(.20f * CombatAudio::GetMusicVolume()); }
    Super::Tick(DeltaTime);
    UpdateSkillVfx();
    if (HasAuthority() && RunRegen > 0.f && CombatHealth > 0.f && CombatHealth < MaxCombatHealth)
    {
        CombatHealth = FMath::Min(MaxCombatHealth, CombatHealth + RunRegen * DeltaTime);
    }
    if (HasAuthority() && !JinxRockets.IsEmpty())
    {
        TickJinxRockets(DeltaTime);
    }

    if (HasAuthority() && !SeraphineWaves.IsEmpty()) { TickSeraphineWaves(DeltaTime); }
    if (HasAuthority() && !LeonaZones.IsEmpty()) { TickLeonaZones(GetCombatTime()); }
    if (HasAuthority() && CharacterChoice >= 3 && CharacterChoice <= 5) { TickAdditionalCombat(GetCombatTime(), DeltaTime); }
    if (HasAuthority() && CharacterChoice == 6) { TickRivenCombat(GetCombatTime(), DeltaTime); }
    if (HasAuthority() && CharacterChoice == 7 && bDistrictCamera) { TickAuroraCombat(GetCombatTime(), DeltaTime); }
    if (HasAuthority() && CharacterChoice == 8 && bDistrictCamera) { TickXayahFeathers(GetCombatTime(), DeltaTime); }
    if (HasAuthority() && bDistrictCamera && GetController() && CombatHealth > 0.f) { TickGeneralWeapons(GetCombatTime(), DeltaTime); }
    const float PassiveFraction = GetJinxPassiveRemaining() / JinxPassiveDuration;
    const float RivenSpeedBonus = CharacterChoice == 6 ? .25f * RivenCharge / 100.f + (GetCombatTime() < RivenUltimateEnd ? .30f : 0.f) : 0.f;
    GetCharacterMovement()->MaxWalkSpeed = BaseMoveSpeed * (1.f + (GetCombatTime() < LabSprintEnd ? .85f : 0.f) + RunMoveBonus + (GetCombatTime() < TrailBoostEnd ? .025f * TrailBoostStacks : 0.f) + (GetCombatTime() < AuroraVeilEnd ? .35f : 0.f) + RivenSpeedBonus + JinxInitialSpeedBonus * PassiveFraction + (GetCombatTime() < SeraphineShieldEnd ? .20f : 0.f) + (CharacterChoice == 4 ? 1.f - CombatHealth / FMath::Max(MaxCombatHealth, 1.f) : 0.f));
    if (HasAuthority() && bDistrictCamera && CharacterChoice != 6 && GetController())
    {
        const float Now = GetCombatTime();
        if (Now >= JinxWeaponReadyAt)
        {
            if (CharacterChoice < 5 || YasuoFlow >= 100.f) { PlayCombatMotion(0); }
            if (CharacterChoice == 8) { FireXayahWeapon(Now); PlayCombatMotion(0); }
            else if (CharacterChoice == 7) { FireAuroraWeapon(Now); PlayCombatMotion(0); }
            else if (CharacterChoice == 0) { FireJinxWeapon(Now); }
            else if (CharacterChoice == 1) { FireSeraphineWeapon(Now); }
            else if (CharacterChoice == 2) { FireLeonaWeapon(Now); }
            else { FireAdditionalWeapon(Now); }
        }
    }

    FlushCombatVfx(GetCombatTime());
    const bool bMoving = GetVelocity().SizeSquared2D() > 25.f;
    if (bMoving != bWasMoving)
    {
        GetMesh()->PlayAnimation(bMoving ? Run.Get() : Idle.Get(), true);
        bWasMoving = bMoving;
    }

    UpdateCombatMotion(DeltaTime, bMoving);

    const APlayerController* Player = Cast<APlayerController>(GetController());
    if (!Player || !Player->IsLocalController())
    {
        return;
    }

    if (bDistrictCamera)
    {
        const int32 ZoomSteps = static_cast<int32>(Player->WasInputKeyJustPressed(EKeys::MouseScrollDown))
            - static_cast<int32>(Player->WasInputKeyJustPressed(EKeys::MouseScrollUp));
        DesiredCameraDistance = FMath::Clamp(DesiredCameraDistance + ZoomSteps * DistrictZoomStep,
            DistrictClosestZoom, DistrictDefaultZoom);
        CameraArm->TargetArmLength = FMath::FInterpTo(CameraArm->TargetArmLength, DesiredCameraDistance,
            DeltaTime, 12.f);
    }

    if (Player->WasInputKeyJustPressed(EKeys::Escape)
        && SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this)) != INDEX_NONE)
    {
        ReturnToSoloMenu();
        return;
    }

    if (!LevelChoices.IsEmpty())
    {
        for (int32 Slot = 0; Slot < LevelChoices.Num(); ++Slot)
        {
            if (Player->WasInputKeyJustPressed(CharacterKeys[Slot]))
            {
                SelectLevelCard(Slot);
                break;
            }
        }
    }
    else if (!bDistrictCamera)
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(CharacterKeys); ++Index)
    {
        if (Player->WasInputKeyJustPressed(CharacterKeys[Index]))
        {
            if (HasAuthority())
            {
                ApplyCharacterChoice(Index);
            }
            else
            {
                ServerChooseCharacter(Index);
            }
        }
    }

    const FVector Direction(
        static_cast<int32>(Player->IsInputKeyDown(EKeys::W)) - static_cast<int32>(Player->IsInputKeyDown(EKeys::S)),
        static_cast<int32>(Player->IsInputKeyDown(EKeys::D)) - static_cast<int32>(Player->IsInputKeyDown(EKeys::A)),
        0.f);
    const bool bFrenzied = CharacterChoice == 4 && GetCombatTime() < BriarFrenzyEnd;
    if (!bFrenzied && !bRivenDashing && !Direction.IsNearlyZero())
    {
        const FRotator ViewYaw(0.f, CameraArm->GetComponentRotation().Yaw, 0.f);
        AddMovementInput(ViewYaw.RotateVector(Direction.GetSafeNormal()));
    }
    if (CharacterChoice == 6 && bDistrictCamera && Player->WasInputKeyJustPressed(EKeys::E)
        && GetJinxSkillRemaining(false) <= 0.f)
    {
        ServerCastCharacterSkill(false, GetActorLocation());
    }
    if (CharacterChoice == 7 && Player->WasInputKeyJustPressed(EKeys::C)) { ServerToggleAuroraAutoAim(); }
    FVector CursorRayOrigin;
    FVector CursorRayDirection;
    if (Player->DeprojectMousePositionToWorld(CursorRayOrigin, CursorRayDirection)
        && !FMath::IsNearlyZero(CursorRayDirection.Z))
    {
        const FVector Position = GetActorLocation();
        const float GroundZ = Position.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const float Distance = (GroundZ - CursorRayOrigin.Z) / CursorRayDirection.Z;
        const FVector Aim = CursorRayOrigin + CursorRayDirection * Distance - Position;
        if (Distance >= 0.f)
        {
            const FVector FlatAim(Aim.X, Aim.Y, 0.f);
            if (!FlatAim.IsNearlyZero(1.f))
            {
                const FRotator AimRotation = FlatAim.Rotation();
                if (!bFrenzied && !bRivenDashing) { SetActorRotation(AimRotation); }
                if (CharacterChoice == 7 && HasAuthority()) { AuroraAimPoint = CursorRayOrigin + CursorRayDirection * Distance; }
                if (CharacterChoice != 6 && bDistrictCamera)
                {
                    const FVector AimPoint = CursorRayOrigin + CursorRayDirection * Distance;
                    if (Player->WasInputKeyJustPressed(EKeys::E) && (bFrenzied || GetJinxSkillRemaining(false) <= 0.f))
                    {
                        ServerCastCharacterSkill(false, AimPoint);
                    }
                    if (Player->WasInputKeyJustPressed(EKeys::R) && (CharacterChoice == 7 || GetJinxSkillRemaining(true) <= 0.f))
                    {
                        ServerCastCharacterSkill(true, AimPoint);
                    }
                }
                if (CharacterChoice == 6 && bDistrictCamera && Player->WasInputKeyJustPressed(EKeys::R)
                    && GetJinxSkillRemaining(true) <= 0.f)
                {
                    ServerCastCharacterSkill(true, GetActorLocation());
                }
                const float Time = GetWorld()->GetTimeSeconds();
                if (!bFrenzied && !bRivenDashing && !HasAuthority() && Time - LastAimSendTime >= 0.05f)
                {
                    LastAimSendTime = Time;
                    ServerUpdateAim(AimRotation.Yaw, CursorRayOrigin + CursorRayDirection * Distance);
                }
            }
        }
    }


}

AAnimaPreviewMode::AAnimaPreviewMode()
{
    DefaultPawnClass = AAnimaPreviewCharacter::StaticClass();
    HUDClass = ACombatHUD::StaticClass();
}

int32 AAnimaPreviewMode::GetCharacterChoice(const AController* Controller) const
{
    if (const int32* Selection = CharacterChoices.Find(Controller))
    {
        return *Selection;
    }
    return FMath::Clamp(UGameplayStatics::GetIntOption(OptionsString, TEXT("Character"), 0), 0,
        static_cast<int32>(UE_ARRAY_COUNT(Roster)) - 1);
}

FString AAnimaPreviewMode::InitNewPlayer(APlayerController* NewPlayerController,
    const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
    const FString Error = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
    if (Error.IsEmpty())
    {
        const int32 Chosen = UGameplayStatics::GetIntOption(Options, TEXT("Character"), GetCharacterChoice(nullptr));
        CharacterChoices.Add(NewPlayerController, FMath::Clamp(Chosen, 0, static_cast<int32>(UE_ARRAY_COUNT(Roster)) - 1));
        UE_LOG(LogTemp, Display, TEXT("District player joined (Players=%d, Character=%d)"),
            GetNumPlayers(), GetCharacterChoice(NewPlayerController));
    }
    return Error;
}

void AAnimaPreviewMode::PreLogin(const FString& Options, const FString& Address,
    const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    const int32 Capacity = FMath::Clamp(UGameplayStatics::GetIntOption(OptionsString, TEXT("Players"), 1), 1, 4);
    if (ErrorMessage.IsEmpty() && GetNumPlayers() >= Capacity)
    {
        ErrorMessage = TEXT("Room is full.");
    }
}

void AAnimaPreviewMode::Logout(AController* Exiting)
{
    if (auto* Character = Cast<AAnimaPreviewCharacter>(Exiting->GetPawn()))
    {
        SetLevelChoicePending(Character, false);
    }
    CharacterChoices.Remove(Exiting);
    Super::Logout(Exiting);
}

#if WITH_EDITOR
void AAnimaPreviewCharacter::RunSkillVisualSmokeTest(int32 Index)
{
    if (!HasAuthority()) { return; }
    ApplyCharacterChoice(FMath::Clamp(Index, 0, 8));
    const FVector Aim = GetActorLocation() + FVector(260.f, 180.f, -55.f);
    if(FParse::Param(FCommandLine::Get(),TEXT("NiagaraVfxReview"))) { ReviewNiagaraAssets(); return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("WeaponSummonReview")))
    {
        InvulnerableEnd=GetCombatTime()+30.f; JinxWeaponReadyAt=GetCombatTime()+30.f;
        RunWeaponStats.Init(0,8);
        GeneralWeapons.Reset(); WeaponZones.Reset(); GeneralWeaponRanks.Init(5,GeneralWeaponCount);
        const uint8 Bear=static_cast<uint8>(EGeneralWeapon::Tibber),Cat=static_cast<uint8>(EGeneralWeapon::Yuumi);
        EvolvedGeneralWeapons={Bear,Cat};
        FireGeneralWeapon(Bear,GetCombatTime()); FireGeneralWeapon(Cat,GetCombatTime());
        for(int32 I=0;I<WeaponZones.Num();++I)
        {
            WeaponZones[I].Position=GetActorLocation()+FVector(0,I==0?-180.f:180.f,0);
            WeaponZones[I].EndAt=GetCombatTime()+1.8f;
        }
        FTimerHandle Shot,Expire;
        GetWorldTimerManager().SetTimer(Shot,FTimerDelegate::CreateWeakLambda(this,[this]()
        {
            UE_LOG(LogTemp,Display,TEXT("Summon review: visuals=%d meshes=%d"),GeneralSummonVisuals.Num(),GeneralSummonMeshes.Num());
            if(auto* P=Cast<APlayerController>(GetController())) { P->ConsoleCommand(TEXT("Shot ShowUI"),true); }
        }),.7f,false);
        GetWorldTimerManager().SetTimer(Expire,FTimerDelegate::CreateWeakLambda(this,[this]()
        { UE_LOG(LogTemp,Display,TEXT("Summon expiry: visuals=%d meshes=%d"),GeneralSummonVisuals.Num(),GeneralSummonMeshes.Num()); }),2.2f,false);
        return;
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("WeaponCardReview")))
    {
        GeneralWeaponRanks.Init(0,GeneralWeaponCount); RunWeaponStats.Init(0,8); RunWeaponStats[2]=3;
        const uint8 Launcher=static_cast<uint8>(EGeneralWeapon::Launcher),Boomerang=static_cast<uint8>(EGeneralWeapon::Boomerang);
        GeneralWeaponRanks[Launcher]=2; GeneralWeaponRanks[Boomerang]=4; GeneralWeapons={Launcher,Boomerang};
        PendingLevelChoices=1;
        LevelChoices={uint8(FirstGeneralWeaponChoice+Launcher),uint8(FirstGeneralWeaponChoice+static_cast<uint8>(EGeneralWeapon::Crossbow)),uint8(FirstGeneralWeaponChoice+Boomerang)};
        LevelChoiceDeadline=GetLevelChoiceClock()+30.f; InvulnerableEnd=GetCombatTime()+30.f;
        FTimerHandle Shot;
        GetWorldTimerManager().SetTimer(Shot,FTimerDelegate::CreateWeakLambda(this,[this]()
        { if(auto* P=Cast<APlayerController>(GetController())) { P->ConsoleCommand(TEXT("Shot ShowUI"),true); } }),.3f,false);
        UE_LOG(LogTemp,Display,TEXT("Weapon card review: upgrade/new/upgrade projectilesBonus=2"));
        return;
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("VfxBatchNetworkTest")))
    {
        InvulnerableEnd=GetCombatTime()+120.f; JinxWeaponReadyAt=GetCombatTime()+120.f;
        FTimerHandle Send;
        GetWorldTimerManager().SetTimer(Send,FTimerDelegate::CreateWeakLambda(this,[this]()
        {
            for(int32 I=0;I<40;++I)
            {
                FCombatVfxEvent E; E.Kind=0; E.Weapon=I%GeneralWeaponCount; E.ReviewToken=I+1;
                E.Start=GetActorLocation()+FVector((I%5-2)*120.f,(I/5-3.5f)*100.f,0);
                E.End=FVector(E.Start)+FVector(90,0,0); E.Radius=60.f; E.Variant=I%2; E.bEnhanced=I>=20;
                QueueCombatVfx(E);
            }
            UE_LOG(LogTemp,Display,TEXT("VFX network review queued=40"));
        }),12.f,false);
        return;
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmGeneralProjectileTest")))
    {
        ApplyCharacterChoice(0); RunWeaponStats.Init(0,8); GeneralWeaponRanks.Init(1,GeneralWeaponCount);
        GeneralProjectiles.Reset();
        for(TActorIterator<ASwarmEnemy> It(GetWorld());It;++It) { It->Destroy(); }
        const FVector Origin=GetActorLocation();
        const uint8 Launcher=static_cast<uint8>(EGeneralWeapon::Launcher);
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Target=GetWorld()->SpawnActor<ASwarmEnemy>(Origin+GetActorForwardVector()*450.f,FRotator::ZeroRotator,Params);
        bool bFlight=false,bHit=false;
        if(Target)
        {
            const float Before=Target->GetHealth();
            LaunchGeneralProjectile(Launcher,Origin,Origin+GetActorForwardVector()*1000.f,30.f,8.f,true);
            TickGeneralProjectiles(GetCombatTime(),.04f);
            bFlight=GeneralProjectiles.Num()==1 && FVector::Dist2D(GeneralProjectiles[0].Position,Origin)>100.f
                && FMath::IsNearlyEqual(Target->GetHealth(),Before);
            TickGeneralProjectiles(GetCombatTime()+.2f,.2f);
            bHit=GeneralProjectiles.IsEmpty() && (!IsValid(Target)||Target->GetHealth()<Before);
            if(IsValid(Target)) { Target->Destroy(); }
        }
        const FVector Isolated=Origin+FVector(5000,5000,3000);
        AActor* Wall=GetWorld()->SpawnActor<AActor>(Isolated+FVector(250,0,0),FRotator::ZeroRotator,Params);
        UBoxComponent* Box=NewObject<UBoxComponent>(Wall);
        Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(12,200,200));
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent(); Box->SetWorldLocation(Isolated+FVector(250,0,0));
        const uint8 Blade=static_cast<uint8>(EGeneralWeapon::Blade);
        LaunchGeneralProjectile(Blade,Isolated,Isolated+FVector(900,0,0),47.f,20.f,false);
        TickGeneralProjectiles(GetCombatTime()+.4f,.2f);
        const bool bBounce=GeneralProjectiles.Num()==1 && GeneralProjectiles[0].Bounces==1 && GeneralProjectiles[0].Direction.X<-.9f;
        GeneralProjectiles.Reset(); Wall->Destroy();
        LaunchGeneralProjectile(Launcher,Isolated,Isolated+FVector(280,0,0),30.f,8.f,true);
        TickGeneralProjectiles(GetCombatTime()+.6f,.5f);
        const bool bExpires=GeneralProjectiles.IsEmpty();
        TArray<TWeakObjectPtr<ASwarmEnemy>> LineTargets;
        for(int32 J=1;J<=3;++J)
        { LineTargets.Add(GetWorld()->SpawnActor<ASwarmEnemy>(Isolated+FVector(J*160.f,0,0),FRotator::ZeroRotator,Params)); }
        TArray<float> HealthBefore;
        for(const auto& E:LineTargets) { HealthBefore.Add(E.IsValid()?E->GetHealth():0.f); }
        LaunchGeneralProjectile(static_cast<uint8>(EGeneralWeapon::Boomerang),Isolated,Isolated+FVector(1000,0,0),30.f,8.f,false);
        TickGeneralProjectiles(GetCombatTime()+.7f,.5f);
        int32 Pierced=0;
        for(int32 J=0;J<LineTargets.Num();++J)
        {
            if(!LineTargets[J].IsValid() || LineTargets[J]->GetHealth()<HealthBefore[J]) { ++Pierced; }
            if(LineTargets[J].IsValid()) { LineTargets[J]->Destroy(); }
        }
        GeneralProjectiles.Reset();
        for(int32 Shot=0;Shot<300;++Shot) { LaunchGeneralProjectile(Launcher,Isolated,Isolated+FVector(1000,0,0),30.f,8.f,true); }
        const bool bBounded=GeneralProjectiles.Num()==256;
        GeneralProjectiles.Reset();
        UE_LOG(LogTemp,Display,TEXT("General projectile: flight=%d impact=%d bounce=%d rangeExpiry=%d budget=%d pierced=%d"),
            bFlight,bHit,bBounce,bExpires,bBounded,Pierced);
        return;
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmProjectileIntegrationTest")))
    {
        int32 Passed=0,Total=0;
        static const int32 Bonus[]={0,1,1,2,2,3};
        for(int32 Champion : {7,8})
        {
            ApplyCharacterChoice(Champion);
            RunWeaponStats.Init(0,8);
            for(int32 Evolved=0;Evolved<2;++Evolved)
            {
                bSignatureWeaponEvolved=Evolved!=0;
                for(int32 Level=1;Level<=5;++Level)
                {
                    SignatureWeaponLevel=Level;
                    for(int32 Stat=0;Stat<=5;++Stat)
                    {
                        RunWeaponStats[2]=Stat;
                        XayahFeathers.Reset(); AuroraBolts.Reset();
                        if(Champion==8) { FireXayahWeapon(GetCombatTime()); }
                        else { FireAuroraWeapon(GetCombatTime()); }
                        const int32 Actual=Champion==8?XayahFeathers.Num():AuroraBolts.Num();
                        const int32 Expected=(Champion==7 && Evolved?6:Level)+Bonus[Stat];
                        ++Total; Passed+=Actual==Expected?1:0;
                    }
                }
            }
        }
        XayahFeathers.Reset(); AuroraBolts.Reset();
        UE_LOG(LogTemp,Display,TEXT("Projectile integration: passed=%d total=%d"),Passed,Total);
        return;
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("GeneralWeaponTuningTest")))
    {
        bool bValid=true;
        for(uint8 W=0;W<GeneralWeaponCount;++W)
        {
            float PreviousDamage=0.f,PreviousCooldown=TNumericLimits<float>::Max();
            for(int32 Rank=1;Rank<=5;++Rank)
            {
                const auto Base=GetGeneralWeaponStats(W,Rank,false);
                const auto Evolved=GetGeneralWeaponStats(W,Rank,true);
                bValid &= Base.Damage>=PreviousDamage && Base.Cooldown<=PreviousCooldown && Base.Cooldown>=.08f
                    && Evolved.Damage>=Base.Damage && Evolved.Cooldown>0.f && Base.Radius>0.f && Base.Projectiles>=0;
                PreviousDamage=Base.Damage; PreviousCooldown=Base.Cooldown;
            }
        }
        // Exercise the actual firing function, not just the tuning lookup.
        ApplyCharacterChoice(0); RunWeaponStats.Init(0,8); GeneralWeaponRanks.Init(1,GeneralWeaponCount);
        PermanentAbilityHaste=0.f; JinxPassiveEnd=XayahBuffEnd=0.f;
        for(TActorIterator<ASwarmEnemy> It(GetWorld());It;++It) { It->Destroy(); }
        const uint8 Launcher=static_cast<uint8>(EGeneralWeapon::Launcher);
        FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Target=GetWorld()->SpawnActor<ASwarmEnemy>(GetActorLocation()+GetActorForwardVector()*450.f,FRotator::ZeroRotator,P);
        bool bHit=false;
        if(Target)
        {
            const float Before=Target->GetHealth();
            const float Now=GetCombatTime();
            GeneralProjectiles.Reset();
            FireGeneralWeapon(Launcher,Now);
            const bool bWaitsForImpact=IsValid(Target)&&FMath::IsNearlyEqual(Target->GetHealth(),Before);
            TickGeneralProjectiles(Now+.2f,.2f);
            bHit=(!IsValid(Target)||Target->GetHealth()<Before)
                && bWaitsForImpact && FMath::IsNearlyEqual(GeneralWeaponReadyAt[Launcher]-Now,.28f,.01f);
            if(IsValid(Target)) { Target->Destroy(); }
        }
        UE_LOG(LogTemp,Display,TEXT("Weapon tuning: configurations=200 valid=%d launcherDamageAndCadence=%d"),bValid,bHit);
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("ArcadeVfxGallery")))
    {
        InvulnerableEnd = GetCombatTime() + 120.f;
        JinxWeaponReadyAt = GetCombatTime() + 120.f;
        for (int32 Weapon = 0; Weapon < GeneralWeaponCount; ++Weapon)
        {
            const FVector P = GetActorLocation() + FVector((Weapon % 5 - 2) * 170.f, (Weapon / 5 - 1.5f) * 190.f, 0);
            ShowWeaponVfx(Weapon, P, P + FVector(90.f, 15.f, 0.f), 65.f, Weapon % 2, Index != 0);
        }
        UE_LOG(LogTemp, Display, TEXT("VFX gallery: weapons=20 evolved=%d active=%d"),Index!=0,CombatVfx?CombatVfx->GetActiveCount():0);
        if (auto* Player = Cast<APlayerController>(GetController()))
        {
            FTimerHandle Shot;
            GetWorldTimerManager().SetTimer(Shot,FTimerDelegate::CreateWeakLambda(this,[Player]()
            { if (IsValid(Player)) { Player->ConsoleCommand(TEXT("Shot ShowUI"),true); } }),.65f,false);
        }
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("ArcadeVfxLifecycleTest")))
    {
        auto* Probe=GetCombatVfx();
        if (!Probe) { return; }
        Probe->Clear();
        for (int32 J=0;J<160;++J)
        { Probe->Emit(ECombatVfxShape::Pulse,Aim,Aim,100.f,FLinearColor::White,.2f); }
        Probe->Emit(ECombatVfxShape::Ring,Aim,Aim,100.f,FLinearColor::White,.16f,false,7);
        const bool bBudgetPass=Probe->GetActiveCount()<=96;
        FTimerHandle Check;
        GetWorldTimerManager().SetTimer(Check,FTimerDelegate::CreateWeakLambda(this,[Probe,bBudgetPass]()
        {
            UE_LOG(LogTemp,Display,TEXT("VFX lifecycle: budget=%d expired=%d active=%d"),bBudgetPass,
                IsValid(Probe)&&Probe->GetActiveCount()==0,IsValid(Probe)?Probe->GetActiveCount():-1);
            if (IsValid(Probe)) { Probe->Destroy(); }
        }),.8f,false);
        // Isolate the probe from normal gameplay emitters during this lifecycle assertion.
        CombatVfx=nullptr;
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("SwarmHudGrowthSmokeTest")))
    {
        RunHealthBonus = 300.f; RunArmorBonus = 16.f;
        RunWeaponStats.Init(0, 8);
        RunWeaponStats[0] = 2; RunWeaponStats[1] = 2; RunWeaponStats[2] = 3; RunWeaponStats[6] = 1;
        PendingLevelChoices = 1;
        LevelChoices = {6, 7, PickupRangeChoice};
        LevelChoiceDeadline = GetLevelChoiceClock() + 30.f;
        InvulnerableEnd = GetCombatTime() + 30.f;
        UE_LOG(LogTemp, Display, TEXT("Growth HUD smoke: owned=%d cards=%d"), GetRunStatCount(), LevelChoices.Num());
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("SwarmDamageSlotsSmokeTest")))
    {
        PermanentDamageMultiplier = 1.3f;
        RunWeaponStats.Init(0, 8);
        RunWeaponStats[0] = 5;
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Enemy = GetWorld()->SpawnActor<ASwarmEnemy>(GetActorLocation() + FVector(900.f, 0.f, 0.f), FRotator::ZeroRotator, Spawn);
        bool bDamagePass = false;
        if (Enemy)
        {
            const float Before = Enemy->GetHealth();
            HitAdditionalEnemy(Enemy, 1.f);
            const float AfterSkill = Enemy->GetHealth();
            ApplyGeneralWeaponDamage(Enemy, 1.f, 0, false, false);
            const float AfterWeapon = Enemy->GetHealth();
            bDamagePass = FMath::IsNearlyEqual(Before - AfterSkill, 1.95f, .01f)
                && FMath::IsNearlyEqual(AfterSkill - AfterWeapon, 1.95f, .01f);
            Enemy->Destroy();
        }
        RunWeaponStats.Init(0, 8);
        RunWeaponStats[0] = RunWeaponStats[1] = 1;
        RunHealthBonus = 150.f; RunArmorBonus = 8.f; RunMoveBonus = .08f; RunRegen = 4.f;
        PendingLevelChoices = 1;
        LevelChoices = {7};
        ServerSelectLevelChoice(0, LevelChoiceSequence);
        const bool bSeventhRejected = RunWeaponStats[2] == 0 && PendingLevelChoices == 1 && GetRunStatCount() == 6;
        LevelChoices = {5};
        ServerSelectLevelChoice(0, LevelChoiceSequence);
        const bool bExistingUpgraded = RunWeaponStats[0] == 2 && PendingLevelChoices == 0;
        bool bPoolPass = true;
        PendingLevelChoices = 1;
        for (int32 Attempt = 0; Attempt < 30; ++Attempt)
        {
            RollLevelChoices();
            for (uint8 Choice : LevelChoices)
            {
                if ((Choice >= 1 && Choice <= 10) || Choice == PickupRangeChoice || Choice == ExperienceChoice)
                { bPoolPass &= GetRunStatRank(Choice) > 0; }
            }
        }
        PendingLevelChoices = 0; LevelChoices.Reset(); EndLevelChoiceFreeze();
        UE_LOG(LogTemp, Display, TEXT("Damage slots smoke: sharedDamage=%d seventhRejected=%d existingUpgrade=%d pool=%d"),
            bDamagePass, bSeventhRejected, bExistingUpgraded, bPoolPass);
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("SwarmEvolutionSmokeTest")))
    {
        static const uint8 Requirements[] = {6, 7, 2, 8, 1, 10, 3, ExperienceChoice, PickupRangeChoice};
        int32 Passed = 0;
        for (int32 Champion = 0; Champion < 9; ++Champion)
        {
            CharacterChoice = Champion;
            SignatureWeaponLevel = 4;
            bSignatureWeaponEvolved = false;
            const bool bReachedFive = UpgradeSignatureWeapon() && SignatureWeaponLevel == 5 && !IsSignatureWeaponEvolved();
            const bool bCap = !UpgradeSignatureWeapon();
            RunWeaponStats.Init(0, 8);
            RunHealthBonus = RunArmorBonus = RunMoveBonus = RunRegen = 0.f;
            const bool bMissingStat = !CanEvolveSignatureWeapon();
            const uint8 Requirement = Requirements[Champion];
            if (Requirement == 1) { RunHealthBonus = 150.f; }
            else if (Requirement == 2) { RunArmorBonus = 8.f; }
            else if (Requirement == 3) { RunMoveBonus = .08f; }
            else { RunWeaponStats[Requirement == PickupRangeChoice ? 6 : Requirement == ExperienceChoice ? 7 : Requirement - 5] = 1; }
            PendingAccessCards = 1;
            RollLevelChoices();
            const bool bOffered = LevelChoices.Num() == 1 && LevelChoices[0] == SignatureEvolutionChoice;
            if (bOffered) { ServerSelectLevelChoice(0, LevelChoiceSequence); }
            const bool bPass = bReachedFive && bCap && bMissingStat && bOffered
                && IsSignatureWeaponEvolved() && PendingAccessCards == 0 && !CanEvolveSignatureWeapon();
            Passed += bPass ? 1 : 0;
            UE_LOG(LogTemp, Display, TEXT("Signature evolution smoke: character=%d pass=%d"), Champion, bPass);
        }
        UE_LOG(LogTemp, Display, TEXT("Signature evolution smoke total: %d/9"), Passed);
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("SwarmGrowthSmokeTest")))
    {
        RunWeaponStats.Init(5, 8);
        bool bProjectilePass = true;
        static const int32 Expected[] = {0, 1, 1, 2, 2, 3};
        for (int32 Rank = 0; Rank <= 5; ++Rank)
        {
            RunWeaponStats[2] = Rank;
            bProjectilePass &= GetCombatBonusProjectiles() == Expected[Rank];
        }
        RunHealthBonus = 750.f; RunArmorBonus = 40.f; RunMoveBonus = .4f; RunRegen = 20.f;
        SignatureWeaponLevel = 5;
        bSignatureWeaponEvolved = true;
        GeneralWeapons = {0, 1, 2, 3};
        GeneralWeaponRanks.Init(5, GeneralWeaponCount);
        EvolvedGeneralWeapons = GeneralWeapons;
        PendingLevelChoices = 1;
        LevelChoices = {5};
        ServerSelectLevelChoice(0, LevelChoiceSequence);
        const bool bCapRejected = RunWeaponStats[0] == 5 && PendingLevelChoices == 1;
        RunWeaponStats[0] = 4;
        ServerSelectLevelChoice(0, LevelChoiceSequence);
        const bool bFinalRankApplied = RunWeaponStats[0] == 5 && PendingLevelChoices == 0;
        PendingLevelChoices = 1;
        RollLevelChoices();
        const bool bFallback = LevelChoices.Num() == 1 && LevelChoices[0] == RunGoldRewardChoice;
        const int32 BeforeGold = RunGold;
        ServerSelectLevelChoice(0, LevelChoiceSequence);
        const bool bRewardApplied = RunGold == BeforeGold + 25 && PendingLevelChoices == 0;
        UE_LOG(LogTemp, Display, TEXT("Growth smoke: projectiles=%d cappedRejected=%d finalRank=%d fallback=%d gold=%d"),
            bProjectilePass, bCapRejected, bFinalRankApplied, bFallback, bRewardApplied);
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("SwarmCooldownSmokeTest")))
    {
        // Exercise real casts with both permanent and run haste enabled.
        PermanentAbilityHaste = 20.f;
        RunWeaponStats.SetNum(8);
        RunWeaponStats[1] = 5;
        RunWeaponStats[3] = RunWeaponStats[4] = 5;
        PermanentDurationMultiplier = PermanentAreaMultiplier = 1.2f;
        JinxPassiveEnd = 0.f;
        XayahBuffEnd = 0.f;
        JinxEReadyAt = JinxRReadyAt = ExtraChargeReadyAt = 0.f;
        BriarFrenzyEnd = AuroraRealmEnd = 0.f;
        bRivenDashing = false;
        ExtraSkillCharges = Index == 3 ? 2 : Index == 5 ? 3 : 0;
        const float Now = GetCombatTime();
        ServerCastCharacterSkill(false, Aim);
        static const float BaseE[] = {8.f, 16.f, 17.f, 12.f, 20.f, 10.f, 8.f, 15.f, 13.f};
        static const float BaseR[] = {50.f, 90.f, 80.f, 90.f, 120.f, 25.f, 100.f, 90.f, 90.f};
        const float E = (Index == 3 || Index == 5 ? ExtraChargeReadyAt : JinxEReadyAt) - Now;
        float ObservedDuration = Index == 4 ? BriarFrenzyEnd - Now
            : Index == 7 ? AuroraVeilEnd - Now : Index == 8 ? XayahPickupEnd - Now : 0.f;
        // Briar cannot start her ultimate during the E frenzy.
        BriarFrenzyEnd = 0.f;
        ServerCastCharacterSkill(true, Aim);
        const float R = JinxRReadyAt - Now;
        if (Index == 6) { ObservedDuration = RivenUltimateEnd - Now; }
        const float BaseDuration = Index == 4 ? 6.f : Index == 6 ? 15.f : Index == 7 || Index == 8 ? 3.f : 0.f;
        const bool bDurationPass = FMath::IsNearlyEqual(ObservedDuration, BaseDuration * 1.8f, .02f);
        UE_LOG(LogTemp, Display, TEXT("Duration cast smoke: character=%d seconds=%.3f pass=%d area=%.3f"),
            Index, ObservedDuration, bDurationPass, GetCombatAreaMultiplier());
        const bool bPass = bDurationPass && FMath::IsNearlyEqual(E, BaseE[Index] / 1.7f, .02f)
            && FMath::IsNearlyEqual(R, BaseR[Index] / 1.7f, .02f);
        UE_LOG(LogTemp, Display, TEXT("Cooldown cast smoke: character=%d haste=70 E=%.3f R=%.3f pass=%d"),
            Index, E, R, bPass);
        return;
    }
    if (FParse::Param(FCommandLine::Get(),TEXT("ArcadeSignatureReview")))
    {
        const float Now=GetCombatTime();
        InvulnerableEnd=Now+60.f;
        bSignatureWeaponEvolved=FParse::Param(FCommandLine::Get(),TEXT("EvolvedVfx"));
        SignatureWeaponLevel=bSignatureWeaponEvolved?5:1;
        switch(Index)
        {
        case 0: FireJinxWeapon(Now); break;
        case 1: FireSeraphineWeapon(Now); break;
        case 2: FireLeonaWeapon(Now); break;
        case 3: case 4: case 5: YasuoFlow=100.f; FireAdditionalWeapon(Now); break;
        case 6: BeginRivenDash(Now,false); break;
        case 7: FireAuroraWeapon(Now); break;
        case 8: FireXayahWeapon(Now); break;
        }
        JinxWeaponReadyAt=Now+60.f;
        FTimerHandle Capture;
        GetWorldTimerManager().SetTimer(Capture,FTimerDelegate::CreateWeakLambda(this,[this]()
        {
            if(auto* Player=Cast<APlayerController>(GetController())) { Player->ConsoleCommand(TEXT("Shot ShowUI"),true); }
        }),Index==6?.3f:.09f,false);
        UE_LOG(LogTemp,Display,TEXT("VFX signature review: character=%d evolved=%d"),Index,bSignatureWeaponEvolved);
        return;
    }
    if (FParse::Param(FCommandLine::Get(),TEXT("ArcadeSkillReview")))
    {
        InvulnerableEnd=GetCombatTime()+60.f;
        JinxWeaponReadyAt=GetCombatTime()+60.f;
        ServerCastCharacterSkill(FParse::Param(FCommandLine::Get(),TEXT("UltimateOnly")),Aim);
        FTimerHandle Capture;
        GetWorldTimerManager().SetTimer(Capture,FTimerDelegate::CreateWeakLambda(this,[this]()
        {
            if(auto* Player=Cast<APlayerController>(GetController())) { Player->ConsoleCommand(TEXT("Shot ShowUI"),true); }
        }),.15f,false);
        UE_LOG(LogTemp,Display,TEXT("VFX skill review: character=%d ultimate=%d"),Index,FParse::Param(FCommandLine::Get(),TEXT("UltimateOnly")));
        return;
    }
    ServerCastCharacterSkill(false, Aim);
    ServerCastCharacterSkill(true, Aim);
    if (GetSelectedCharacter() == 8)
    {
        JinxWeaponReadyAt = GetCombatTime() + 30.f;
        InvulnerableEnd = GetCombatTime() + 30.f;
        FTimerHandle RecallTimer;
        GetWorldTimerManager().SetTimer(RecallTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
        {
            const int32 Before = XayahFeatherMeshes.Num();
            JinxEReadyAt = 0.f;
            CastXayahSkill(false, GetCombatTime());
            UpdateXayahFeatherMeshes();
            UE_LOG(LogTemp, Display, TEXT("Xayah recall smoke: landedBefore=%d after=%d meshes=%d"),
                Before, LandedXayahFeathers.Num(), XayahFeatherMeshes.Num());
        }), 3.f, false);
    }
    if (GetSelectedCharacter() == 3)
    {
        JinxWeaponReadyAt = GetCombatTime() + 30.f;
        InvulnerableEnd = GetCombatTime() + 30.f;
        const int32 InitialCount = CombatHazards.Num();
        FTimerHandle CheckTentacleExpiration;
        GetWorldTimerManager().SetTimer(CheckTentacleExpiration, FTimerDelegate::CreateWeakLambda(this,
            [this, InitialCount]()
            {
                UE_LOG(LogTemp, Display, TEXT("Tentacle lifetime smoke: spawned=%d hazards=%d visuals=%d meshes=%d"),
                    InitialCount, CombatHazards.Num(), TentacleVisuals.Num(), TentacleMeshes.Num());
            }), 17.f, false);
    }
    if (GetSelectedCharacter() == 5)
    {
        const FVector Center = WindWallCenter;
        const FVector Normal = WindWallNormal;
        const FVector Side = FVector::CrossProduct(FVector::UpVector, Normal);
        float HitTime = -1.f;
        const bool bIntercepts = TraceWindWall(Center - Normal * 500.f, Center + Normal * 500.f, 14.f, HitTime)
            && HitTime >= 0.f && HitTime <= 1.f;
        const FVector Outside = Center + Side * (WindWallHalfWidth + 40.f);
        const bool bAllowsOutside = !TraceWindWall(Outside - Normal * 500.f, Outside + Normal * 500.f, 14.f, HitTime);
        FTimerHandle CheckWallExpiration;
        GetWorldTimerManager().SetTimer(CheckWallExpiration, FTimerDelegate::CreateWeakLambda(this,
            [this, Center, Normal, bIntercepts, bAllowsOutside]()
            {
                float Time = -1.f;
                const bool bExpired = !TraceWindWall(Center - Normal * 500.f, Center + Normal * 500.f, 14.f, Time);
                UE_LOG(LogTemp, Display, TEXT("Wind wall smoke: blocks=%d outside=%d expired=%d visualRemoved=%d"),
                    bIntercepts, bAllowsOutside, bExpired, WindWallVisual == nullptr);
            }), 4.2f, false);
    }
    UE_LOG(LogTemp, Display, TEXT("Skill visual smoke cast: character=%d"), GetSelectedCharacter());
}
#endif
