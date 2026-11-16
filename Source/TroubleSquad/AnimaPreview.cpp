#include "AnimaPreview.h"

#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

namespace
{
constexpr float DistrictDefaultZoom = 1025.f;
constexpr float DistrictClosestZoom = 725.f;
constexpr float DistrictZoomStep = 100.f;
constexpr float DistrictCameraYaw = -90.f;

const TCHAR* const Roster[] = {TEXT("Jinx"), TEXT("Seraphine"), TEXT("Leona"),
    TEXT("Illaoi"), TEXT("Briar"), TEXT("Yasuo"), TEXT("Riven"), TEXT("Aurora"), TEXT("Xayah")};
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
        DesiredCameraDistance = DistrictDefaultZoom;
        CameraArm->TargetArmLength = DesiredCameraDistance;
        CameraArm->SetRelativeRotation(FRotator(-61.f, DistrictCameraYaw, 0.f));
    }

    int32 CharacterIndex = 0;
    if (const AGameModeBase* Mode = UGameplayStatics::GetGameMode(this))
    {
        CharacterIndex = FMath::Clamp(UGameplayStatics::GetIntOption(Mode->OptionsString,
            TEXT("Character"), 0), 0, static_cast<int32>(UE_ARRAY_COUNT(Roster)) - 1);
    }
    SelectCharacter(CharacterIndex);
}

void AAnimaPreviewCharacter::PawnClientRestart()
{
    Super::PawnClientRestart();

    if (APlayerController* Player = Cast<APlayerController>(GetController());
        Player && Player->IsLocalController())
    {
        Player->bShowMouseCursor = true;
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
    GetMesh()->PlayAnimation(Idle, true);
    SelectedIndex = Index;
    bWasMoving = false;
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(27, 4.f, FColor::Cyan,
            FString::Printf(TEXT("%s   |   WASD move   |   Mouse aim   |   1-9 switch"), *Name));
    }
}

void AAnimaPreviewCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

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
            SelectCharacter(Index);
        }
    }

    const FVector Direction(
        static_cast<int32>(Player->IsInputKeyDown(EKeys::W)) - static_cast<int32>(Player->IsInputKeyDown(EKeys::S)),
        static_cast<int32>(Player->IsInputKeyDown(EKeys::D)) - static_cast<int32>(Player->IsInputKeyDown(EKeys::A)),
        0.f);
    if (!Direction.IsNearlyZero())
    {
        const FRotator ViewYaw(0.f, CameraArm->GetComponentRotation().Yaw, 0.f);
        AddMovementInput(ViewYaw.RotateVector(Direction.GetSafeNormal()));
    }
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
                SetActorRotation(FlatAim.Rotation());
            }
        }
    }

    const bool bMoving = GetVelocity().SizeSquared2D() > 25.f;
    if (bMoving != bWasMoving)
    {
        GetMesh()->PlayAnimation(bMoving ? Run.Get() : Idle.Get(), true);
        bWasMoving = bMoving;
    }
}

AAnimaPreviewMode::AAnimaPreviewMode()
{
    DefaultPawnClass = AAnimaPreviewCharacter::StaticClass();
}
