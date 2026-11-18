#include "GeneralWeapons.h"

namespace
{
struct FWeaponTuning
{
    float Damage[5], Cooldown[5];
    int32 Count[5];
    float Range, Radius, Duration, Interval;
    bool bProjectileStat;
    float EvolutionDamage, EvolutionCooldown;
};
// Rank 1..5. Counts mean shots, chain contacts, orbiters, or strikes according to archetype.
constexpr FWeaponTuning Tuning[] = {
    {{38,52,68,90,120},{4.8f,4.4f,4,3.6f,3.2f},{3,4,5,6,7},950,125,0,0,false,1.25f,1},
    {{95,130,170,220,280},{8,7.5f,7,6.5f,6},{1,1,1,1,1},1900,125,.6f,.35f,false,1.2f,1},
    {{30,42,58,76,100},{3.6f,3.3f,3,2.7f,2.4f},{1,1,2,2,3},800,65,.65f,.4f,true,1.2f,1},
    {{14,21,30,42,58},{.6f,.55f,.5f,.45f,.4f},{1,1,1,1,1},0,125,2.5f,.45f,false,1.15f,1},
    {{35,48,64,84,110},{4,3.7f,3.4f,3.1f,2.8f},{1,1,2,2,3},640,55,0,0,true,1.25f,1},
    {{8,11,14,18,22},{.28f,.25f,.22f,.19f,.16f},{1,1,1,1,1},1000,30,0,0,true,1.25f,.6f},
    {{28,40,55,72,95},{4.2f,3.9f,3.6f,3.3f,3},{1,1,2,2,3},920,47,0,0,true,1.2f,1},
    {{30,42,58,78,105},{10,9.5f,9,8.5f,8},{1,1,1,1,1},850,155,6,.65f,false,1.25f,1},
    {{18,25,34,45,58},{6,5.7f,5.4f,5.1f,4.8f},{1,1,1,1,1},610,610,2.1f,.24f,false,1.2f,1},
    {{45,65,90,120,155},{5.5f,5.1f,4.7f,4.3f,4},{3,3,4,4,5},330,145,1.4f,.24f,true,1.15f,1},
    {{24,34,47,63,82},{5.5f,5.1f,4.7f,4.3f,4},{1,1,2,2,3},550,135,3,.4f,true,1.2f,1},
    {{30,42,56,74,96},{4.5f,4.2f,3.9f,3.6f,3.2f},{3,4,5,6,7},810,30,0,0,true,1.1f,1},
    {{75,105,145,195,260},{6,5.6f,5.2f,4.8f,4.4f},{2,2,3,3,4},1500,150,1,0,false,1.2f,1},
    {{40,60,85,115,150},{10,9.25f,8.5f,7.75f,7},{0,0,0,0,0},0,285,4,0,false,1.2f,1},
    {{25,35,48,64,84},{4.8f,4.45f,4.1f,3.75f,3.4f},{3,4,5,6,7},1050,450,2,.35f,false,1.1f,1},
    {{9,13,18,24,32},{.8f,.8f,.8f,.8f,.8f},{0,0,0,0,0},0,245,0,0,false,1.15f,1},
    {{18,25,34,45,58},{.24f,.23f,.22f,.21f,.2f},{1,1,2,2,3},740,25,0,0,true,1.1f,1},
    {{30,42,58,78,104},{6.5f,6.25f,6,5.75f,5.5f},{2,2,3,3,4},195,55,3.2f,.15f,true,1.15f,1},
    {{20,28,38,50,66},{10,9.5f,9,8.5f,8},{1,1,1,1,1},850,165,6,.65f,false,1.2f,1},
    {{170,240,330,440,580},{50,47,44,41,38},{0,0,0,0,0},1750,1750,0,0,false,1.35f,.9f}
};
static_assert(UE_ARRAY_COUNT(Tuning)==GeneralWeaponCount);
}

FGeneralWeaponStats GetGeneralWeaponStats(uint8 Weapon,int32 Rank,bool bEvolved)
{
    const int32 W=FMath::Clamp<int32>(Weapon,0,GeneralWeaponCount-1);
    const int32 R=FMath::Clamp(Rank,1,5)-1;
    const FWeaponTuning& T=Tuning[W];
    const int32 Count=bEvolved && W==static_cast<int32>(EGeneralWeapon::Boomerang)?4:T.Count[R];
    return {T.Damage[R]*(bEvolved?T.EvolutionDamage:1.f),T.Cooldown[R]*(bEvolved?T.EvolutionCooldown:1.f),
        T.Range,T.Radius,T.Duration,T.Interval,Count,T.bProjectileStat};
}
