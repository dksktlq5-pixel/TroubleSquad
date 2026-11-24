#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Styling/SlateBrush.h"
#include "SoloMenu.generated.h"

class SWidget;
class UTexture2D;

struct FLobbyPartyMemberView
{
 uint64 Id=0;FString Name;int32 Character=INDEX_NONE;bool bReady=false,bHost=false;
};

UCLASS()
class ASoloMenuController : public APlayerController
{
    GENERATED_BODY()

protected:
    void BeginPlay() override;
    void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void ShowPage(bool bLobby);
    void ShowSelection(bool bMaps);
    void ShowStartup();
    FTimerHandle StartupTimer;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> LogoTexture;
    uint8 ActiveDrawer = 0;
    bool bSettingsOpen = false;
    bool bDisplaySettings = false;
    void ShowUpgrades();
    void ShowReadyCheck();
    void BeginSoloTravel();
    void HandleConnectionFailure(UWorld* World, UNetDriver* Driver,
        ENetworkFailure::Type Failure, const FString& Error);
    FTimerHandle TravelTimer;
    FDelegateHandle NetworkFailureHandle;
    void RequestSteamRoom(bool bCreate);
    void PollSteamRoom();
    void WatchSteamRoom();
    void RefreshPartyMembers();
    TArray<FLobbyPartyMemberView> PartyMembers;
    UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> PartyPortraitTextures;
    TArray<FSlateBrush> PartyPortraitBrushes;
    bool bPartyRosterReview=false;
    bool CanStartSteamRoom() const;
    void StartSteamMatch();
    FTimerHandle SteamRoomWatchTimer;
    double SteamRoomRequestAt = 0.0;
    double SteamHostWaitAt = 0.0;
    uint64 SteamRoomOwner = 0;
    int32 PublishedCharacter = INDEX_NONE;
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
    int32 SelectedArena = 0;
    int32 SelectedDifficulty = 0;
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
