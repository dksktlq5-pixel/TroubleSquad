#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#endif
#include "AnimaPreview.h"
#include "SwarmLoot.h"
#include "GameFramework/PlayerState.h"
#include "ClientPreferences.h"
#include "CombatVfx.h"
#include "SwarmArenas.h"
#include "SwarmEncounter.h"
#include "CombatAudio.h"
#include "DistrictProgress.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "CombatHUD.h"
#include "CombatHealth.h"
#include "DrawDebugHelpers.h"

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
        SelectedIndex=INDEX_NONE;
        DesiredCameraDistance = DistrictDefaultZoom;
        CameraArm->TargetArmLength = DesiredCameraDistance;
        CameraArm->SetRelativeRotation(FRotator(-61.f, DistrictCameraYaw, 0.f));
    }

    if(IsLocallyControlled()&&bDistrictCamera) { ClientPreferences::EnterWorld(true); }
#if !UE_BUILD_SHIPPING
    if(IsLocallyControlled()&&bDistrictCamera&&FParse::Param(FCommandLine::Get(),TEXT("HostTravelAcceptance")))
    { FTimerHandle Back;InvulnerableEnd=GetCombatTime()+100.f;GetWorldTimerManager().SetTimer(Back,FTimerDelegate::CreateWeakLambda(this,[this](){ReturnToSoloMenu();}),5.f,false); }
#endif
    if (HasAuthority() && GetController())
    {
        if (AAnimaPreviewMode* Mode = GetWorld()->GetAuthGameMode<AAnimaPreviewMode>())
        {
            ApplyCharacterChoice(Mode->GetCharacterChoice(GetController()));
        }
    }
    if(HasAuthority()) { if(auto* Mode=GetWorld()->GetAuthGameMode<AAnimaPreviewMode>()) { GameDifficulty=Mode->GetDifficulty(); } }
    SelectCharacter(CharacterChoice);
    SubmitPermanentRanks();
}

void AAnimaPreviewCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    if (AAnimaPreviewMode* Mode = GetWorld()->GetAuthGameMode<AAnimaPreviewMode>())
    {
        ApplyCharacterChoice(Mode->GetCharacterChoice(NewController));
        Mode->SyncTeamProgress(this);
        if(IsLocallyControlled()) { SubmitPermanentRanks(); }
    }
}

void AAnimaPreviewCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AAnimaPreviewCharacter, CharacterChoice);
    DOREPLIFETIME(AAnimaPreviewCharacter, GameDifficulty);
    DOREPLIFETIME(AAnimaPreviewCharacter, bStoryAugmentsUnlocked);
    DOREPLIFETIME(AAnimaPreviewCharacter, bExtremeAugmentsUnlocked);

    DOREPLIFETIME(AAnimaPreviewCharacter, WorldBoonType);
    DOREPLIFETIME(AAnimaPreviewCharacter, WorldBoonEnd);
    DOREPLIFETIME(AAnimaPreviewCharacter, VacuumEnd);
    DOREPLIFETIME(AAnimaPreviewCharacter, RunBoonsCollected);
    DOREPLIFETIME(AAnimaPreviewCharacter, BoonSpireKills);
    DOREPLIFETIME(AAnimaPreviewCharacter, BoonSpireRemaining);
    DOREPLIFETIME(AAnimaPreviewCharacter, BoonSpireCenter);

    DOREPLIFETIME(AAnimaPreviewCharacter, PermanentRegen);
    DOREPLIFETIME(AAnimaPreviewCharacter, PermanentCritChance);
    DOREPLIFETIME(AAnimaPreviewCharacter, PermanentProjectiles);
    DOREPLIFETIME(AAnimaPreviewCharacter, PermanentPickupMultiplier);
    DOREPLIFETIME(AAnimaPreviewCharacter, PermanentGoldMultiplier);
    DOREPLIFETIME(AAnimaPreviewCharacter, PermanentBoonMultiplier);
    DOREPLIFETIME(AAnimaPreviewCharacter, BaseArmor);
    DOREPLIFETIME(AAnimaPreviewCharacter, BaseMoveSpeed);

    DOREPLIFETIME(AAnimaPreviewCharacter, OwnedAugments);
    DOREPLIFETIME(AAnimaPreviewCharacter, AugmentCardStacks);
    DOREPLIFETIME(AAnimaPreviewCharacter, AugmentArmorStacks);
    DOREPLIFETIME(AAnimaPreviewCharacter, AugmentRegenStacks);
    DOREPLIFETIME(AAnimaPreviewCharacter, AugmentJourneyStacks);
    DOREPLIFETIME(AAnimaPreviewCharacter, AugmentLevels);
    DOREPLIFETIME(AAnimaPreviewCharacter, AugmentXpEnd);
    DOREPLIFETIME(AAnimaPreviewCharacter, AugmentXpBonus);
    DOREPLIFETIME(AAnimaPreviewCharacter, AugmentCelebrateEnd);
    DOREPLIFETIME(AAnimaPreviewCharacter, AugmentStationaryTime);
    DOREPLIFETIME(AAnimaPreviewCharacter, AugmentRecentCrits);
    DOREPLIFETIME(AAnimaPreviewCharacter, YuumiQuestRemaining);
    DOREPLIFETIME(AAnimaPreviewCharacter, YuumiQuestKills);
    DOREPLIFETIME(AAnimaPreviewCharacter, bYuumiCarryingToken);
    DOREPLIFETIME(AAnimaPreviewCharacter, YuumiQuestType);
    DOREPLIFETIME(AAnimaPreviewCharacter, YuumiQuestGoal);
    DOREPLIFETIME(AAnimaPreviewCharacter, YuumiQuestCenter);
    DOREPLIFETIME(AAnimaPreviewCharacter, BulletLoverLevels);

    DOREPLIFETIME_CONDITION(AAnimaPreviewCharacter, LevelChoiceOffer, COND_OwnerOnly);
    DOREPLIFETIME(AAnimaPreviewCharacter, CombatLevel);
    DOREPLIFETIME(AAnimaPreviewCharacter, Experience);
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
    DOREPLIFETIME(AAnimaPreviewCharacter, EvolvedGeneralWeapons);
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
    DOREPLIFETIME_CONDITION(AAnimaPreviewCharacter, GeneralWeaponCooldownEnds, COND_OwnerOnly);
    DOREPLIFETIME(AAnimaPreviewCharacter, ReviveProgress);
    DOREPLIFETIME(AAnimaPreviewCharacter, ReviveWindowEndsAt);
    DOREPLIFETIME(AAnimaPreviewCharacter, bReviveExpired);
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
    if(IsLocallyControlled()&&bDistrictCamera) { ClientPreferences::EnterWorld(true); }
    SubmitPermanentRanks();

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
    if(HasAuthority() && !bPermanentRanksApplied)
    {
        const float Fraction=SelectedIndex==INDEX_NONE?1.f:FMath::Clamp(CombatHealth/FMath::Max(1.f,MaxCombatHealth),0.f,1.f);
        MaxCombatHealth=Stats.Health+RunHealthBonus; CombatHealth=MaxCombatHealth*Fraction;
        BaseArmor=Stats.Armor; BaseMoveSpeed=Stats.MoveSpeed;
        PermanentExperienceMultiplier=PermanentDamageMultiplier=PermanentDurationMultiplier=PermanentAreaMultiplier=1.f;
        PermanentAbilityHaste=0.f;
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
    AttackMotions.Reset();
    for(const TCHAR* Slot : {TEXT("Weapon"),TEXT("E"),TEXT("R")})
    {
        const FString AnimName=TEXT("AN_")+Name+TEXT("_")+Slot;
        AttackMotions.Add(LoadObject<UAnimSequence>(nullptr,*(Folder+AnimName+TEXT(".")+AnimName)));
    }
    GetMesh()->SetSkeletalMesh(CharacterMesh);
    // Jinx's imported mesh faces local +Y. The pawn aims along local +X.
    GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
    const FBoxSphereBounds ImportedBounds = CharacterMesh->GetBounds();
    const float UniformScale = 180.f / FMath::Max(1.f, ImportedBounds.BoxExtent.Z * 2.f);
    GetMesh()->SetRelativeScale3D(FVector(UniformScale));
    GetMesh()->SetRelativeLocation(FVector(0,0,-GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - (ImportedBounds.Origin.Z-ImportedBounds.BoxExtent.Z)*UniformScale));
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
            const float Applied=UGameplayStatics::ApplyPointDamage(Target, (bHitEnemy ? 8.f : 20.f) + ConsumeSunlight(Target),
                (End - Start).GetSafeNormal(), Hit, GetController(), this, UDamageType::StaticClass());
            if(Applied>0.f){NotifySignatureHit(Hit.ImpactPoint);}
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
    return PermanentDamageMultiplier * (1.f + .10f * (RunWeaponStats.IsValidIndex(0) ? RunWeaponStats[0] : 0)) * GetAugmentDamageMultiplier() * (CharacterChoice==4?1.f+.6f*(1.f-FMath::Clamp(CombatHealth/FMath::Max(1.f,MaxCombatHealth),0.f,1.f)):1.f) * (WorldBoonType==0&&GetWorldBoonRemaining()>0.f?1.5f:1.f);
}

float AAnimaPreviewCharacter::GetCombatCooldown(float BaseCooldown) const
{
    const float RunHaste = RunWeaponStats.IsValidIndex(1) ? 10.f * RunWeaponStats[1] : 0.f;
    const float Haste = PermanentAbilityHaste + RunHaste + GetAugmentHaste() + (WorldBoonType==1&&GetWorldBoonRemaining()>0.f?100.f:0.f) + (GetJinxPassiveRemaining() > 0.f ? 150.f : 0.f)
        + (GetXayahBuffRemaining() > 0.f ? 80.f : 0.f);
    return FMath::Max(0.f, BaseCooldown) / (1.f + FMath::Max(-80.f, Haste) * .01f);
}

float AAnimaPreviewCharacter::GetCombatDurationMultiplier() const
{
    return PermanentDurationMultiplier + .12f * (RunWeaponStats.IsValidIndex(3) ? RunWeaponStats[3] : 0);
}

float AAnimaPreviewCharacter::GetCombatAreaMultiplier() const
{
    return PermanentAreaMultiplier + .11f * (RunWeaponStats.IsValidIndex(4) ? RunWeaponStats[4] : 0)
        + (HasAugment(ESwarmAugment::Journey) ? .01f*AugmentJourneyStacks : 0.f)
        + (HasAugment(ESwarmAugment::CriticalExpansion) ? .01f*AugmentRecentCrits : 0.f)
        + (HasAugment(ESwarmAugment::SizeScrambler) ? .15f*(FMath::FloorToInt(GetCombatTime()/8.f)%5) : 0.f)
        + (HasAugment(ESwarmAugment::Celebrate) && GetCombatTime()<AugmentCelebrateEnd ? .3f : 0.f)
        + (HasAugment(ESwarmAugment::Cycle) && FMath::FloorToInt(GetCombatTime()/8.f)%5==2 ? .5f : 0.f);
}

int32 AAnimaPreviewCharacter::GetCombatBonusProjectiles() const
{
    const int32 Rank = RunWeaponStats.IsValidIndex(2) ? FMath::Clamp<int32>(RunWeaponStats[2], 0, 5) : 0;
    return (Rank + 1) / 2 + PermanentProjectiles + (HasAugment(ESwarmAugment::Spray)?4:0)
        + (HasAugment(ESwarmAugment::BulletLover)?BulletLoverLevels/6:0)
        + (HasAugment(ESwarmAugment::Cycle) && FMath::FloorToInt(GetCombatTime()/8.f)%5==3?3:0);
}

float AAnimaPreviewCharacter::GetJinxSkillRemaining(bool bUltimate) const
{
    return FMath::Max(0.f, (bUltimate ? JinxRReadyAt : JinxEReadyAt) - GetCombatTime());
}

void AAnimaPreviewCharacter::ServerCastCharacterSkill_Implementation(bool bUltimate, FVector_NetQuantize Aim)
{
    if ((bUltimate && CombatLevel < 6) || CharacterChoice > 8 || !bDistrictCamera || bDistrictFinished || CombatHealth <= 0.f || Aim.ContainsNaN()
        || !LevelChoices.IsEmpty() || UGameplayStatics::GetGlobalTimeDilation(this)<.001f)
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

    ++CareerSkills;
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

    ReadyAt = Now + (bUltimate ? GetUltimateCooldown(50.f) : GetCombatCooldown(8.f));
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
    if (!HasAuthority() || Damage <= 0.f || CombatHealth <= 0.f || HasTimedInvulnerability(GetCombatTime()))
    {
        return 0.f;
    }
    if (HasAugment(ESwarmAugment::ArmorStack)) { AugmentArmorStacks=FMath::Min(10,AugmentArmorStacks+1); }
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
    ReadyAt = Now + (bUltimate ? GetUltimateCooldown(90.f) : GetCombatCooldown(16.f));
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
            const FVector Impact=Target->GetActorLocation();
            if(UGameplayStatics::ApplyDamage(Target, Wave.Damage + ConsumeSunlight(Target), GetController(), this, UDamageType::StaticClass())>0.f){NotifySignatureHit(Impact);}
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
    return CharacterChoice == 2 ? GetLeonaArmor(GetCombatTime()) : BaseArmor + RunArmorBonus
        + (HasAugment(ESwarmAugment::WarMachine)?10.f:0.f) + 8.f*AugmentArmorStacks;
}

float AAnimaPreviewCharacter::GetLeonaArmor(float Now) const
{
    return FMath::Max(0.f, BaseArmor + LeonaBonusArmor + RunArmorBonus
        + (HasAugment(ESwarmAugment::WarMachine)?10.f:0.f) + 8.f*AugmentArmorStacks) * (Now < LeonaEclipseEnd ? 2.f : 1.f);
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
        JinxRReadyAt = Now + GetUltimateCooldown(80.f);
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
    EndLevelChoiceSelection();
    if (CombatMusic) { CombatMusic->Stop(); }
    Super::EndPlay(Reason);
}

void AAnimaPreviewCharacter::ReturnToSoloMenu()
{
    if (IsLocallyControlled())
    {
        EndLevelChoiceSelection();
        UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/TroubleSquad/Maps/L_SoloMenu")));
    }
}

float AAnimaPreviewCharacter::GetLevelChoiceClock() const
{
    return GetWorld()->GetRealTimeSeconds();
}

void AAnimaPreviewCharacter::ApplyLevelChoiceOffer(const FSwarmChoiceOffer& Offer)
{
    if(Offer.Sequence<LevelChoiceSequence) { return; }
    const bool NewOffer=Offer.Sequence!=LevelChoiceSequence || LastPresentedChoiceSequence<0;
    LevelChoiceSequence=Offer.Sequence;LevelChoices=Offer.Cards;LevelChoiceDeadline=Offer.Deadline;
    bAugmentRewardOpen=Offer.bAugment;bAccessRewardOpen=Offer.bAccess;
    LastPresentedChoiceSequence=Offer.Sequence;
    if(NewOffer)
    {
        bLevelChoiceSubmitted=false;
        if(!Offer.Cards.IsEmpty()&&IsLocallyControlled()&&ClientPreferences::IsOpen())
        { ClientPreferences::Toggle(Cast<APlayerController>(GetController())); }
    }
    const auto* PS=GetPlayerState<APlayerState>();
    const float Transit=PS?FMath::Clamp(PS->GetPingInMilliseconds()*.0005f,0.f,.5f):0.f;
    const float LocalDeadline=Offer.Deadline>0.f?GetLevelChoiceClock()+FMath::Max(0.f,Offer.Deadline-Offer.ServerRealTime-Transit):0.f;
    LocalLevelChoiceDeadline=NewOffer||LocalLevelChoiceDeadline<=0.f?LocalDeadline:FMath::Min(LocalLevelChoiceDeadline,LocalDeadline);
    if(!Offer.Cards.IsEmpty()) { ServerConfirmLevelChoiceShown(Offer.Sequence); }
}

void AAnimaPreviewCharacter::OnRep_LevelChoiceOffer() { ApplyLevelChoiceOffer(LevelChoiceOffer); }
void AAnimaPreviewCharacter::ClientReceiveLevelChoices_Implementation(const FSwarmChoiceOffer& Offer)
{ if(!HasAuthority()) { ApplyLevelChoiceOffer(Offer); } }

void AAnimaPreviewCharacter::PublishLevelChoices()
{
    if(!HasAuthority()) { return; }
    LevelChoiceOffer.Cards=LevelChoices;LevelChoiceOffer.Sequence=LevelChoiceSequence;
    LevelChoiceOffer.Deadline=LevelChoiceDeadline;LevelChoiceOffer.ServerRealTime=GetLevelChoiceClock();LevelChoiceOffer.bAugment=bAugmentRewardOpen;LevelChoiceOffer.bAccess=bAccessRewardOpen;
    ForceNetUpdate();ClientReceiveLevelChoices(LevelChoiceOffer);
}

void AAnimaPreviewCharacter::ServerConfirmLevelChoiceShown_Implementation(int32 Sequence)
{
    if(Sequence==LevelChoiceSequence && !LevelChoices.IsEmpty())
    {
        if(!bChoicePresentationConfirmed) { UE_LOG(LogTemp,Display,TEXT("Choice presented: player=%s sequence=%d remaining=%.2f"),*GetName(),Sequence,GetLevelChoiceTimeRemaining()); }
        bChoicePresentationConfirmed=true;
    }
}

void AAnimaPreviewCharacter::SubmitLevelChoice(int32 Slot,int32 Sequence)
{
    if(Sequence!=LevelChoiceSequence || !LevelChoices.IsValidIndex(Slot) || bLevelChoiceSubmitted || CombatHealth<=0.f) { return; }
    if(!HasAuthority()) { bLevelChoiceSubmitted=true; }
    ServerSelectLevelChoice(Slot,Sequence);
}

void AAnimaPreviewCharacter::BeginLevelChoiceSelection()
{
    if(IsLocallyControlled()&&ClientPreferences::IsOpen()) { ClientPreferences::Toggle(Cast<APlayerController>(GetController())); }
    if(bDistrictCamera) { if(auto* Mode=GetWorld()->GetAuthGameMode<AAnimaPreviewMode>()) { Mode->SetLevelChoicePending(this,true); } }
}

void AAnimaPreviewCharacter::EndLevelChoiceSelection()
{
    if(auto* Mode=GetWorld()->GetAuthGameMode<AAnimaPreviewMode>()) { Mode->SetLevelChoicePending(this,false); }
    if(HasAuthority()) { ++LevelChoiceSequence;PublishLevelChoices(); }
}

float AAnimaPreviewCharacter::GetLevelChoiceTimeRemaining() const
{ return FMath::Max(0.f,(HasAuthority()?LevelChoiceDeadline:LocalLevelChoiceDeadline)-GetLevelChoiceClock()); }

void AAnimaPreviewCharacter::Tick(float DeltaTime)
{
    #if WITH_EDITOR
    TickCombatEnduranceTest(DeltaTime);
    TickRenderStressTest();
    TickGrowthNetworkTest();
    TickTeamChoiceNetworkReview();
    #endif
    if(bDistrictCamera&&(HasAuthority()||IsLocallyControlled())&&UGameplayStatics::GetCurrentLevelName(this)==TEXT("L_SubterraneanLab_Rebuilt"))
    {
        const FVector Safe=SwarmArenas::LabPositionOnLand(GetActorLocation(),GetCapsuleComponent()->GetScaledCapsuleRadius());
        if(!Safe.Equals(GetActorLocation(),.1f)) {SetActorLocation(Safe,false);GetCharacterMovement()->StopMovementImmediately();}
    }
    TickDownedState(DeltaTime);
    TickAugments(DeltaTime);
    if(HasAuthority()) { RefreshSkillHaste(); }
    if(HasAuthority() && GetCombatTime()>=NextCareerFlushAt) { NextCareerFlushAt=GetCombatTime()+30.f; FlushCareerProgress(); }
    if(HasAuthority() && (CombatHealth<=0.f || bDistrictFinished)) { GeneralProjectiles.Reset(); PendingCombatVfx.Reset(); }
    UpdateTentacleMeshes();
    UpdateGeneralSummonMeshes();
    UpdateSupercarVisual();
    UpdateWindWallVisual();
    UpdateXayahFeatherMeshes();
    if (HasAuthority() && !LevelChoices.IsEmpty() && CombatHealth <= 0.f)
    {
        LevelChoices.Reset();
        LevelChoiceDeadline = 0.f;
        EndLevelChoiceSelection();
    }
    if(HasAuthority() && CombatHealth>0.f && !bDistrictFinished && LevelChoices.IsEmpty()
        && (PendingAugmentRewards>0 || PendingAccessCards>0 || PendingLevelChoices>0)) { RollLevelChoices(); }
    if(HasAuthority() && !LevelChoices.IsEmpty() && !bChoicePresentationConfirmed && GetLevelChoiceClock()>=NextChoiceResendAt)
    { NextChoiceResendAt=GetLevelChoiceClock()+.75f;PublishLevelChoices(); }
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
            CombatAudio::PlayLocal(GetWorld(),bDistrictCleared?TEXT("Music/Victory"):TEXT("Music/Defeat"),.30f);
        }
        Super::Tick(DeltaTime);
        if (auto* Player = Cast<APlayerController>(GetController());
            IsLocallyControlled() && Player && Player->WasInputKeyJustPressed(EKeys::Escape))
        {
            ReturnToSoloMenu();
        }
        return;
    }
    if (bDistrictCamera && IsLocallyControlled() && GetNetMode()!=NM_DedicatedServer && GetCombatTime()>=NextMusicStateCheckAt)
    {
        NextMusicStateCheckAt=GetCombatTime()+.5f;
        const int32 Arena=FMath::Max(0,SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this,true)));
        bool Boss=false,SecondPhase=false;
        for(TActorIterator<ASwarmEnemy> It(GetWorld());It;++It)
        {if(It->Kind==3&&It->GetHealth()>0.f){Boss=true;SecondPhase|=It->IsSecondPhase();}}
        const FString Cue=CombatAudio::SelectMusicCue(Arena,GetCombatTime(),Boss,SecondPhase);
        if(!CombatMusic||CurrentMusicCue!=Cue)
        {
            if(USoundWave* Music=CombatAudio::Load(*Cue))
            {
                if(CombatMusic){CombatMusic->bAutoDestroy=true;CombatMusic->FadeOut(.9f,0.f);}
                Music->bLooping=true;
                CombatMusic=UGameplayStatics::SpawnSound2D(this,Music,.20f*CombatAudio::GetMusicVolume(),1.f,0.f,nullptr,false,false);
                if(CombatMusic){CombatMusic->FadeIn(.9f,1.f);CurrentMusicCue=Cue;}
            }
        }
    }
    if(CombatMusic){CombatMusic->SetVolumeMultiplier(.20f*CombatAudio::GetMusicVolume());}
    Super::Tick(DeltaTime);
    UpdateSkillVfx();
    if (HasAuthority() && RunRegen+PermanentRegen > 0.f && CombatHealth > 0.f && CombatHealth < MaxCombatHealth)
    {
        CombatHealth = FMath::Min(MaxCombatHealth, CombatHealth + (RunRegen+PermanentRegen) * DeltaTime);
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
    if(HasAuthority()) { GeneralWeaponCooldownEnds.SetNum(GeneralWeaponCount);for(int32 I=0;I<GeneralWeaponCount;++I) { GeneralWeaponCooldownEnds[I]=GeneralWeaponReadyAt[I]; } }
    const float PassiveFraction = GetJinxPassiveRemaining() / JinxPassiveDuration;
    const float RivenSpeedBonus = CharacterChoice == 6 ? .25f * RivenCharge / 100.f + (GetCombatTime() < RivenUltimateEnd ? .30f : 0.f) : 0.f;
    GetCharacterMovement()->MaxWalkSpeed = BaseMoveSpeed * FMath::Max(.1f, (1.f + GetAugmentMoveBonus() + (WorldBoonType==3&&GetWorldBoonRemaining()>0.f?1.5f:0.f) + (GetCombatTime() < LabSprintEnd ? .85f : 0.f) + RunMoveBonus + (GetCombatTime() < TrailBoostEnd ? .025f * TrailBoostStacks : 0.f) + (GetCombatTime() < AuroraVeilEnd ? .35f : 0.f) + RivenSpeedBonus + JinxInitialSpeedBonus * PassiveFraction + (GetCombatTime() < SeraphineShieldEnd ? .20f : 0.f) + (CharacterChoice == 4 ? 1.f - CombatHealth / FMath::Max(MaxCombatHealth, 1.f) : 0.f)));
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
    if (bMoving != bWasMoving && CombatMotionTime <= 0.f)
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
        if(ZoomSteps!=0) DesiredCameraDistance = FMath::Clamp(DesiredCameraDistance + ZoomSteps * DistrictZoomStep,
            DistrictClosestZoom, DistrictDefaultZoom);
        CameraArm->TargetArmLength = FMath::FInterpTo(CameraArm->TargetArmLength, DesiredCameraDistance,
            DeltaTime, 12.f);
    }

    if (Player->WasInputKeyJustPressed(EKeys::Escape)
        && SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this)) != INDEX_NONE)
    {
        if (auto* Hud=Cast<ACombatHUD>(Player->GetHUD())) { Hud->ToggleSettings(); }
        return;
    }
    if (auto* Hud=Cast<ACombatHUD>(Player->GetHUD()); Hud && Hud->IsSettingsOpen()) { return; }

    if(ClientPreferences::IsOpen()) { return; }
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
        return;
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

    if(bDistrictCamera && CombatHealth<=0.f)
    {
        if(Player->WasInputKeyJustPressed(EKeys::LeftMouseButton)) { UpdateSpectatorView(1); }
        else if(Player->WasInputKeyJustPressed(EKeys::RightMouseButton)) { UpdateSpectatorView(-1); }
        return;
    }
    if(bDistrictCamera && UGameplayStatics::GetGlobalTimeDilation(this)<.001f) { return; }
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
        DropYuumiCargo(Character);
    }
    CharacterChoices.Remove(Exiting);
    Super::Logout(Exiting);
}

#if WITH_EDITOR
void AAnimaPreviewCharacter::RunSkillVisualSmokeTest(int32 Index)
{
    if (!HasAuthority()) { return; }
    ApplyCharacterChoice(FMath::Clamp(Index, 0, 8));
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmAudioEventTimingTest"))){RunAudioEventTimingTest();return;}
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmOriginalAudioTest"))){RunOriginalAudioAcceptanceTest();return;}
    if(FParse::Param(FCommandLine::Get(),TEXT("ArenaLayoutReview")))
    {
        InvulnerableEnd=GetCombatTime()+30.f;JinxWeaponReadyAt=GetCombatTime()+30.f;
        SetActorLocation(FVector(0,0,130));DesiredCameraDistance=6000;CameraArm->TargetArmLength=6000;
        FTimerHandle Shot;GetWorldTimerManager().SetTimer(Shot,[](){FScreenshotRequest::RequestScreenshot(true);},1.f,false);return;
    }

    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmSettingsAcceptanceTest"))) { ClientPreferences::RunAcceptanceTest(GetWorld()); return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmSettingsAcceptanceTest"))) { ClientPreferences::RunAcceptanceTest(GetWorld()); return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmDensityAcceptanceTest"))) { if(auto* Mode=GetWorld()->GetAuthGameMode<AAnimaPreviewMode>()) { Mode->RunDensityAcceptanceTest(); } return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmEnemyMotionTest"))) { RunEnemyMotionTest(); return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmChampionMotionTest"))) { RunChampionMotionTest(); return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmLabTraversalTest"))) { RunLabTraversalTest(); return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmHealthMinimapReview"))) { RunHealthMinimapReview(); return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmPlayerFeedbackTest"))) { RunPlayerFeedbackTest(); return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmChampionMechanicsTest"))) { RunChampionMechanicsTest(); return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmBoomerangFlightTest"))) { RunBoomerangFlightTest(); return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmRenderStressTest"))) { StartRenderStressTest(); return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmAugmentPoolTest"))) { RunAugmentPoolTest(); return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmYuumiVariantsTest"))) { if(auto* Mode=GetWorld()->GetAuthGameMode<AAnimaPreviewMode>()) { Mode->RunYuumiVariantAcceptanceTest(); } return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmVacuumVisualReview")))
    {
        InvulnerableEnd=GetCombatTime()+60.f;JinxWeaponReadyAt=GetCombatTime()+60.f;
        for(TActorIterator<ASwarmExperienceOrb> It(GetWorld());It;++It) { It->Destroy(); }
        for(int32 I=0;I<48;++I)
        {
            const FVector Offset=FVector::ForwardVector.RotateAngleAxis(I*137.5f,FVector::UpVector)*(1500.f+(I%5)*250.f);
            ASwarmExperienceOrb::SpawnPickup(GetWorld(),GetActorLocation()+Offset,I%4==0?5:0);
        }
        ASwarmExperienceOrb::SpawnPickup(GetWorld(),GetActorLocation()+FVector(400,200,0),6);
        ASwarmExperienceOrb::SpawnPickup(GetWorld(),GetActorLocation()+FVector(-400,200,0),7);
        ASwarmExperienceOrb::SpawnPickup(GetWorld(),GetActorLocation()+FVector(0,-500,0),8);
        CombatLevel=30;CollectWorldPowerup(SwarmLoot::Magnet);
        FTimerHandle Capture;
        GetWorldTimerManager().SetTimer(Capture,FTimerDelegate::CreateWeakLambda(this,[this]()
        { if(auto* Player=Cast<APlayerController>(GetController())) { Player->ConsoleCommand(TEXT("Shot ShowUI"),true); } }),.45f,false);
        UE_LOG(LogTemp,Display,TEXT("Vacuum visual: stream=48 protected=3"));return;
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmTeamProgressTest"))) { RunTeamProgressTest(); return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmTeamChoiceNetworkHost"))) { StartTeamChoiceNetworkReview(); return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmPickupNetworkHost"))) { RunPickupNetworkHostTest(); return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmPickupRulesTest"))) { RunPickupRulesTest(); return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmWorldLootTest"))) { RunWorldLootAcceptanceTest(); return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmBoonSpireTest"))) { if(auto* Mode=GetWorld()->GetAuthGameMode<AAnimaPreviewMode>()) { Mode->RunBoonSpireAcceptanceTest(); } return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SupercarVisualReview")))
    {
        CollectWorldPowerup(11); UpdateSupercarVisual();
        GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[](){ FScreenshotRequest::RequestScreenshot(false); }));
        return;
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmPermanentUpgradeTest"))) { RunPermanentUpgradeTest(); return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmEncounterTuningTest"))) { RunEncounterTuningTest(); return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmGrowthNetworkHost")))
    {
        int32 Players=0;
        for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
        {
            if(!It->GetController()) { continue; }
            It->GrantAugmentReward(); It->LevelChoices={FirstAugmentChoice+15,FirstAugmentChoice+3,FirstAugmentChoice+25}; It->PublishLevelChoices();
            GetWorld()->SpawnActor<ASwarmExperienceOrb>(It->GetActorLocation()+FVector(100.f,0.f,-38.f),FRotator::ZeroRotator); ++Players;
        }
        UE_LOG(LogTemp,Display,TEXT("Growth network host: rewards=%d"),Players);
        return;
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmEnemyAttackMotionTest"))) { RunEnemyAttackMotionTest();return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmEquipmentTooltipTest"))) { RunEquipmentTooltipTest();return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmBossChallengeTest"))) { RunBossChallengeTest();return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmCoopRecoveryTest"))) { RunCoopRecoveryTest();return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmCombatEnduranceTest"))) { StartCombatEnduranceTest(); return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmAugmentAcceptanceTest"))) { RunAugmentAcceptanceTests(); return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmYuumiAcceptanceTest"))) { if(auto* Mode=GetWorld()->GetAuthGameMode<AAnimaPreviewMode>()) { Mode->RunYuumiAcceptanceTest(); } return; }
    if(FParse::Param(FCommandLine::Get(),TEXT("AugmentCardReview")) || FParse::Param(FCommandLine::Get(),TEXT("SignatureCardReview")))
    {
        GrantAugmentReward();
        if(FParse::Param(FCommandLine::Get(),TEXT("SignatureCardReview"))) { bAugmentRewardOpen=false;LevelChoices={0,SignatureEvolutionChoice,FirstGeneralWeaponChoice}; }
        else { LevelChoices={FirstAugmentChoice+3,FirstAugmentChoice+15,FirstAugmentChoice+25}; }
        GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[](){ FScreenshotRequest::RequestScreenshot(false); }));
        return;
    }
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
    if(FParse::Param(FCommandLine::Get(),TEXT("WeaponModelCombatReview")))
    {
        CombatLevel=6;InvulnerableEnd=JinxWeaponReadyAt=GetCombatTime()+120.f;
        for(TActorIterator<ASwarmEnemy> It(GetWorld());It;++It){It->Destroy();}
        for(int I=0;I<8;++I)
        {
            FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* E=GetWorld()->SpawnActor<ASwarmEnemy>(GetActorLocation()+FVector::ForwardVector.RotateAngleAxis(I*45.f,FVector::UpVector)*420.f,FRotator::ZeroRotator,P);
            if(E){E->Freeze(30.f);}
        }
        GeneralWeapons=Index==0?TArray<uint8>{1,2,4,6,7}:TArray<uint8>{11,17};
        GeneralWeaponRanks.Init(1,GeneralWeaponCount);for(int W=0;W<GeneralWeaponCount;++W){GeneralWeaponReadyAt[W]=0;}
        const bool Evolved=FParse::Param(FCommandLine::Get(),TEXT("EvolvedVfx"));
        if(Evolved){EvolvedGeneralWeapons=GeneralWeapons;for(uint8 W:GeneralWeapons){GeneralWeaponRanks[W]=5;}}
        static const TCHAR* Models[]={TEXT("Train"),TEXT("Boomerang"),TEXT("Crescent"),TEXT("Blade"),TEXT("Tibber"),TEXT("Crossbow"),TEXT("Cyclone")};
        for(const TCHAR* N:Models){const FString Path=FString::Printf(TEXT("/Game/TroubleSquad/CombatVisuals/GeneralWeapons/Models/SM_GW_%s.SM_GW_%s"),N,N);LoadObject<UStaticMesh>(nullptr,*Path);}
        if(auto* Renderer=GetCombatVfx())
        {
            const FVector Hidden=GetActorLocation()-FVector(0,0,10000);
            for(uint8 K=0;K<7;++K){Renderer->EmitModel(K,Hidden,Hidden,20,.05f,false);}
            Renderer->Clear();
        }
#if WITH_EDITOR
        FAssetCompilingManager::Get().FinishAllCompilation();if(GShaderCompilingManager){GShaderCompilingManager->FinishAllCompilation();}
#endif
        for(uint8 W:GeneralWeapons){FireGeneralWeapon(W,GetCombatTime());GeneralWeaponReadyAt[W]=GetCombatTime()+120.f;}
        FTimerHandle Shot,Check;GetWorldTimerManager().SetTimer(Shot,[](){FScreenshotRequest::RequestScreenshot(true);},.16f,false);
        GetWorldTimerManager().SetTimer(Check,FTimerDelegate::CreateWeakLambda(this,[this,Index,Evolved]()
        {
            const bool Pet=GeneralSummonMeshes.Num()>0&&GeneralSummonMeshes[0]&&GeneralSummonMeshes[0]->GetStaticMesh()&&GeneralSummonMeshes[0]->GetStaticMesh()->GetName()==TEXT("SM_GW_Tibber");
            UE_LOG(LogTemp,Display,TEXT("Weapon model combat: group=%d evolved=%d pet=%d models=%d"),Index,Evolved,Pet,CombatVfx?CombatVfx->GetActiveModelCount():0);
        }),1.f,false);return;
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("WeaponModelGallery")))
    {
        InvulnerableEnd=JinxWeaponReadyAt=GetCombatTime()+120.f;
        static const EGeneralWeapon Models[]={EGeneralWeapon::Train,EGeneralWeapon::Boomerang,EGeneralWeapon::Crescent,EGeneralWeapon::Blade,EGeneralWeapon::Tibber,EGeneralWeapon::Crossbow,EGeneralWeapon::Cyclone};
        for(int32 I=0;I<7;++I)
        {
            const FVector P=GetActorLocation()+FVector((I%4-1.5f)*280.f,(I/4-.5f)*350.f,0);
            ShowWeaponVfx(static_cast<uint8>(Models[I]),P,P+FVector(1,0,0),75.f,0,Index!=0);
        }
#if WITH_EDITOR
        FAssetCompilingManager::Get().FinishAllCompilation();
        if(GShaderCompilingManager){GShaderCompilingManager->FinishAllCompilation();}
#endif
        UE_LOG(LogTemp,Display,TEXT("Weapon model gallery: emitted=%d"),CombatVfx?CombatVfx->GetActiveModelCount():0);
        FTimerHandle Shot;GetWorldTimerManager().SetTimer(Shot,[](){FScreenshotRequest::RequestScreenshot(true);},.65f,false);return;
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("WeaponModelLifecycleTest")))
    {
        auto* Probe=GetCombatVfx();if(!Probe){return;}Probe->Clear();
        for(int I=0;I<84;++I){Probe->EmitModel(I%7,Aim,Aim+FVector(200,0,0),100,.2f,false);}
        const bool Budget=Probe->GetActiveModelCount()==48;
        FTimerHandle Check;GetWorldTimerManager().SetTimer(Check,FTimerDelegate::CreateWeakLambda(this,[Probe,Budget]()
        {
            UE_LOG(LogTemp,Display,TEXT("Weapon model lifetime: budget=%d expired=%d instancesCleared=%d"),Budget,IsValid(Probe)&&Probe->GetActiveModelCount()==0,IsValid(Probe)&&Probe->GetModelInstanceCount()==0);
            if(IsValid(Probe)){Probe->Destroy();}
        }),.8f,false);CombatVfx=nullptr;return;
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
        PendingLevelChoices = 0; LevelChoices.Reset(); EndLevelChoiceSelection();
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
        CombatLevel = 6;
        // Exercise real casts with both permanent and run haste enabled.
        CombatLevel=6;
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
        CombatLevel=6;
        InvulnerableEnd=GetCombatTime()+60.f;
        JinxWeaponReadyAt=GetCombatTime()+60.f;
        ServerCastCharacterSkill(FParse::Param(FCommandLine::Get(),TEXT("UltimateOnly")),Aim);
        const bool IsUltimate=FParse::Param(FCommandLine::Get(),TEXT("UltimateOnly"));
        const bool CastAccepted=IsUltimate?JinxRReadyAt>GetCombatTime():JinxEReadyAt>GetCombatTime()||ExtraChargeReadyAt>GetCombatTime();
        UE_LOG(LogTemp,Display,TEXT("Skill visual cast accepted=%d"),CastAccepted);
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

void AAnimaPreviewCharacter::SubmitPermanentRanks()
{
    if(!IsLocallyControlled() || SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this))==INDEX_NONE) { return; }
#if WITH_EDITOR
    int32 DiagnosticCharacter=0;
    if(FParse::Value(FCommandLine::Get(),TEXT("SwarmSkillVisual="),DiagnosticCharacter)) { return; }
#endif
    const auto* Progress=UDistrictProgress::Load();
    TArray<uint8> Ranks; Ranks.SetNumZeroed(15);
    for(int32 I=0;I<15;++I) { Ranks[I]=FMath::Clamp(Progress->GetUpgradeRank(I),0,UDistrictProgress::UpgradeCap(I)); }
    ServerApplyPermanentRanks(Ranks);
    ServerSetAugmentUnlocks(Progress->HasClearedArena(3),Progress->ExtremeClearedArenaMask!=0);
}

void AAnimaPreviewCharacter::ServerApplyPermanentRanks_Implementation(const TArray<uint8>& Ranks)
{
    if(bPermanentRanksApplied || !bDistrictCamera || bDistrictFinished || Ranks.Num()!=15) { return; }
    for(int32 I=0;I<15;++I) { if(Ranks[I]>UDistrictProgress::UpgradeCap(I)) { return; } }
    bPermanentRanksApplied=true;
    const auto& Stats=ChampionBaseStats[FMath::Clamp(CharacterChoice,0,8)];
    const float Fraction=FMath::Clamp(CombatHealth/FMath::Max(1.f,MaxCombatHealth),0.f,1.f);
    MaxCombatHealth=(Stats.Health*(1.f+.1f*Ranks[2])+RunHealthBonus)*GetAugmentHealthMultiplier();
    CombatHealth=MaxCombatHealth*Fraction;
    BaseArmor=Stats.Armor+5.f*Ranks[1];
    BaseMoveSpeed=Stats.MoveSpeed*(1.f+.05f*Ranks[4]+.01f*Ranks[14]);
    PermanentDamageMultiplier=1.f+.1f*Ranks[0]+.01f*Ranks[14];
    PermanentAbilityHaste=5.f*Ranks[9]+Ranks[14];
    PermanentDurationMultiplier=1.f+.05f*Ranks[7];
    PermanentAreaMultiplier=1.f+.05f*Ranks[6];
    PermanentExperienceMultiplier=1.f+.05f*Ranks[10];
    PermanentRegen=3.f*Ranks[3]; PermanentCritChance=.05f*Ranks[8]; PermanentProjectiles=Ranks[11];
    PermanentPickupMultiplier=1.f+.25f*Ranks[5]+.01f*Ranks[14];
    PermanentGoldMultiplier=1.f+.15f*Ranks[12]; PermanentBoonMultiplier=1.f+.25f*Ranks[13];
    ForceNetUpdate();
}

void AAnimaPreviewCharacter::FlushCareerProgress()
{
    if(!HasAuthority() || !bDistrictCamera) { return; }
    if(CareerOrbs||CareerHeals||CareerCrits||CareerSkills)
    {
        ClientRecordCareer(CareerOrbs,CareerHeals,CareerCrits,CareerSkills,CharacterChoice,CombatLevel);
        CareerOrbs=CareerHeals=CareerCrits=CareerSkills=0;
    }
}

void AAnimaPreviewCharacter::ClientRecordCareer_Implementation(int32 Orbs,int32 Heals,int32 Crits,int32 Skills,int32 Champion,int32 Level)
{
#if WITH_EDITOR
    int32 DiagnosticCharacter=0;
    if(FParse::Value(FCommandLine::Get(),TEXT("SwarmSkillVisual="),DiagnosticCharacter)
        || FParse::Param(FCommandLine::Get(),TEXT("SwarmGrowthNetworkReview"))) { return; }
#endif
    UDistrictProgress::RecordCareer(Orbs,Heals,Crits,Skills,Champion,Level);
}

void AAnimaPreviewCharacter::RefreshSkillHaste()
{
    const float Now=GetCombatTime(), Normal=GetCombatCooldown(1.f), Ultimate=GetUltimateCooldown(1.f);
    const auto Rescale=[Now](float& Deadline,float Before,float After)
    { if(Before>0.f && Deadline>Now) { Deadline=Now+(Deadline-Now)*After/Before; } };
    Rescale(JinxEReadyAt,PreviousSkillCooldownFactor,Normal);
    Rescale(ExtraChargeReadyAt,PreviousSkillCooldownFactor,Normal);
    Rescale(JinxRReadyAt,PreviousUltimateCooldownFactor,Ultimate);
    PreviousSkillCooldownFactor=Normal; PreviousUltimateCooldownFactor=Ultimate;
}

FString AAnimaPreviewCharacter::GetSkillTooltip(bool U) const
{
 static const TCHAR* E[]={TEXT("생선 대가리"),TEXT("소리의 장막"),TEXT("일식"),TEXT("촉수 강타"),TEXT("물고기의 광분 / 생존의 절규"),TEXT("질풍검"),TEXT("용맹"),TEXT("장막 너머로"),TEXT("깃부르미")};
 static const TCHAR* R[]={TEXT("초강력 초토화 로켓"),TEXT("앙코르"),TEXT("흑점 폭발"),TEXT("믿음의 도약"),TEXT("급강하"),TEXT("바람 장막"),TEXT("추방자의 검"),TEXT("세계의 경계"),TEXT("칼날 폭풍")};
 static const TCHAR* ED[]={TEXT("커서 방향으로 여러 로켓을 발사해 폭발 피해를 줍니다."),TEXT("자신과 가까운 아군에게 보호막과 이동 속도를 부여합니다."),TEXT("보호막과 방어력을 얻고 주변 적에게 피해를 줍니다."),TEXT("지정 방향을 강타해 적을 띄우고 끝에 촉수를 생성합니다. 2회 충전."),TEXT("짧게 돌진해 광분합니다. 피해 감소와 회복을 얻습니다. 재사용하면 종료합니다."),TEXT("보호막을 얻고 무적 돌진 후 주변을 벱니다. 3회 충전."),TEXT("현재 바라보는 방향으로 무적 돌진하며 주변 적을 기절시킵니다."),TEXT("무적 상태로 이동하며 닿은 적에게 피해와 저주를 적용합니다."),TEXT("모든 깃털을 회수합니다. 회수 경로의 적을 타격하며 잠시 습득 반경이 증가합니다.")};
 static const TCHAR* RD[]={TEXT("커서 위치로 대형 로켓을 발사해 폭발 피해를 줍니다."),TEXT("주변 적을 타격하고 가까운 아군을 잠시 무적으로 만듭니다."),TEXT("커서 위치에 광역 피해를 주고 태양 영역을 생성합니다."),TEXT("지면을 내려쳐 피해를 주고 다수의 촉수를 생성합니다. 투사체 수에 영향을 받습니다."),TEXT("커서 위치로 급강하해 피해를 주고 강화된 광분에 진입합니다."),TEXT("적과 적의 투사체를 막는 바람 장막을 설치합니다. 최종 보스는 막지 못합니다."),TEXT("이동 속도가 증가하며 돌진할 때마다 바람 가르기를 발사합니다."),TEXT("영역을 펼치고 경계를 통해 반대편으로 이동합니다. 재사용하면 해제합니다."),TEXT("무적으로 도약해 원형으로 깃털을 뿌리고 E의 쿨타임을 초기화합니다.")};
 static const float EC[]={8,16,17,12,20,10,8,15,13},RC[]={50,90,80,90,120,25,100,90,90};
 const int32 I=FMath::Clamp(CharacterChoice,0,8);
 FString Result=FString::Printf(TEXT("%s · %s\n%s\n쿨타임 %.2f초%s"),U?TEXT("R"):TEXT("E"),U?R[I]:E[I],U?RD[I]:ED[I],U?GetUltimateCooldown(RC[I]):GetCombatCooldown(EC[I]),!U&&(I==3||I==5)?TEXT(" / 충전"):TEXT(""));
 if(U&&CombatLevel<6){Result+=TEXT("\n6레벨에 해금됩니다.");}
 if(I==1&&!U){Result+=FString::Printf(TEXT("\n보호막 %.0f"),100.f+.25f*MaxCombatHealth);}
 else if(I==2&&!U){Result+=FString::Printf(TEXT("\n보호막 %.0f"),.25f*MaxCombatHealth);}
 else if(I==5&&!U){Result+=FString::Printf(TEXT("\n보호막 %.0f"),150.f+.1f*MaxCombatHealth);}
 else if((I==4&&!U)||((I==5||I==6)&&U)||I==8){Result+=TEXT("\n피해: 무기 및 투사체 적중에 따라 적용");}
 else {float Base=I==0?(U?JinxRDamage:JinxEDamage):I==1?160.f:I==2?160.f:I==3?(U?250.f:145.f):I==4?(U?220.f:90.f):I==5?135.f:I==6?110.f:I==7?(U?180.f:95.f):0.f;Result+=FString::Printf(TEXT("\n피해량 %.0f (대상별 추가 효과 제외)"),Base*GetCombatDamageMultiplier());}
 return Result;
}

void AAnimaPreviewCharacter::UpdateSpectatorView(int32 Direction)
{
    auto* PC=Cast<APlayerController>(GetController());
    if(!PC||!PC->IsLocalController()) { return; }
    if(CombatHealth>0.f) { if(SpectatorTarget.IsValid()||PC->GetViewTarget()!=this) { PC->SetViewTargetWithBlend(this,.2f); } SpectatorTarget.Reset();return; }
    TArray<AAnimaPreviewCharacter*> Alive;
    for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
    { if(*It!=this&&(It->GetPlayerState()||It->GetController())&&It->CombatHealth>0.f&&!It->bDistrictFinished) { Alive.Add(*It); } }
    Alive.Sort([](const AAnimaPreviewCharacter& A,const AAnimaPreviewCharacter& B){return A.GetName()<B.GetName();});
    int32 Index=Alive.IndexOfByKey(SpectatorTarget.Get());
    if(Index!=INDEX_NONE&&Direction==0) { return; }
    if(Alive.IsEmpty()) { SpectatorTarget.Reset();PC->SetViewTarget(this);return; }
    Index=Index==INDEX_NONE?0:(Index+Direction+Alive.Num())%Alive.Num();
    SpectatorTarget=Alive[Index];PC->SetViewTargetWithBlend(Alive[Index],.2f);
}
void AAnimaPreviewCharacter::TickDownedState(float DeltaTime)
{
    if(!bDistrictCamera) { return; }
    if(CombatHealth<=0.f)
    {
        if(!bDeathMovementStopped)
        {
            GetCharacterMovement()->StopMovementImmediately();GetCharacterMovement()->DisableMovement();
            GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
            CombatMotionTime=0.f;bRivenDashing=false;GetMesh()->PlayAnimation(Idle.Get(),true);bDeathMovementStopped=true;
            if(HasAuthority()) { ReviveWindowEndsAt=GetCombatTime()+30.f;ReviveProgress=0.f;bReviveExpired=false;ForceNetUpdate(); }
        }
        if(HasAuthority()&&!bDistrictFinished&&!bReviveExpired)
        {
            if(GetCombatTime()>=ReviveWindowEndsAt) { bReviveExpired=true;ReviveProgress=0.f;ForceNetUpdate(); }
            else
            {
                bool Nearby=false;
                for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
                { if(*It!=this&&It->GetController()&&It->CombatHealth>0.f&&!It->bDistrictFinished&&FVector::DistSquared2D(GetActorLocation(),It->GetActorLocation())<=FMath::Square(ReviveRadius)) { Nearby=true;break; } }
                ReviveProgress=Nearby?FMath::Min(ReviveChannelSeconds,ReviveProgress+DeltaTime):0.f;
                if(ReviveProgress>=ReviveChannelSeconds) { CombatHealth=MaxCombatHealth*.3f;InvulnerableEnd=GetCombatTime()+3.f;ReviveWindowEndsAt=0.f;ReviveProgress=0.f;ForceNetUpdate(); }
            }
        }
        if(GetNetMode()!=NM_DedicatedServer&&!bReviveExpired&&!bDistrictFinished)
        {
            const FVector Center=GetActorLocation()-FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-5.f);
            DrawDebugCircle(GetWorld(),Center,ReviveRadius,64,FColor::Cyan,false,-1.f,0,3.f,FVector::ForwardVector,FVector::RightVector,false);
            if(ReviveProgress>0.f) { DrawDebugCircle(GetWorld(),Center+FVector(0,0,2),ReviveRadius*ReviveProgress/ReviveChannelSeconds,48,FColor::Green,false,-1.f,0,3.f,FVector::ForwardVector,FVector::RightVector,false); }
        }
    }
    if(CombatHealth>0.f&&bDeathMovementStopped)
    {
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);bDeathMovementStopped=false;
        if(HasAuthority()) { ReviveProgress=0.f;ReviveWindowEndsAt=0.f;bReviveExpired=false; }
    }
    if(IsLocallyControlled()) { UpdateSpectatorView(); }
}
