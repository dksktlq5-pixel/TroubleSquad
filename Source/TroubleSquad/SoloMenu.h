#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Styling/SlateBrush.h"
#include "SoloMenu.generated.h"

class SWidget;
class UTexture2D;

UCLASS()
class ASoloMenuController : public APlayerController
{
    GENERATED_BODY()

protected:
    void BeginPlay() override;
    void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void ShowPage(bool bLobby);
    TSharedPtr<SWidget> Menu;
    FSlateBrush PortraitBrush;
    FSlateBrush DistrictBrush;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> PortraitTexture;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> DistrictTexture;
    int32 SelectedCharacter = 0;
    bool bTraveling = false;
};

UCLASS()
class ASoloMenuMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ASoloMenuMode();
};
