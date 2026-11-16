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
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    void SelectCharacter(int32 Index);

    UPROPERTY() TObjectPtr<USpringArmComponent> CameraArm;
    UPROPERTY() TObjectPtr<UCameraComponent> Camera;
    UPROPERTY() TObjectPtr<UAnimSequence> Idle;
    UPROPERTY() TObjectPtr<UAnimSequence> Run;
    int32 SelectedIndex = INDEX_NONE;
    bool bWasMoving = false;
};

UCLASS()
class TROUBLESQUAD_API AAnimaPreviewMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AAnimaPreviewMode();
};

