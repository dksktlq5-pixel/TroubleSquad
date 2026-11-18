#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "SteamRoomState.generated.h"

// Keeps Steam lobby membership alive while the menu controller is replaced during travel.
UCLASS()
class USteamRoomState : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
public:
    void Adopt(uint64 Lobby, uint64 Owner);
    void Leave();
    void StartHost();
    void Tick(float DeltaTime) override;
    TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(USteamRoomState, STATGROUP_Tickables); }
    bool IsTickable() const override { return !IsTemplate() && LobbyId != 0; }
    void Deinitialize() override;
    uint64 LobbyId = 0;
    uint64 OwnerId = 0;
private:
    bool bWaitingForHost = false;
    bool bEnteredMatch = false;
    double HostDeadline = 0.0;
};
