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
    static void RecordCareer(int32 ExperienceOrbs,int32 HealthPacks,int32 CriticalHits,int32 Skills,int32 Champion,int32 Level);
    static void RecordWorldProgress(int32 Bombs,bool bSupercar,int32 Boons,bool bHardAatrox);
    static void RecordDifficultyClear(int32 Arena,int32 Difficulty);
    int32 GetUpgradeRank(int32 Entry) const { return UpgradeRanks.IsValidIndex(Entry) ? FMath::Clamp(UpgradeRanks[Entry],0,UpgradeCap(Entry)) : 0; }

    UPROPERTY(SaveGame) TArray<int32> UpgradeRanks;
    bool IsUpgradeUnlocked(int32 Entry) const;
    UPROPERTY(SaveGame) int32 HighestMaxHealth = 0;
    UPROPERTY(SaveGame) int32 GoldBalance = 0;
    UPROPERTY(SaveGame) int32 HighestLevel = 0;
    bool HasClearedArena(int32 Arena) const;
    UPROPERTY(SaveGame) int32 ClearedArenaMask = 0;
    UPROPERTY(SaveGame) bool bBossDefeated = false;
    UPROPERTY(SaveGame) int32 TotalExperienceOrbs = 0;
    UPROPERTY(SaveGame) int32 TotalHealthPacks = 0;
    UPROPERTY(SaveGame) int32 TotalCriticalHits = 0;
    UPROPERTY(SaveGame) int32 TotalSkillsUsed = 0;
    UPROPERTY(SaveGame) int32 TotalGoldEarned = 0;
    UPROPERTY(SaveGame) int32 HighestSeraphineLevel = 0;
    UPROPERTY(SaveGame) int32 TotalBombPickups = 0;
    UPROPERTY(SaveGame) bool bRodeSupercar = false;
    UPROPERTY(SaveGame) int32 MostBoonsInRun = 0;
    UPROPERTY(SaveGame) bool bHardAatroxDefeated = false;
    UPROPERTY(SaveGame) int32 ExtremeClearedArenaMask=0;
    UPROPERTY(SaveGame) bool bGoldHistoryMigrated=false;
};
