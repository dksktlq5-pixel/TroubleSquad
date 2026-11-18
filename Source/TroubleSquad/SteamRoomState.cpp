#include "SteamRoomState.h"
#include "SwarmArenas.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Kismet/GameplayStatics.h"
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

void USteamRoomState::Adopt(uint64 Lobby, uint64 Owner)
{
    if (LobbyId && LobbyId != Lobby) { Leave(); }
    LobbyId = Lobby; OwnerId = Owner;
    bWaitingForHost = bEnteredMatch = false;
}
void USteamRoomState::Leave()
{
    if (LobbyId && SteamMatchmaking()) { SteamMatchmaking()->LeaveLobby(CSteamID(LobbyId)); }
    LobbyId = OwnerId = 0;
    bWaitingForHost = bEnteredMatch = false;
}
void USteamRoomState::Deinitialize() { Leave(); Super::Deinitialize(); }
void USteamRoomState::StartHost()
{
    bWaitingForHost = true;
    HostDeadline = FPlatformTime::Seconds() + 45.0;
}
void USteamRoomState::Tick(float)
{
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (!World || !SteamMatchmaking() || !SteamUser()) { return; }
    const bool bMatch = SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(World)) != INDEX_NONE;
    if (bMatch) { bEnteredMatch = true; }
    else if (bEnteredMatch) { Leave(); return; }
    if (!bWaitingForHost) { return; }
    const bool bOwnsLobby = SteamMatchmaking()->GetLobbyOwner(CSteamID(LobbyId)).ConvertToUint64() == SteamUser()->GetSteamID().ConvertToUint64();
    if (!bOwnsLobby) { bWaitingForHost = false; return; }
    UNetDriver* Driver = World->GetNetDriver();
    if (bMatch && World->GetNetMode() == NM_ListenServer && Driver && Driver->IsNetResourceValid()
        && Driver->GetClass()->GetName() == TEXT("SteamSocketsNetDriver"))
    {
        // The listen-server SteamSockets interface uses the logged-in user's P2P identity.
        const FString Host = FString::Printf(TEXT("%llu"), SteamUser()->GetSteamID().ConvertToUint64());
        SteamMatchmaking()->SetLobbyData(CSteamID(LobbyId), "host_id", TCHAR_TO_UTF8(*Host));
        if (SteamMatchmaking()->SetLobbyData(CSteamID(LobbyId), "state", "ready"))
        {
            bWaitingForHost = false;
            UE_LOG(LogTemp, Display, TEXT("Room-code host listener ready (SteamSockets)"));
        }
    }
    else if (FPlatformTime::Seconds() > HostDeadline)
    {
        SteamMatchmaking()->SetLobbyData(CSteamID(LobbyId), "state", "failed");
        bWaitingForHost = false;
        UE_LOG(LogTemp, Error, TEXT("Room-code host did not establish a SteamSockets listener"));
    }
}
