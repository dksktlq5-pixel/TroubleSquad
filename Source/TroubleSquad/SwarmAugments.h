#pragma once
#include "CoreMinimal.h"
enum class ESwarmAugment : uint8
{
    Vegetarian,DeathTaxes,Hoarder,FullSpeed,UltimateHaste,EbbFlow,CardCollector,LongRange,PointBlank,
    BiteSize,Growing,Metabolic,HasteMove,Magnet,SizeScrambler,GlassCannon,HeavyHitter,SpeedCollector,
    Pets,BulletLover,WarMachine,CriticalMission,Experienced,Celebrate,Cycle,Spray,Journey,EliteBomber,
    ArmorStack,CriticalExpansion,RegenTissue,Stationary,Count
};
constexpr uint8 FirstAugmentChoice=100;
struct FSwarmAugmentInfo { const TCHAR* Name; const TCHAR* Description; };
const FSwarmAugmentInfo& GetSwarmAugmentInfo(uint8 Index);
