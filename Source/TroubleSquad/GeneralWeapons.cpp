#include "GeneralWeapons.h"
#include "AnimaPreview.h"

#include "SwarmEncounter.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

namespace
{
constexpr FGeneralWeaponInfo Weapons[] = {
    {TEXT("Mine"), TEXT("Shark Mine"), TEXT("Endless Hunt"), 5},
    {TEXT("Train"), TEXT("City Transit"), TEXT("FC Express"), 5},
    {TEXT("Boomerang"), TEXT("Blade Boomerang"), TEXT("Quad Boomerang"), 3},
    {TEXT("Trail"), TEXT("Toxic Footprints"), TEXT("Chemical Sprayer"), 3},
    {TEXT("Crescent"), TEXT("Lion's Lament"), TEXT("Coiling Light"), 6},
    {TEXT("Launcher"), TEXT("UwU Blaster"), TEXT("OwO Blaster"), 6},
    {TEXT("Blade"), TEXT("Echoing Batblades"), TEXT("Vayne's Chromablades"), 7},
    {TEXT("Tibber"), TEXT("T.I.B.B.E.R.S"), TEXT("T.I.B.B.E.R.S XL"), 8},
    {TEXT("Gatling"), TEXT("Gatling Bunny Guns"), TEXT("Double Bunny Barrage"), 8},
    {TEXT("Bomb"), TEXT("Ani-Mines"), TEXT("Triple Dynamite"), 9},
    {TEXT("Fire"), TEXT("Searing Shortbow"), TEXT("Evolved Flame Shot"), 9},
    {TEXT("Crossbow"), TEXT("Bunny Crossbow"), TEXT("Bunny Prime Crossbow"), 10},
    {TEXT("Orbital"), TEXT("Bunny Mega-Blast"), TEXT("Rapid Bunny Artillery"), 10},
    {TEXT("IceArmor"), TEXT("Iceblast Armor"), TEXT("Deep Freeze"), 2},
    {TEXT("Statikk"), TEXT("Statikk Sword"), TEXT("Prumbis's Electrocarver"), 1},
    {TEXT("Radiance"), TEXT("Radiant Field"), TEXT("Explosive Embrace"), 1},
    {TEXT("Glove"), TEXT("Vortex Glove"), TEXT("Tempest's Gauntlet"), 4},
    {TEXT("Cyclone"), TEXT("Cyclonic Slicers"), TEXT("Unceasing Storm"), 4},
    {TEXT("Yuumi"), TEXT("YuumiBot"), TEXT("YuumiBot Final Final"), PickupRangeChoice},
    {TEXT("Annihilator"), TEXT("Annihilator"), TEXT("Animapocalypse"), ExperienceChoice}
};
static_assert(UE_ARRAY_COUNT(Weapons) == GeneralWeaponCount);
}

const FGeneralWeaponInfo& GetGeneralWeaponInfo(uint8 Weapon)
{
    return Weapons[FMath::Min(static_cast<int32>(Weapon), GeneralWeaponCount - 1)];
}

int32 AAnimaPreviewCharacter::GetGeneralWeaponRank(uint8 Weapon) const
{
    return GeneralWeaponRanks.IsValidIndex(Weapon) ? GeneralWeaponRanks[Weapon] : 0;
}

bool AAnimaPreviewCharacter::IsGeneralWeaponEvolved(uint8 Weapon) const
{
    return EvolvedGeneralWeapons.Contains(Weapon);
}

bool AAnimaPreviewCharacter::CanEvolveGeneralWeapon(uint8 Weapon) const
{
    const uint8 Requirement = GetGeneralWeaponInfo(Weapon).EvolutionStat;
    if (GetGeneralWeaponRank(Weapon) != 5 || IsGeneralWeaponEvolved(Weapon)) { return false; }
    switch (Requirement)
    {
    case 1: return RunHealthBonus > 0.f;
    case 2: return RunArmorBonus > 0.f;
    case 3: return RunMoveBonus > 0.f;
    case 4: return RunRegen > 0.f;
    case PickupRangeChoice: return RunWeaponStats.IsValidIndex(6) && RunWeaponStats[6] > 0;
    case ExperienceChoice: return RunWeaponStats.IsValidIndex(7) && RunWeaponStats[7] > 0;
    default: return RunWeaponStats.IsValidIndex(Requirement-5) && RunWeaponStats[Requirement - 5] > 0;
    }
}

bool AAnimaPreviewCharacter::UpgradeGeneralWeapon(uint8 Weapon)
{
    if (Weapon >= GeneralWeaponCount || GetGeneralWeaponRank(Weapon) == 5)
    {
        return false;
    }
    if (!GeneralWeapons.Contains(Weapon))
    {
        if (GeneralWeapons.Num() >= 4) { return false; }
        GeneralWeapons.Add(Weapon);
    }
    GeneralWeaponRanks.SetNum(GeneralWeaponCount);
    RunWeaponStats.SetNum(8);
    ++GeneralWeaponRanks[Weapon];
    return true;
}

void AAnimaPreviewCharacter::CollectRunGold(int32 Amount, FVector Origin)
{
    if (HasAuthority() && Amount > 0)
    {
        RunGold += Amount;
        RegisterLootPickup(Origin, true);
    }
}

bool AAnimaPreviewCharacter::ApplyGeneralWeaponDamage(AActor* Target, float Damage, uint8 Weapon, bool bCrit, bool bGold)
{
    if (!IsValid(Target)) { return false; }
    const float Crit = GetBaseCriticalChance() + .08f * (RunWeaponStats.IsValidIndex(5)?RunWeaponStats[5]:0)
        + ((Weapon == static_cast<uint8>(EGeneralWeapon::Crossbow) || Weapon == static_cast<uint8>(EGeneralWeapon::Orbital)) ? .20f : 0.f);
    const bool bCriticalHit = bCrit && FMath::FRand() < Crit;
    const FVector Position = Target->GetActorLocation();
    if (Weapon < GeneralWeaponCount && GetCombatTime() >= NextWeaponImpactVfxAt[Weapon])
    {
        NextWeaponImpactVfxAt[Weapon] = GetCombatTime() + .08f;
        ShowWeaponVfx(Weapon, Position, Position, bCriticalHit ? 38.f : 24.f, 2, IsGeneralWeaponEvolved(Weapon));
    }
    HitAdditionalEnemy(Target, Damage * (bCriticalHit ? 1.75f : 1.f));
    if (bCriticalHit && CharacterChoice == 5) { YasuoFlow = FMath::Min(100.f, YasuoFlow + 3.f); }
    if (bGold && !IsValid(Target) && FMath::FRand() < FMath::Min(.75f, .12f + Crit))
    {
        ASwarmExperienceOrb::SpawnGold(GetWorld(), Position);
    }
    return bCriticalHit;
}

void AAnimaPreviewCharacter::FireGeneralWeapon(uint8 Weapon, float Now)
{
    const int32 Rank = GetGeneralWeaponRank(Weapon);
    const bool bEvolved = IsGeneralWeaponEvolved(Weapon);
    const auto Type = static_cast<EGeneralWeapon>(Weapon);
    const FGeneralWeaponStats Stats=GetGeneralWeaponStats(Weapon,Rank,bEvolved);
    const int32 Extra = Stats.bUsesProjectileStat ? GetCombatBonusProjectiles() : 0;
    const int32 Shots=Stats.Projectiles+Extra;
    const float Duration = GetCombatDurationMultiplier();
    const float Area = GetCombatAreaMultiplier();
    const float Damage = Stats.Damage;
    const bool bUsesHaste=Type!=EGeneralWeapon::Trail && Type!=EGeneralWeapon::Radiance && Type!=EGeneralWeapon::Glove;
    GeneralWeaponReadyAt[Weapon] = Now + (bUsesHaste?GetCombatCooldown(Stats.Cooldown):Stats.Cooldown);
    const FVector Origin = GetActorLocation();
    const FVector Forward = GetActorForwardVector();
    auto Targets = FindCombatTargets(Origin, 1500.f);
    Targets.Sort([Origin](const AActor& A, const AActor& B)
    {
        return FVector::DistSquared2D(A.GetActorLocation(), Origin) < FVector::DistSquared2D(B.GetActorLocation(), Origin);
    });
    const auto Blast = [&](FVector Center, float Radius, float Power)
    {
        for (AActor* Target : FindCombatTargets(Center, Radius)) { ApplyGeneralWeaponDamage(Target, Power, Weapon, false, false); }
        ShowWeaponVfx(Weapon, Center, Center, Radius, 1, bEvolved);
    };
    const auto Line = [&](FVector Start, FVector End, float Width, float Power, bool bFirstOnly, bool bGold)
    {
        if(Type==EGeneralWeapon::Launcher || Type==EGeneralWeapon::Boomerang || Type==EGeneralWeapon::Glove
            || Type==EGeneralWeapon::Crossbow || (Type==EGeneralWeapon::Crescent && !bEvolved))
        {
            LaunchGeneralProjectile(Weapon,Start,End,Width,Power,bFirstOnly);
            return;
        }
        const FVector Delta = End - Start;
        const float Length = Delta.Size2D();
        const FVector Direction = Delta.GetSafeNormal2D();
        auto Candidates = FindCombatTargets((Start + End) * .5f, Length * .5f + Width);
        Candidates.Sort([Start](const AActor& A, const AActor& B)
        {
            return FVector::DistSquared2D(A.GetActorLocation(), Start) < FVector::DistSquared2D(B.GetActorLocation(), Start);
        });
        for (AActor* Target : Candidates)
        {
            if (!IsValid(Target)) { continue; }
            const FVector Offset = Target->GetActorLocation() - Start;
            const float Along = FVector::DotProduct(Direction, Offset);
            if (Along < -Width || Along > Length + Width || (Offset - Direction * Along).SizeSquared2D() > FMath::Square(Width))
            {
                continue;
            }
            const bool bCritical = ApplyGeneralWeaponDamage(Target, Power, Weapon, true, bGold);
            if (bGold && IsValid(Target))
            {
                if (ACharacter* Enemy = Cast<ACharacter>(Target)) { Enemy->LaunchCharacter(FVector(0, 0, 320), false, true); }
            }
            if (bFirstOnly || (Type == EGeneralWeapon::Crossbow && !bCritical)) { break; }
        }
        ShowWeaponVfx(Weapon, Start, End, Width, 0, bEvolved);
    };

    switch (Type)
    {
    case EGeneralWeapon::Mine:
        {
            FVector Last = Origin;
            TSet<AActor*> Visited;
            const int32 Limit = bEvolved ? Targets.Num() : Stats.Projectiles;
            for (int32 Hop = 0; Hop < Limit; ++Hop)
            {
                AActor* Best = nullptr;
                double Distance = FMath::Square(Hop == 0 ? double(Stats.Range) : double(Stats.Radius*3.52f));
                for (AActor* Target : Targets)
                {
                    if (!IsValid(Target) || Visited.Contains(Target)) { continue; }
                    const double Candidate = FVector::DistSquared2D(Last, Target->GetActorLocation());
                    if (Candidate < Distance) { Best = Target; Distance = Candidate; }
                }
                if (!Best) { break; }
                Visited.Add(Best);
                const FVector End = Best->GetActorLocation();
                ShowWeaponVfx(Weapon, Last, End, Stats.Radius * Area, 0, bEvolved);
                Blast(End, Stats.Radius * Area, Damage);
                Last = End;
            }
            break;
        }
    case EGeneralWeapon::Train:
        {
            const FVector Direction = FRotator(0, FMath::FRandRange(-180.f, 180.f), 0).Vector();
            Line(Origin - Direction * (Stats.Range*.5f), Origin + Direction * (Stats.Range*.5f), Stats.Radius * Area,
                Damage + GetCombatArmor() * 1.5f, false, bEvolved);
            if (bEvolved)
            {
                for (int32 Index = -2; Index <= 2; ++Index)
                {
                    WeaponZones.Add({Weapon, Origin + Direction * (Index * 350.f), Direction, Stats.Radius * 1.16f * Area,
                        Damage * .65f, Now + Stats.Interval, Now + Stats.Duration, Stats.Interval, 1, true});
                }
            }
            break;
        }
    case EGeneralWeapon::Boomerang:
        if (!Targets.IsEmpty())
        {
            const FVector Direction = (Targets[0]->GetActorLocation() - Origin).GetSafeNormal2D();
            for (int32 Shot = 0; Shot < Shots; ++Shot)
            {
                const FVector End = Origin + Direction.RotateAngleAxis((Shot-(Shots-1)*.5f) * 8.f, FVector::UpVector) * Stats.Range;
                Line(Origin, End, Stats.Radius, Damage, false, false);
                WeaponZones.Add({Weapon, End, Direction, Stats.Radius * (bEvolved ? 3.5f : 1.f),
                    Damage, Now + Stats.Duration*.7f, Now + Stats.Duration, Stats.Interval, 1, bEvolved});
            }
        }
        break;
    case EGeneralWeapon::Trail:
        if (GetVelocity().SizeSquared2D() > 25.f)
        {
            WeaponZones.Add({Weapon, Origin, Forward, Stats.Radius * Area, Damage * .33f, Now + .2f,
                Now + Stats.Duration * Duration, Stats.Interval, FMath::CeilToInt(Stats.Duration*Duration/Stats.Interval), bEvolved});
        }
        break;
    case EGeneralWeapon::Crescent:
        for (int32 Side : {-1, 1})
        {
            for (int32 Shot = 0; Shot < Shots; ++Shot)
            {
                const FVector Direction = Forward.RotateAngleAxis(Side * (90.f + 8.f * Shot), FVector::UpVector);
                Line(Origin, Origin + Direction * Stats.Range * (bEvolved ? 2.42f : 1.f), Stats.Radius * (bEvolved ? 1.9f : 1.f), Damage, false, false);
            }
        }
        break;
    case EGeneralWeapon::Launcher:
        if (!Targets.IsEmpty() && FVector::DistSquared2D(Origin,Targets[0]->GetActorLocation())<=FMath::Square(Stats.Range))
        {
            const FVector End = Targets[0]->GetActorLocation();
            for (int32 Shot = 0; Shot < Shots; ++Shot)
            {
                const FVector Side=FVector::CrossProduct((End-Origin).GetSafeNormal2D(),FVector::UpVector);
                Line(Origin+Side*((Shot-(Shots-1)*.5f)*12.f), End, Stats.Radius, Damage, true, false);
            }
        }
        break;
    case EGeneralWeapon::Blade:
        for (int32 Shot = 0; Shot < Shots; ++Shot)
        {
            FVector Start = Origin;
            FVector Direction = Forward.RotateAngleAxis(11.f * (Shot-(Shots-1)*.5f), FVector::UpVector);
            LaunchGeneralProjectile(Weapon,Start,Start+Direction*Stats.Range,Stats.Radius,Damage,!bEvolved);
        }
        break;
    case EGeneralWeapon::Tibber:
        WeaponZones.Add({Weapon, Origin + Forward * 150.f, Forward, Stats.Radius * Area, Damage * .8f,
            Now + .35f, Now + Stats.Duration * Duration, Stats.Interval, FMath::CeilToInt(Stats.Duration*Duration/Stats.Interval), bEvolved});
        break;
    case EGeneralWeapon::Gatling:
        WeaponZones.Add({Weapon, Origin, Forward, Stats.Radius * Area, Damage * .38f,
            Now, Now + Stats.Duration * Duration, Stats.Interval, FMath::CeilToInt(Stats.Duration*Duration/Stats.Interval), bEvolved});
        break;
    case EGeneralWeapon::Bomb:
    case EGeneralWeapon::Fire:
        for (int32 Shot = 0; Shot < Shots; ++Shot)
        {
            const FVector Direction = Forward.RotateAngleAxis((Shot - (Shots-1) * .5f) * 39.f, FVector::UpVector);
            const FVector End = Origin + Direction * Stats.Range;
            ShowWeaponVfx(Weapon, Origin, End, 35.f, 0, bEvolved);
            if (Type == EGeneralWeapon::Bomb)
            {
                WeaponZones.Add({Weapon, End, Direction, Stats.Radius * Area, Damage, Now + .75f, Now + Stats.Duration,
                    Stats.Interval, bEvolved ? 3 : 1, bEvolved});
            }
            else
            {
                WeaponZones.Add({Weapon, End, Direction, Stats.Radius * (bEvolved ? 1.63f : 1.f) * Area, Damage * .38f,
                    Now + .25f, Now + Stats.Duration * Duration, Stats.Interval, FMath::CeilToInt(Stats.Duration*Duration/Stats.Interval), bEvolved});
            }
        }
        break;
    case EGeneralWeapon::Crossbow:
        {
            const int32 Count = Shots + (bEvolved ? 30 : 0);
            const float Yaw = FMath::FRandRange(-180.f, 180.f);
            for (int32 Shot = 0; Shot < Count; ++Shot)
            {
                const FVector Direction = Forward.RotateAngleAxis(Yaw + (Shot - (Count - 1) * .5f) * (bEvolved ? 8.f : 15.f),
                    FVector::UpVector);
                Line(Origin, Origin + Direction * Stats.Range, Stats.Radius, Damage * .66f, false, false);
            }
            break;
        }
    case EGeneralWeapon::Orbital:
        if (!Targets.IsEmpty())
        {
            const int32 Strikes = Stats.Projectiles + (bEvolved ? 2 : 0);
            for (int32 Shot = 0; Shot < Strikes; ++Shot)
            {
                const FVector Center = Targets[FMath::RandRange(0, Targets.Num() - 1)]->GetActorLocation();
                const float Delay = .35f + Shot * .26f;
                WeaponZones.Add({Weapon, Center, {}, Stats.Radius * Area, Damage * (bEvolved && Shot == Strikes - 1 ? 2.3f : 1.f),
                    Now + Delay, Now + Delay + Stats.Duration, 0.f, 1, bEvolved});
            }
        }
        break;
    case EGeneralWeapon::IceArmor:
        if (bEvolved && IceShieldEnd <= 0.f)
        {
            IceShieldRemaining = Stats.Damage + MaxCombatHealth * .10f;
            IceShieldEnd = Now + Stats.Duration * Duration;
        }
        break;
    case EGeneralWeapon::Statikk:
        {
            FVector Last = Origin;
            for (int32 Jump = 0; Jump < Stats.Projectiles; ++Jump)
            {
                ASwarmEnemy* Target = nullptr;
                for (AActor* Candidate : Targets)
                {
                    auto* Enemy = Cast<ASwarmEnemy>(Candidate);
                    if (IsValid(Enemy) && FVector::DistSquared2D(Enemy->GetActorLocation(), Last) <= FMath::Square((Jump ? Stats.Radius : Stats.Range) * Area)
                        && (!Target || Enemy->GetHealth() > Target->GetHealth()))
                    {
                        Target = Enemy;
                    }
                }
                if (!Target) { break; }
                Targets.RemoveSwap(Target);
                const FVector Center = Target->GetActorLocation();
                ShowWeaponVfx(Weapon, Last, Center, 40.f, 0, bEvolved);
                Last = Center;
                ApplyGeneralWeaponDamage(Target, Damage, Weapon, true, false);
                if (bEvolved)
                {
                    const float Lifetime = Stats.Duration * Duration;
                    WeaponZones.Add({Weapon, Center, {}, Stats.Radius * .3111f * Area, Damage * .25f, Now + Stats.Interval,
                        Now + Lifetime + .01f, Stats.Interval, FMath::FloorToInt(Lifetime / Stats.Interval), true});
                }
            }
            break;
        }
    case EGeneralWeapon::Radiance:
        GeneralWeaponReadyAt[Weapon] = Now + Stats.Cooldown;
        Blast(Origin, (Stats.Radius + Rank * 12.f) * Area, Damage * .35f + MaxCombatHealth * .025f);
        break;
    case EGeneralWeapon::Glove:
        GeneralWeaponReadyAt[Weapon] = Now + Stats.Cooldown;
        for (int32 Shot = 0; Shot < Shots * (bEvolved ? 2 : 1); ++Shot)
        {
            const FVector Direction = FVector::ForwardVector.RotateAngleAxis(Now * 105.f + Shot * 360.f / (Shots * (bEvolved ? 2 : 1)), FVector::UpVector);
            Line(Origin, Origin + Direction * Stats.Range, Stats.Radius, Damage * .40f, true, false);
        }
        break;
    case EGeneralWeapon::Cyclone:
        {
            const float Lifetime = bEvolved ? TNumericLimits<float>::Max() : Stats.Duration * Duration;
            GeneralWeaponReadyAt[Weapon] = bEvolved ? Lifetime : FMath::Max(Now + Lifetime, GeneralWeaponReadyAt[Weapon]);
            WeaponZones.Add({Weapon, Origin, {}, Stats.Radius * Area, Damage * .65f, Now, Now + Lifetime,
                Stats.Interval, bEvolved ? MAX_int32 : FMath::CeilToInt(Lifetime / Stats.Interval), bEvolved});
            break;
        }
    case EGeneralWeapon::Yuumi:
        {
            const float Lifetime = Stats.Duration * Duration;
            WeaponZones.Add({Weapon, Origin, {}, Stats.Radius * Area, Damage * .65f, Now, Now + Lifetime,
                Stats.Interval, FMath::CeilToInt(Lifetime / Stats.Interval), bEvolved});
            break;
        }
    case EGeneralWeapon::Annihilator:
        ShowWeaponVfx(Weapon, Origin, Origin, Stats.Range * Area, 1, bEvolved);
        for (AActor* Actor : FindCombatTargets(Origin, Stats.Range * Area))
        {
            auto* Enemy = Cast<ASwarmEnemy>(Actor);
            if (!IsValid(Enemy) || Enemy->Kind == 4) { continue; }
            const FVector Position = Enemy->GetActorLocation();
            ApplyGeneralWeaponDamage(Enemy, Enemy->Kind < 2 ? Enemy->GetHealth() * 2.f : Damage * 8.f, Weapon, false, bEvolved);
            if (bEvolved && !IsValid(Enemy)) { ASwarmExperienceOrb::SpawnPickup(GetWorld(), Position, 0); }
        }
        break;
    default: break;
    }
}

void AAnimaPreviewCharacter::TriggerIceBurst(float Now)
{
    const uint8 Weapon = static_cast<uint8>(EGeneralWeapon::IceArmor);
    IceShieldEnd = 0.f;
    IceShieldRemaining = 0.f;
    GeneralWeaponReadyAt[Weapon] = Now + GetCombatCooldown(GetGeneralWeaponStats(Weapon,GetGeneralWeaponRank(Weapon),IsGeneralWeaponEvolved(Weapon)).Cooldown);
    const FGeneralWeaponStats Stats=GetGeneralWeaponStats(Weapon,GetGeneralWeaponRank(Weapon),IsGeneralWeaponEvolved(Weapon));
    const float Radius = Stats.Radius * GetCombatAreaMultiplier();
    ShowWeaponVfx(Weapon, GetActorLocation(), GetActorLocation(), Radius, 1, IsGeneralWeaponEvolved(Weapon));
    for (AActor* Target : FindCombatTargets(GetActorLocation(), Radius))
    {
        if (auto* Enemy = Cast<ASwarmEnemy>(Target))
        {
            Enemy->Freeze(1.2f * GetCombatDurationMultiplier());
        }
        ApplyGeneralWeaponDamage(Target, Stats.Damage + GetCombatArmor() * 1.5f
            + MaxCombatHealth * .035f, Weapon, false, false);
    }
}

void AAnimaPreviewCharacter::NotifyGeneralWeaponEnemyDeath(FVector Position)
{
    if (CombatHealth <= 0.f || !IsGeneralWeaponEvolved(static_cast<uint8>(EGeneralWeapon::Radiance))) { return; }
    if (FVector::DistSquared2D(Position, GetActorLocation()) <= FMath::Square(305.f * GetCombatAreaMultiplier()))
    {
        RadianceExplosions.Add(Position);
    }
}

void AAnimaPreviewCharacter::TickGeneralWeapons(float Now, float DeltaTime)
{
    TickGeneralProjectiles(Now,DeltaTime);
    if (IceShieldEnd > 0.f && Now >= IceShieldEnd) { TriggerIceBurst(Now); }
    const uint8 Radiance = static_cast<uint8>(EGeneralWeapon::Radiance);
    const auto Explosions = MoveTemp(RadianceExplosions);
    for (const FVector& Position : Explosions)
    {
        const float Radius = 170.f * GetCombatAreaMultiplier();
        ShowWeaponVfx(Radiance, Position, Position, Radius, 1, true);
        for (AActor* Target : FindCombatTargets(Position, Radius))
        {
            ApplyGeneralWeaponDamage(Target, MaxCombatHealth * .055f, Radiance, false, false);
        }
    }
    for (uint8 Weapon : GeneralWeapons)
    {
        if (Weapon == static_cast<uint8>(EGeneralWeapon::IceArmor) && !IsGeneralWeaponEvolved(Weapon)) { continue; }
        if (Now >= GeneralWeaponReadyAt[Weapon]) { FireGeneralWeapon(Weapon, Now); }
    }
    for (int32 Index = WeaponZones.Num() - 1; Index >= 0; --Index)
    {
        FWeaponZone& Zone = WeaponZones[Index];
        if (Now >= Zone.EndAt || Zone.Ticks <= 0)
        {
            WeaponZones.RemoveAtSwap(Index);
            continue;
        }
        const auto Type = static_cast<EGeneralWeapon>(Zone.Weapon);
        const FGeneralWeaponStats ZoneStats=GetGeneralWeaponStats(Zone.Weapon,GetGeneralWeaponRank(Zone.Weapon),Zone.bEvolved);
        if (Type == EGeneralWeapon::Tibber || Type == EGeneralWeapon::Yuumi)
        {
            auto Targets = FindCombatTargets(Zone.Position, ZoneStats.Range);
            if (!Targets.IsEmpty())
            {
                AActor* Target = Targets[0];
                if (Zone.bEvolved)
                {
                    for (AActor* Candidate : Targets)
                    {
                        const auto* Enemy = Cast<ASwarmEnemy>(Candidate);
                        const auto* Current = Cast<ASwarmEnemy>(Target);
                        if (Enemy && (!Current || Enemy->GetHealth() > Current->GetHealth())) { Target = Candidate; }
                    }
                }
                Zone.Direction=(Target->GetActorLocation()-Zone.Position).GetSafeNormal2D();
                Zone.Position = FMath::VInterpConstantTo(Zone.Position, Target->GetActorLocation(), DeltaTime,
                    (Zone.bEvolved ? 450.f : 300.f) * (1.f + RunMoveBonus));
            }
        }
        if (Now < Zone.NextTick) { continue; }
        Zone.NextTick += Zone.Interval;
        --Zone.Ticks;
        if (Type == EGeneralWeapon::Cyclone)
        {
            const FVector Origin = GetActorLocation();
            const int32 Count = GetGeneralWeaponStats(Zone.Weapon,GetGeneralWeaponRank(Zone.Weapon),Zone.bEvolved).Projectiles + GetCombatBonusProjectiles();
            TSet<AActor*> Hit;
            for (int32 Blade = 0; Blade < Count; ++Blade)
            {
                const FVector Direction = FVector::ForwardVector.RotateAngleAxis(Now * 132.f + Blade * 360.f / Count, FVector::UpVector);
                const FVector Position = Origin + Direction * ZoneStats.Range * (Zone.bEvolved ? 1.5128f : 1.f);
                ShowWeaponVfx(Zone.Weapon, Position, Position + Direction, Zone.Radius, 1, Zone.bEvolved);
                for (AActor* Target : FindCombatTargets(Position, Zone.Radius))
                {
                    if (Hit.Contains(Target)) { continue; }
                    Hit.Add(Target);
                    if (auto* Enemy = Cast<ASwarmEnemy>(Target)) { Enemy->LaunchCharacter(Direction * 320.f, true, false); }
                    ApplyGeneralWeaponDamage(Target, Zone.Damage, Zone.Weapon, false, false);
                }
            }
            continue;
        }
        if (Type == EGeneralWeapon::Yuumi)
        {
            for (TActorIterator<ASwarmExperienceOrb> It(GetWorld()); It; ++It)
            {
                if (It->CanBeCollectedByDrone() && FVector::DistSquared2D(It->GetActorLocation(), Zone.Position) <= FMath::Square(Zone.Radius * (1.f + .35f * (RunWeaponStats.IsValidIndex(6)?RunWeaponStats[6]:0))))
                {
                    It->CollectBy(this);
                }
            }
        }
        const FVector Center = Type == EGeneralWeapon::Gatling ? GetActorLocation() : Zone.Position;
        if (Type == EGeneralWeapon::Boomerang && !Zone.bEvolved)
        {
            const FVector Destination = GetActorLocation();
            const FVector Segment = Destination - Center;
            for (AActor* Target : FindCombatTargets((Center + Destination) * .5f, Segment.Size() * .5f + Zone.Radius))
            {
                const FVector Nearest = FMath::ClosestPointOnSegment(Target->GetActorLocation(), Center, Destination);
                if (FVector::DistSquared2D(Nearest, Target->GetActorLocation()) <= FMath::Square(Zone.Radius))
                {
                    ApplyGeneralWeaponDamage(Target, Zone.Damage, Zone.Weapon, true, false);
                }
            }
            ShowWeaponVfx(Zone.Weapon, Center, Destination, Zone.Radius, 0, Zone.bEvolved);
            continue;
        }
        for (AActor* Target : FindCombatTargets(Center, Zone.Radius))
        {
            if (Type == EGeneralWeapon::Gatling &&
                FVector::DotProduct(Zone.Direction, (Target->GetActorLocation() - Center).GetSafeNormal2D()) < .61f)
            {
                continue;
            }
            if (Type == EGeneralWeapon::Yuumi)
            {
                if (auto* Enemy = Cast<ASwarmEnemy>(Target))
                {
                    Enemy->LaunchCharacter(FVector(0, 0, 280), false, true);
                    YuumiDamage += FMath::Min(Enemy->GetHealth(), Zone.Damage);
                    if (Zone.bEvolved && YuumiDamage >= 280.f)
                    {
                        YuumiDamage -= 280.f;
                        ASwarmExperienceOrb::SpawnPickup(GetWorld(), Center, 6);
                    }
                }
            }
            ApplyGeneralWeaponDamage(Target, Zone.Damage, Zone.Weapon,
                Type == EGeneralWeapon::Boomerang || Type == EGeneralWeapon::Orbital, Type == EGeneralWeapon::Train && Zone.bEvolved);
            if (Type == EGeneralWeapon::Trail && Zone.bEvolved)
            {
                TrailBoostStacks = FMath::Min(8, (Now < TrailBoostEnd ? TrailBoostStacks : 0) + 1);
                TrailBoostEnd = Now + 2.f;
                RivenShield = FMath::Min(.15f * MaxCombatHealth, (Now < RivenShieldEnd ? RivenShield : 0.f) + 12.f);
                RivenShieldEnd = Now + 2.f;
            }
        }
        if (Type == EGeneralWeapon::Bomb && Zone.bEvolved) { Zone.Position += Zone.Direction * 85.f; }
        if ((Type == EGeneralWeapon::Fire || Type == EGeneralWeapon::Tibber) && Zone.bEvolved)
        {
            Zone.Radius += 9.f;
            Zone.Damage *= 1.06f;
        }
        ShowWeaponVfx(Zone.Weapon, Center, Center + Zone.Direction, Zone.Radius, 1, Zone.bEvolved);
    }
    PublishGeneralSummons(Now);
}

void AAnimaPreviewCharacter::PublishGeneralSummons(float Now)
{
    if(Now<NextSummonSnapshotAt) { return; }
    NextSummonSnapshotAt=Now+.1f;
    GeneralSummonVisuals.Reset();
    for(const FWeaponZone& Zone:WeaponZones)
    {
        const auto Type=static_cast<EGeneralWeapon>(Zone.Weapon);
        if((Type!=EGeneralWeapon::Tibber && Type!=EGeneralWeapon::Yuumi) || Now>=Zone.EndAt) { continue; }
        FGeneralSummonVisual V;
        V.Weapon=Zone.Weapon; V.Position=Zone.Position;
        V.Facing=Zone.Direction.IsNearlyZero()?GetActorForwardVector():Zone.Direction;
        V.Size=FMath::Clamp(Zone.Radius/(Type==EGeneralWeapon::Tibber?155.f:165.f),.6f,3.5f);
        V.EndAt=Zone.EndAt; GeneralSummonVisuals.Add(V);
        if(GeneralSummonVisuals.Num()>=12) { break; }
    }
}
