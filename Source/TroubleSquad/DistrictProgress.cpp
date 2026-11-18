#include "DistrictProgress.h"
#include "SwarmArenas.h"
#include "Kismet/GameplayStatics.h"

namespace
{
const FString SlotName(TEXT("TroubleSquad_DistrictProgress"));
}

UDistrictProgress* UDistrictProgress::Load()
{
    if (auto* Progress = Cast<UDistrictProgress>(UGameplayStatics::LoadGameFromSlot(SlotName, 0)))
    {
        return Progress;
    }
    return CastChecked<UDistrictProgress>(UGameplayStatics::CreateSaveGameObject(StaticClass()));
}

bool UDistrictProgress::HasClearedArena(int32 Arena) const
{
    return Arena >= 0 && Arena < UE_ARRAY_COUNT(SwarmArenas::Names)
        && ((ClearedArenaMask & (1 << Arena)) != 0 || (Arena == 0 && bBossDefeated));
}

void UDistrictProgress::Record(int32 Level, int32 ClearedArena, int32 EarnedGold, int32 MaximumHealth)
{
    UDistrictProgress* Progress = Load();
    const bool bNewClear = ClearedArena >= 0 && ClearedArena < UE_ARRAY_COUNT(SwarmArenas::Names) && !Progress->HasClearedArena(ClearedArena);
    if (EarnedGold <= 0 && MaximumHealth <= Progress->HighestMaxHealth && Level <= Progress->HighestLevel && !bNewClear)
    {
        return;
    }
    Progress->GoldBalance = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(Progress->GoldBalance) + FMath::Max(0, EarnedGold)));
    Progress->HighestMaxHealth = FMath::Max(MaximumHealth, Progress->HighestMaxHealth);
    Progress->HighestLevel = FMath::Max(Level, Progress->HighestLevel);
    if (bNewClear) { Progress->ClearedArenaMask |= 1 << ClearedArena; }
    UGameplayStatics::SaveGameToSlot(Progress, SlotName, 0);
}

int32 UDistrictProgress::UpgradeCap(int32 Entry)
{
    switch (Entry)
    {
    case 0: return 8;
    case 1: case 2: case 10: return 5;
    case 4: return 4;
    default: return 0;
    }
}

int32 UDistrictProgress::UpgradePrice(int32 Rank)
{
    return 100 * (Rank + 1);
}

bool UDistrictProgress::IsUpgradeUnlocked(int32 Entry) const
{
    if (Entry == 2) { return HighestMaxHealth >= 1750; }
    if (Entry == 10) { return HighestLevel >= 30; }
    return UpgradeCap(Entry) > 0;
}

bool UDistrictProgress::PurchaseUpgrade(int32 Entry)
{
    const int32 Cap = UpgradeCap(Entry);
    if (Cap == 0) { return false; }
    UDistrictProgress* Progress = Load();
    const int32 Rank = Progress->GetUpgradeRank(Entry);
    const int32 Cost = UpgradePrice(Rank);
    if (!Progress->IsUpgradeUnlocked(Entry) || Rank >= Cap || Progress->GoldBalance < Cost) { return false; }
    if (Progress->UpgradeRanks.Num() <= Entry) { Progress->UpgradeRanks.SetNum(Entry + 1); }
    Progress->UpgradeRanks[Entry] = Rank + 1;
    Progress->GoldBalance -= Cost;
    return UGameplayStatics::SaveGameToSlot(Progress, SlotName, 0);
}
