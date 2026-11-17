#include "AnimaPreview.h"
#include "CombatHUD.h"
#include "CombatHealth.h"

#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"
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
    Super::BeginPlay();

    bDistrictCamera = UGameplayStatics::GetCurrentLevelName(this) == TEXT("L_CoastalResearch");
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
        JinxWeaponReadyAt = 0.f;
        JinxPassiveEnd = 0.f;
        JinxEReadyAt = JinxRReadyAt = 0.f;
        LeonaZones.Reset();
        LeonaShield = LeonaEclipseEnd = 0.f;
        CombatHazards.Reset();
        XayahFeathers.Reset();
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
    if (HasAuthority())
    {
        const float HealthFraction = SelectedIndex == INDEX_NONE
            ? 1.f : FMath::Clamp(CombatHealth / FMath::Max(1.f, MaxCombatHealth), 0.f, 1.f);
        CombatHealth = Stats.Health * HealthFraction;
    }
    MaxCombatHealth = Stats.Health;
    BaseArmor = Stats.Armor;
    BaseMoveSpeed = Stats.MoveSpeed;

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
    if (!HasAuthority() || IsSignatureWeaponEvolved())
    {
        return false;
    }
    ++SignatureWeaponLevel;
    return true;
}

void AAnimaPreviewCharacter::FireJinxWeapon(float Now)
{
    const float Cooldown = IsSignatureWeaponEvolved() ? 1.25f : 2.5f;
    const float HasteMultiplier = GetJinxPassiveRemaining() > 0.f ? 2.5f : 1.f;
    JinxWeaponReadyAt = Now + Cooldown / HasteMultiplier;
    const int32 BulletCount = SignatureWeaponLevel + 2;
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

void AAnimaPreviewCharacter::ShowJinxTracer_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End)
{
    if (GetNetMode() != NM_DedicatedServer)
    {
        DrawDebugLine(GetWorld(), Start, End, FColor(221, 66, 182), false, .10f, 0, 2.f);
    }
}

float AAnimaPreviewCharacter::GetJinxSkillRemaining(bool bUltimate) const
{
    return FMath::Max(0.f, (bUltimate ? JinxRReadyAt : JinxEReadyAt) - GetCombatTime());
}

void AAnimaPreviewCharacter::ServerCastCharacterSkill_Implementation(bool bUltimate, FVector_NetQuantize Aim)
{
    if (CharacterChoice > 8 || !bDistrictCamera || Aim.ContainsNaN())
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

    ReadyAt = Now + (bUltimate ? 50.f : 8.f) / (GetJinxPassiveRemaining() > 0.f ? 2.5f : 1.f);
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
        ShowJinxTracer(Rocket.Position, End);
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

void AAnimaPreviewCharacter::ShowJinxExplosion_Implementation(FVector_NetQuantize Position, float Radius, bool bUltimate)
{
    if (GetNetMode() != NM_DedicatedServer)
    {
        DrawDebugSphere(GetWorld(), Position, Radius, 20,
            bUltimate ? FColor::Orange : FColor::Magenta, false, .24f, 0, 3.f);
    }
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
    if ((CharacterChoice == 6 || CharacterChoice == 8) && GetCombatTime() < RivenShieldEnd)
    {
        const float Absorbed = FMath::Min(Damage, RivenShield);
        Damage -= Absorbed;
        RivenShield -= Absorbed;
    }
    const float Applied = FMath::Min(CombatHealth, Damage);
    CombatHealth -= Applied;
    return Applied > 0.f ? Super::TakeDamage(Applied, Event, InstigatorController, Causer) : 0.f;
}

void AAnimaPreviewCharacter::CastSeraphineSkill(bool bUltimate, float Now)
{
    const float Radius = bUltimate ? 1050.f : 575.f;
    float& ReadyAt = bUltimate ? JinxRReadyAt : JinxEReadyAt;
    ReadyAt = Now + (bUltimate ? 90.f : 16.f);
    const FVector Origin = GetActorLocation();
    for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
    {
        if (!It->GetController() || FVector::DistSquared2D(Origin, It->GetActorLocation()) > FMath::Square(Radius))
        {
            continue;
        }
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
    JinxWeaponReadyAt = Now + 3.f - .25f * (SignatureWeaponLevel - 1);
    const bool bEvolved = IsSignatureWeaponEvolved();
    const int32 Count = bEvolved ? 6 : SignatureWeaponLevel;
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
        if (GetNetMode() != NM_DedicatedServer)
        {
            DrawDebugLine(GetWorld(), Wave.Position, Next, FColor::Cyan, false, .08f, 0, 2.f);
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

void AAnimaPreviewCharacter::ShowSeraphineEffect_Implementation(FVector_NetQuantize Center, float Radius, bool bUltimate)
{
    if (GetNetMode() != NM_DedicatedServer)
    {
        DrawDebugSphere(GetWorld(), Center, Radius, 24, bUltimate ? FColor::Purple : FColor::Cyan,
            false, .4f, 0, 2.f);
    }
}

float AAnimaPreviewCharacter::GetCombatArmor() const
{
    return CharacterChoice == 2 ? GetLeonaArmor(GetCombatTime()) : BaseArmor;
}

float AAnimaPreviewCharacter::GetLeonaArmor(float Now) const
{
    return FMath::Max(0.f, BaseArmor + LeonaBonusArmor) * (Now < LeonaEclipseEnd ? 2.f : 1.f);
}

float AAnimaPreviewCharacter::GetLeonaAreaMultiplier() const
{
    const float BonusPercent = .30f * LeonaBonusArmor + .75f * LeonaBonusRegen
        + .02f * LeonaHealthFromStatPassives;
    return 1.f + .01f * FMath::Max(0.f, BonusPercent);
}

float AAnimaPreviewCharacter::ConsumeSunlight(AActor* Target) const
{
    return Target->Tags.Remove(SunlightTag) > 0 ? 300.f : 0.f;
}

void AAnimaPreviewCharacter::FireLeonaWeapon(float Now)
{
    const bool bEvolved = IsSignatureWeaponEvolved();
    JinxWeaponReadyAt = Now + (bEvolved ? 1.5f : 2.75f - .25f * (SignatureWeaponLevel - 1));
    const float Radius = (bEvolved ? 850.f : 550.f) * GetLeonaAreaMultiplier();
    const float Damage = (bEvolved ? 175.f : 25.f + 25.f * SignatureWeaponLevel) + 3.f * GetLeonaArmor(Now);
    DamageLeonaArea(GetActorLocation(), Radius, Damage, false, true);
    ShowSeraphineEffect(GetActorLocation(), Radius, false);
}

void AAnimaPreviewCharacter::CastLeonaSkill(bool bUltimate, float Now, FVector Aim)
{
    if (bUltimate)
    {
        JinxRReadyAt = Now + 80.f;
        Aim.Z = GetActorLocation().Z;
        const float Radius = 520.f * GetLeonaAreaMultiplier();
        DamageLeonaArea(Aim, Radius, 160.f, true, false);
        LeonaZones.Add({Aim, Radius, Now + .5f, Now + 4.f, false});
        ShowSeraphineEffect(Aim, Radius, true);
        return;
    }

    JinxEReadyAt = Now + 17.f;
    LeonaEclipseEnd = Now + 5.f;
    LeonaShield = .25f * MaxCombatHealth;
    const float Radius = 350.f * GetLeonaAreaMultiplier();
    LeonaZones.Add({GetActorLocation(), Radius, Now + .5f, Now + 3.f, true});
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
            FVector::DotProduct(GetActorForwardVector(), (Target->GetActorLocation() - Position).GetSafeNormal2D()) < .5f)
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

void AAnimaPreviewCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
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
    const float PassiveFraction = GetJinxPassiveRemaining() / JinxPassiveDuration;
    const float RivenSpeedBonus = CharacterChoice == 6 ? .25f * RivenCharge / 100.f + (GetCombatTime() < RivenUltimateEnd ? .30f : 0.f) : 0.f;
    GetCharacterMovement()->MaxWalkSpeed = BaseMoveSpeed * (1.f + (GetCombatTime() < AuroraVeilEnd ? .35f : 0.f) + RivenSpeedBonus + JinxInitialSpeedBonus * PassiveFraction + (GetCombatTime() < SeraphineShieldEnd ? .20f : 0.f) + (CharacterChoice == 4 ? 1.f - CombatHealth / FMath::Max(MaxCombatHealth, 1.f) : 0.f));
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
        && UGameplayStatics::GetCurrentLevelName(this) == TEXT("L_CoastalResearch"))
    {
        UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/TroubleSquad/Maps/L_SoloMenu")));
        return;
    }

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
    CharacterChoices.Remove(Exiting);
    Super::Logout(Exiting);
}
