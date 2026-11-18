#pragma once

#include "GameFramework/SaveGame.h"
#include "DistrictProgress.generated.h"

UCLASS()
class UDistrictProgress : public USaveGame
{
    GENERATED_BODY()

public:
    static UDistrictProgress* Load();
    static void Record(int32 Level, int32 ClearedArena, int32 EarnedGold, int32 MaximumHealth);

    static int32 UpgradeCap(int32 Entry);
    static int32 UpgradePrice(int32 Rank);
    static bool PurchaseUpgrade(int32 Entry);
    int32 GetUpgradeRank(int32 Entry) const { return UpgradeRanks.IsValidIndex(Entry) ? UpgradeRanks[Entry] : 0; }

    UPROPERTY(SaveGame) TArray<int32> UpgradeRanks;
    bool IsUpgradeUnlocked(int32 Entry) const;
    UPROPERTY(SaveGame) int32 HighestMaxHealth = 0;
    UPROPERTY(SaveGame) int32 GoldBalance = 0;
    UPROPERTY(SaveGame) int32 HighestLevel = 0;
    bool HasClearedArena(int32 Arena) const;
    UPROPERTY(SaveGame) int32 ClearedArenaMask = 0;
    UPROPERTY(SaveGame) bool bBossDefeated = false;
};
