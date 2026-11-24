#pragma once
#include "CoreMinimal.h"

inline uint8 GetSwarmTimedMiniBossVariant(int32 Event,int32 Arena) { return Event%2==0?6:static_cast<uint8>((Arena+Event+2)%6); }
namespace SwarmRound { inline constexpr float BossAt=600.f; inline constexpr float Events[]={160.f,260.f,400.f,500.f,BossAt}; }

// Playtest targets, not numbers extracted from the reference video.
struct FSwarmEncounterStage { float Interval; int32 PopulationCap; float Health; float Damage; int32 Batch; };
inline FSwarmEncounterStage GetSwarmEncounterStage(float Elapsed)
{
    if(Elapsed<80.f) { return {.75f,80,1.f,1.f,3}; }
    if(Elapsed<240.f) { return {.6f,110,1.35f,1.1f,4}; }
    if(Elapsed<400.f) { return {.45f,145,1.9f,1.25f,6}; }
    return {.35f,180,2.6f,1.4f,8};
}
inline float GetSwarmTeamHealthMultiplier(int32 Players)
{ return 1.f+.65f*(FMath::Clamp(Players,1,4)-1); }
inline float GetSwarmDifficultyHealth(int32 Difficulty)
{ return Difficulty<=0?1.f:Difficulty==1?1.8f:3.f; }
inline float GetSwarmDifficultyDamage(int32 Difficulty)
{ return Difficulty<=0?1.f:Difficulty==1?1.4f:1.9f; }

// Slower early growth for the 10-minute playtest: 2.5x the previous requirements, including early levels.
inline int32 GetSwarmExperienceRequired(int32 Level)
{
    const int32 L=FMath::Clamp(Level,1,999),Mid=FMath::Max(0,L-5),Late=FMath::Max(0,L-15);
    return 120+30*(L-1)+5*Mid*Mid+50*Late;
}
