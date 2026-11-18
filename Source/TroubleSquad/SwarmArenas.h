#pragma once

#include "CoreMinimal.h"

namespace SwarmArenas
{
    inline constexpr float LabFreezeRadius = 1050.f;
    inline const FVector LabFreezeStations[] = { FVector(-3400.f, -2550.f, 0.f), FVector(3400.f, 2550.f, 0.f) };
    inline const FVector OutskirtsCannon(-3050.f, -2450.f, 0.f);

    inline constexpr const TCHAR* Names[] = { TEXT("L_CoastalResearch"), TEXT("L_Outskirts"),
        TEXT("L_SubterraneanLab"), TEXT("L_Beachhead") };
    inline constexpr const TCHAR* Bosses[] = { TEXT("REK'SAI"), TEXT("BRIAR"), TEXT("BEL'VETH"), TEXT("AATROX") };

    inline int32 Find(const FString& Level)
    {
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
        {
            if (Level == Names[Index]) { return Index; }
        }
        return INDEX_NONE;
    }
}
