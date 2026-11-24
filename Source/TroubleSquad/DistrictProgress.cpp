#include "DistrictProgress.h"
#include "SwarmArenas.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
const FString SlotName(TEXT("TroubleSquad_DistrictProgress"));
bool IsDiagnosticRun()
{
#if WITH_EDITOR
    int32 Character=0;
    return FParse::Value(FCommandLine::Get(),TEXT("SwarmSkillVisual="),Character)
        ||FParse::Param(FCommandLine::Get(),TEXT("SwarmGrowthNetworkReview"))
        ||FParse::Param(FCommandLine::Get(),TEXT("VfxNetworkReview"))
        ||FParse::Param(FCommandLine::Get(),TEXT("ClientUISmokeTest"))
        ||FParse::Param(FCommandLine::Get(),TEXT("SteamRoomSmokeTest"));
#else
    return false;
#endif
}
}

UDistrictProgress* UDistrictProgress::Load()
{
    if (auto* Progress = Cast<UDistrictProgress>(UGameplayStatics::LoadGameFromSlot(SlotName, 0)))
    {
        if(!Progress->bGoldHistoryMigrated)
        {
            int64 Historical=FMath::Max(0,Progress->GoldBalance);
            for(int32 I=0;I<15;++I) { const int64 Rank=Progress->GetUpgradeRank(I); Historical+=50*Rank*(Rank+1); }
            Progress->TotalGoldEarned=FMath::Max(Progress->TotalGoldEarned,static_cast<int32>(FMath::Min<int64>(MAX_int32,Historical)));
            Progress->bGoldHistoryMigrated=true;
            if(!IsDiagnosticRun()) { UGameplayStatics::SaveGameToSlot(Progress,SlotName,0); }
        }
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
    if(IsDiagnosticRun()) { return; }
    UDistrictProgress* Progress = Load();
    const bool bNewClear = ClearedArena >= 0 && ClearedArena < UE_ARRAY_COUNT(SwarmArenas::Names) && !Progress->HasClearedArena(ClearedArena);
    if (EarnedGold <= 0 && MaximumHealth <= Progress->HighestMaxHealth && Level <= Progress->HighestLevel && !bNewClear)
    {
        return;
    }
    Progress->GoldBalance = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(Progress->GoldBalance) + FMath::Max(0, EarnedGold)));
    Progress->TotalGoldEarned=static_cast<int32>(FMath::Min<int64>(MAX_int32,static_cast<int64>(Progress->TotalGoldEarned)+FMath::Max(0,EarnedGold)));
    Progress->HighestMaxHealth = FMath::Max(MaximumHealth, Progress->HighestMaxHealth);
    Progress->HighestLevel = FMath::Max(Level, Progress->HighestLevel);
    if (bNewClear) { Progress->ClearedArenaMask |= 1 << ClearedArena; }
    UGameplayStatics::SaveGameToSlot(Progress, SlotName, 0);
}

int32 UDistrictProgress::UpgradeCap(int32 Entry)
{
    static constexpr int32 Caps[]={8,5,5,5,4,3,4,4,4,4,5,2,3,2,100};
    return Entry>=0&&Entry<UE_ARRAY_COUNT(Caps)?Caps[Entry]:0;
}

int32 UDistrictProgress::UpgradePrice(int32 Rank)
{
    return 100 * (FMath::Clamp(Rank,0,100) + 1);
}

bool UDistrictProgress::IsUpgradeUnlocked(int32 Entry) const
{
    if (Entry == 2) { return HighestMaxHealth >= 1750; }
    if (Entry == 10) { return HighestLevel >= 30; }
    if (Entry == 3) { return TotalHealthPacks>=3; }
    if (Entry == 5) { return TotalExperienceOrbs>=10000; }
    if (Entry == 6) { return TotalBombPickups>=10; }
    if (Entry == 7) { return bRodeSupercar; }
    if (Entry == 8) { return TotalCriticalHits>=40000; }
    if (Entry == 9) { return TotalSkillsUsed>=200; }
    if (Entry == 11) { return HighestSeraphineLevel>=25; }
    if (Entry == 12) { return TotalGoldEarned>=5000; }
    if (Entry == 13) { return MostBoonsInRun>=4; }
    if (Entry == 14) { return bHardAatroxDefeated; }
    return UpgradeCap(Entry) > 0;
}

void UDistrictProgress::RecordCareer(int32 ExperienceOrbs,int32 HealthPacks,int32 CriticalHits,int32 Skills,int32 Champion,int32 Level)
{
    if(IsDiagnosticRun()) { return; }
    if(ExperienceOrbs<=0&&HealthPacks<=0&&CriticalHits<=0&&Skills<=0&&!(Champion==1&&Level>=25)) { return; }
    auto* Progress=Load();
    const auto Add=[](int32 A,int32 B){return static_cast<int32>(FMath::Min<int64>(MAX_int32,static_cast<int64>(A)+FMath::Max(0,B)));};
    Progress->TotalExperienceOrbs=Add(Progress->TotalExperienceOrbs,ExperienceOrbs);
    Progress->TotalHealthPacks=Add(Progress->TotalHealthPacks,HealthPacks);
    Progress->TotalCriticalHits=Add(Progress->TotalCriticalHits,CriticalHits);
    Progress->TotalSkillsUsed=Add(Progress->TotalSkillsUsed,Skills);
    if(Champion==1) { Progress->HighestSeraphineLevel=FMath::Max(Progress->HighestSeraphineLevel,Level); }
    UGameplayStatics::SaveGameToSlot(Progress,SlotName,0);
}

bool UDistrictProgress::PurchaseUpgrade(int32 Entry)
{
    if(IsDiagnosticRun()) { return false; }
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

void UDistrictProgress::RecordWorldProgress(int32 Bombs,bool bSupercar,int32 Boons,bool bHardAatrox)
{
    if(IsDiagnosticRun()) { return; }
    auto* Progress=Load();
    Progress->TotalBombPickups=static_cast<int32>(FMath::Min<int64>(MAX_int32,static_cast<int64>(Progress->TotalBombPickups)+FMath::Max(0,Bombs)));
    Progress->bRodeSupercar|=bSupercar;
    Progress->MostBoonsInRun=FMath::Max(Progress->MostBoonsInRun,Boons);
    Progress->bHardAatroxDefeated|=bHardAatrox;
    UGameplayStatics::SaveGameToSlot(Progress,SlotName,0);
}

void UDistrictProgress::RecordDifficultyClear(int32 Arena,int32 Difficulty)
{
    if(IsDiagnosticRun()) { return; }
    if(Arena<0||Arena>=4||Difficulty<1) { return; }
    auto* Progress=Load();
    if(Arena==3) { Progress->bHardAatroxDefeated=true; }
    if(Difficulty>=2) { Progress->ExtremeClearedArenaMask|=1<<Arena; }
    UGameplayStatics::SaveGameToSlot(Progress,SlotName,0);
}
