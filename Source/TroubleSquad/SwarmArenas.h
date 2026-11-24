#pragma once

#include "CoreMinimal.h"

namespace SwarmArenas
{
    inline constexpr float LabFreezeRadius = 1050.f;
    inline const FVector LabFreezeStations[] = { FVector(-3400.f, -2550.f, 0.f), FVector(3400.f, 2550.f, 0.f) };
    inline const FVector OutskirtsCannon(-3050.f, -2450.f, 0.f);

    inline constexpr const TCHAR* Names[] = { TEXT("L_CoastalResearch"), TEXT("L_Outskirts"),
        TEXT("L_SubterraneanLab"), TEXT("L_Beachhead") };
    inline constexpr const TCHAR* TravelNames[] = { TEXT("L_CoastalResearch_Rebuilt"), TEXT("L_Outskirts_Rebuilt"),
        TEXT("L_SubterraneanLab_Rebuilt"), TEXT("L_Beachhead") };
    inline constexpr const TCHAR* Bosses[] = { TEXT("REK'SAI"), TEXT("BRIAR"), TEXT("BEL'VETH"), TEXT("AATROX") };

    inline bool IsLabWater(FVector P)
    {
        for(int32 I=0;I<2;++I)
        {
            const float MinY=I==0?-1500.f:900.f,MaxY=I==0?-900.f:1500.f,Right=I==0?2200.f:3000.f;
            if(P.X<=-4600.f||P.X>=Right||P.Y<=MinY||P.Y>=MaxY) { continue; }
            bool Bridge=false;for(float X:{-2600.f,-400.f,1800.f}) {Bridge|=FMath::Abs(P.X-X)<=235.f;}
            if(!Bridge) { return true; }
        }
        return false;
    }
    inline FVector LabPositionOnLand(FVector P,float Radius)
    {
        if(!IsLabWater(P)) { return P; }
        const float MinY=P.Y<0.f?-1500.f:900.f,MaxY=P.Y<0.f?-900.f:1500.f;
        FVector Best=P;float Cost=TNumericLimits<float>::Max();
        const auto Consider=[&](FVector Q) {const float D=FVector::DistSquared2D(P,Q);if(D<Cost){Cost=D;Best=Q;}};
        Consider(FVector(P.X,MinY-Radius-20.f,P.Z));Consider(FVector(P.X,MaxY+Radius+20.f,P.Z));
        for(float X:{-2600.f,-400.f,1800.f}) {Consider(FVector(X,P.Y,P.Z));}
        return Best;
    }

    // Crossing hints for the rebuilt laboratory's two coolant trenches.
    inline FVector LabTransitWaypoint(FVector Origin, FVector Target)
    {
        constexpr float MinY[]={-1500.f,900.f}, MaxY[]={-900.f,1500.f};
        constexpr float RightX[]={2200.f,3000.f}, Bridges[]={-2600.f,-400.f,1800.f};
        int32 Selected=INDEX_NONE;float Closest=TNumericLimits<float>::Max();
        for(int32 I=0;I<2;++I)
        {
            if((Origin.X>RightX[I]+80.f&&Target.X>RightX[I]+80.f)||(Origin.X<-4680.f&&Target.X<-4680.f)) { continue; }
            const bool Cross=(Origin.Y<MinY[I]&&Target.Y>MaxY[I])||(Origin.Y>MaxY[I]&&Target.Y<MinY[I]);
            const bool Inside=Origin.Y>=MinY[I]&&Origin.Y<=MaxY[I]&&(Target.Y<MinY[I]||Target.Y>MaxY[I]);
            const float Distance=FMath::Abs(Origin.Y-(MinY[I]+MaxY[I])*.5f);
            if((Cross||Inside)&&Distance<Closest) { Selected=I;Closest=Distance; }
        }
        if(Selected==INDEX_NONE) { return Target; }
        float Bridge=Bridges[0],Cost=TNumericLimits<float>::Max();
        for(float X:Bridges)
        {
            const float Candidate=FMath::Abs(Origin.X-X)+FMath::Abs(Target.X-X);
            if(Candidate<Cost) { Cost=Candidate;Bridge=X; }
        }
        const bool North=Origin.Y<(MinY[Selected]+MaxY[Selected])*.5f;
        const float Approach=North?MinY[Selected]-150.f:MaxY[Selected]+150.f;
        if(FMath::Abs(Origin.X-Bridge)>140.f)
        {
            if((Origin.Y<MinY[Selected]||Origin.Y>MaxY[Selected])&&FMath::Abs(Origin.Y-Approach)>90.f)
            { return FVector(Origin.X,Approach,Origin.Z); }
            return FVector(Bridge,Origin.Y,Origin.Z);
        }
        return FVector(Bridge,North?MaxY[Selected]+130.f:MinY[Selected]-130.f,Origin.Z);
    }

    inline int32 Find(const FString& Level)
    {
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
        {
            if (Level == Names[Index] || Level == TravelNames[Index]) { return Index; }
        }
        return INDEX_NONE;
    }
}
