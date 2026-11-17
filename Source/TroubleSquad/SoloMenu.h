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
    void ShowUpgrades();
    void ShowReadyCheck();
    void BeginSoloTravel();
    void HandleConnectionFailure(UWorld* World, UNetDriver* Driver,
        ENetworkFailure::Type Failure, const FString& Error);
    FTimerHandle TravelTimer;
    FDelegateHandle NetworkFailureHandle;
    void RequestSteamRoom(bool bCreate);
    void PollSteamRoom();
    void LeaveSteamRoom();
    FText GetSteamRoomRoster() const;
    FTimerHandle SteamRoomTimer;
    uint64 SteamRoom = 0;
    uint64 SteamRoomRequest = 0;
    int32 SteamRoomRequestType = 0;
    FString RoomCode;
    FString EnteredCode;
    bool bSteamReady = false;
    FString PendingJoinAddress;
    FText JoinStatus;
    int32 RoomCapacity = 1;
    TSharedPtr<SWidget> Menu;
    FSlateBrush PortraitBrush;
    FSlateBrush DistrictBrush;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> PortraitTexture;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> DistrictTexture;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> SplashTexture;
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
