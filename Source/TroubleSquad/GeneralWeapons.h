#pragma once

#include "CoreMinimal.h"

enum class EGeneralWeapon : uint8
{
    Mine, Train, Boomerang, Trail, Crescent, Launcher, Blade, Tibber, Gatling, Bomb, Fire, Crossbow, Orbital, IceArmor, Statikk, Radiance, Glove, Cyclone, Yuumi, Annihilator, Count
};

constexpr uint8 FirstGeneralWeaponChoice = 11;
constexpr int32 GeneralWeaponCount = static_cast<int32>(EGeneralWeapon::Count);

constexpr uint8 PickupRangeChoice = FirstGeneralWeaponChoice + GeneralWeaponCount;
constexpr uint8 ExperienceChoice = PickupRangeChoice + 1;

constexpr uint8 RunGoldRewardChoice = ExperienceChoice + 1;
constexpr uint8 SignatureEvolutionChoice = 63;
constexpr uint8 FirstEvolutionChoice = 64;

struct FGeneralWeaponInfo
{
    const TCHAR* AssetSuffix;
    const TCHAR* Name;
    const TCHAR* EvolvedName;
    uint8 EvolutionStat;
};

struct FGeneralWeaponStats
{
    float Damage;
    float Cooldown;
    float Range;
    float Radius;
    float Duration;
    float Interval;
    int32 Projectiles;
    bool bUsesProjectileStat;
};

// Estimated game tuning, separate from the reference's documented champion values.
FGeneralWeaponStats GetGeneralWeaponStats(uint8 Weapon, int32 Rank, bool bEvolved = false);

const FGeneralWeaponInfo& GetGeneralWeaponInfo(uint8 Weapon);
