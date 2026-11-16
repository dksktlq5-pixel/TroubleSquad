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

    if (UGameplayStatics::GetCurrentLevelName(this).StartsWith(TEXT("L_WarehouseDistrict")))
    {
        // Keep roughly 28 meters of ground across the center of the Swarm camera.
        CameraArm->TargetArmLength = 1400.f;
        CameraArm->SetRelativeRotation(FRotator(-61.f, 0.f, 0.f));
    }

    SelectCharacter(0);
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
    GetMesh()->PlayAnimation(Idle, true);
    SelectedIndex = Index;
    bWasMoving = false;
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(27, 4.f, FColor::Cyan,
            FString::Printf(TEXT("%s   |   WASD move   |   1-9 switch"), *Name));
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
        AddMovementInput(Direction.GetSafeNormal());
        SetActorRotation(Direction.Rotation());
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



