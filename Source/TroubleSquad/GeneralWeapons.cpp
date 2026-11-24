#include "GeneralWeapons.h"
#include "AnimaPreview.h"

#include "SwarmEncounter.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

namespace
{
constexpr FGeneralWeaponInfo Weapons[] = {
    {TEXT("Mine"), TEXT("상어잡이 해양 기뢰"), TEXT("무한의 괴물 퇴치기"), 5},
    {TEXT("Train"), TEXT("최후의 도시 대중교통"), TEXT("FC 급행열차"), 5},
    {TEXT("Boomerang"), TEXT("칼날 부메랑"), TEXT("사중 부메랑"), 3},
    {TEXT("Trail"), TEXT("발자국 중독 장치"), TEXT("맨발 화학 물질 분사기"), 3},
    {TEXT("Crescent"), TEXT("사자의 비가"), TEXT("휘감는 빛"), 6},
    {TEXT("Launcher"), TEXT("귀여운 발사기"), TEXT("왕 귀여운 발사기"), 6},
    {TEXT("Blade"), TEXT("메아리치는 박쥐칼날"), TEXT("베인의 크로마칼날"), 7},
    {TEXT("Tibber"), TEXT("티버"), TEXT("티버 (특대형 에디션)"), 8},
    {TEXT("Gatling"), TEXT("개틀링 토끼 건"), TEXT("이중 깡충깡충 포화"), 8},
    {TEXT("Bomb"), TEXT("동물 지뢰"), TEXT("징크스의 삼중 다이너마이트"), 9},
    {TEXT("Fire"), TEXT("타오르는 단궁"), TEXT("진화한 불꽃 사격"), 9},
    {TEXT("Crossbow"), TEXT("전투 토끼 석궁"), TEXT("토끼 프라임 거대 석궁"), 10},
    {TEXT("Orbital"), TEXT("토끼 초강력 폭발"), TEXT("고속 토끼 속사포"), 10},
    {TEXT("IceArmor"), TEXT("얼음작렬 갑옷"), TEXT("완전 빙결"), 2},
    {TEXT("Statikk"), TEXT("스태틱의 검"), TEXT("프룸비스의 전기도축칼"), 1},
    {TEXT("Radiance"), TEXT("광휘 역장"), TEXT("폭발의 포옹"), 1},
    {TEXT("Glove"), TEXT("소용돌이 장갑"), TEXT("폭풍의 건틀릿"), 4},
    {TEXT("Cyclone"), TEXT("회오리 칼날"), TEXT("그치지 않는 폭풍"), 4},
    {TEXT("Yuumi"), TEXT("유미봇"), TEXT("유미봇_최종_최종"), PickupRangeChoice},
    {TEXT("Annihilator"), TEXT("절멸자"), TEXT("동물의 종말"), ExperienceChoice}
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

void AAnimaPreviewCharacter::CollectRunGold(int32 Amount, FVector Origin, bool bCollectedLocally)
{
    if (HasAuthority() && Amount > 0)
    {
        const int32 Awarded=FMath::RoundToInt(Amount*PermanentGoldMultiplier);
        RunGold+=Awarded;
        if(CombatHealth>0.f&&!bDistrictFinished&&HasAugment(ESwarmAugment::Hoarder))
        { CombatHealth=FMath::Min(MaxCombatHealth,CombatHealth+.005f*Awarded*MaxCombatHealth); }
        if(bCollectedLocally) { RegisterLootPickup(Origin,true); }
        ForceNetUpdate();
    }
}

bool AAnimaPreviewCharacter::ApplyGeneralWeaponDamage(AActor* Target, float Damage, uint8 Weapon, bool bCrit, bool bGold, bool bImpactSound)
{
    if (!IsValid(Target)) { return false; }
    const float Crit = GetBaseCriticalChance() + .08f * (RunWeaponStats.IsValidIndex(5)?RunWeaponStats[5]:0)
        + ((Weapon == static_cast<uint8>(EGeneralWeapon::Crossbow) || Weapon == static_cast<uint8>(EGeneralWeapon::Orbital)) ? .20f : 0.f);
    const bool bCriticalHit = bCrit && FMath::FRand() < Crit;
    const FVector Position = Target->GetActorLocation();
    const bool bPet=Weapon==static_cast<uint8>(EGeneralWeapon::Tibber)||Weapon==static_cast<uint8>(EGeneralWeapon::Yuumi);
    const float Applied=HitAdditionalEnemy(Target, Damage * ResolveCriticalMultiplier(bCriticalHit) * (bPet&&HasAugment(ESwarmAugment::Pets)?1.5f:1.f),false);
    if(Applied>0.f)
    {
        if(bImpactSound){EmitGeneralWeaponAudio(Weapon,Position,true);}
        if (Weapon < GeneralWeaponCount && GetCombatTime() >= NextWeaponImpactVfxAt[Weapon])
        {
            NextWeaponImpactVfxAt[Weapon] = GetCombatTime() + .08f;
            ShowWeaponVfx(Weapon, Position, Position, bCriticalHit ? 38.f : 24.f, 2, IsGeneralWeaponEvolved(Weapon));
        }
    }
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
    const bool NeedsTarget=Type==EGeneralWeapon::Mine||Type==EGeneralWeapon::Boomerang||Type==EGeneralWeapon::Launcher||Type==EGeneralWeapon::Orbital||Type==EGeneralWeapon::Statikk;
    bool HasLaunch=!NeedsTarget||!Targets.IsEmpty();
    if(HasLaunch&&(Type==EGeneralWeapon::Mine||Type==EGeneralWeapon::Launcher||Type==EGeneralWeapon::Statikk))
    {HasLaunch=FVector::DistSquared2D(Origin,Targets[0]->GetActorLocation())<=FMath::Square(Stats.Range*(Type==EGeneralWeapon::Statikk?Area:1.f));}
    if(Type==EGeneralWeapon::IceArmor){HasLaunch=bEvolved&&IceShieldEnd<=0.f;}
    if(HasLaunch){EmitGeneralWeaponAudio(Weapon,Origin,false);}
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
        EmitGeneralWeaponAudio(Weapon,Origin,true);
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
    EmitGeneralWeaponAudio(static_cast<uint8>(EGeneralWeapon::IceArmor),GetActorLocation(),true);
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
        if(!Zone.bAudioStarted&&Type==EGeneralWeapon::Fire){EmitGeneralWeaponAudio(Zone.Weapon,Zone.Position,true);}
        Zone.bAudioStarted=true;
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
                if (It->CanBeCollectedByDrone() && FVector::DistSquared2D(It->GetActorLocation(), Zone.Position) <= FMath::Square(Zone.Radius * (PermanentPickupMultiplier + .35f * (RunWeaponStats.IsValidIndex(6)?RunWeaponStats[6]:0))))
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
                Type == EGeneralWeapon::Boomerang || Type == EGeneralWeapon::Orbital, Type == EGeneralWeapon::Train && Zone.bEvolved, Type!=EGeneralWeapon::Fire);
            if (Type == EGeneralWeapon::Trail && Zone.bEvolved)
            {
                TrailBoostStacks = FMath::Min(8, (Now < TrailBoostEnd ? TrailBoostStacks : 0) + 1);
                TrailBoostEnd = Now + 2.f;
                RivenShield = FMath::Min(.15f * MaxCombatHealth, (Now < RivenShieldEnd ? RivenShield : 0.f) + 12.f);
                RivenShieldEnd = Now + 2.f;
            }
        }
        if(Type==EGeneralWeapon::Bomb||Type==EGeneralWeapon::Orbital){EmitGeneralWeaponAudio(Zone.Weapon,Center,true);}
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

float AAnimaPreviewCharacter::GetGeneralWeaponCooldown(uint8 Weapon) const
{
    if(Weapon>=GeneralWeaponCount||GetGeneralWeaponRank(Weapon)<=0) { return 0.f; }
    const auto Type=static_cast<EGeneralWeapon>(Weapon);const bool Evolved=IsGeneralWeaponEvolved(Weapon);
    const auto Stats=GetGeneralWeaponStats(Weapon,GetGeneralWeaponRank(Weapon),Evolved);
    if(Type==EGeneralWeapon::Cyclone&&Evolved) { return 0.f; }
    const bool Haste=Type!=EGeneralWeapon::Trail&&Type!=EGeneralWeapon::Radiance&&Type!=EGeneralWeapon::Glove;
    const float Cooldown=Haste?GetCombatCooldown(Stats.Cooldown):Stats.Cooldown;
    return Type==EGeneralWeapon::Cyclone?FMath::Max(Cooldown,Stats.Duration*GetCombatDurationMultiplier()):Cooldown;
}
float AAnimaPreviewCharacter::GetGeneralWeaponRemaining(uint8 Weapon) const
{
    if(Weapon>=GeneralWeaponCount||GetGeneralWeaponRank(Weapon)<=0||(Weapon==static_cast<uint8>(EGeneralWeapon::Cyclone)&&IsGeneralWeaponEvolved(Weapon))) { return 0.f; }
    const float Deadline=HasAuthority()?GeneralWeaponReadyAt[Weapon]:(GeneralWeaponCooldownEnds.IsValidIndex(Weapon)?GeneralWeaponCooldownEnds[Weapon]:0.f);
    return FMath::Clamp(Deadline-GetCombatTime(),0.f,3600.f);
}
FString AAnimaPreviewCharacter::GetGeneralWeaponTooltip(uint8 Weapon) const
{
    if(Weapon>=GeneralWeaponCount||GetGeneralWeaponRank(Weapon)<=0) { return {}; }
    static const TCHAR* Descriptions[]={
        TEXT("주변 적 사이를 연결하는 폭발 공격을 가합니다."),TEXT("무작위 방향으로 적을 관통하는 열차를 발사합니다. 방어력에 비례해 피해가 증가합니다."),
        TEXT("가까운 적에게 날아갔다가 돌아오는 부메랑을 발사합니다."),TEXT("이동 경로에 지속 피해를 주는 흔적을 남깁니다. 진화하면 보호막과 이동 속도를 얻습니다."),
        TEXT("좌우로 초승달 투사체를 발사합니다. 진화하면 공격 범위가 길어집니다."),TEXT("가까운 적에게 빠른 투사체를 발사합니다. 처음 맞은 적을 공격합니다."),
        TEXT("벽에서 튕기는 칼날을 발사합니다. 진화하면 관통한 적에게도 피해를 줍니다."),TEXT("티버를 소환해 주변 적을 공격합니다. 진화하면 큰 적을 집중 공격합니다."),
        TEXT("주변 범위를 연속 공격합니다. 진화하면 적을 둔화시키고 기절시킵니다."),TEXT("주변에 지뢰를 떨어뜨립니다. 진화하면 연쇄 폭발합니다."),
        TEXT("불타는 영역을 남겨 지속 피해를 줍니다."),TEXT("무작위 방향으로 석궁을 발사합니다. 치명타가 적을 관통합니다."),
        TEXT("적 위치에 궤도 폭격을 가합니다. 진화하면 마지막에 강력한 폭발을 일으킵니다."),TEXT("피해를 막고 냉기 폭발을 일으킵니다. 진화하면 보호막을 생성합니다."),
        TEXT("체력이 높은 적 사이로 번개를 튕깁니다. 진화하면 대상 주위에 폭풍을 남깁니다."),TEXT("최대 체력에 비례해 주변 적에게 피해를 줍니다. 진화하면 처치한 적이 폭발합니다."),
        TEXT("주위를 회전하는 투사체를 발사합니다. 진화하면 두 줄기로 공격합니다."),TEXT("주위를 회전하며 적에게 피해를 주고 밀어냅니다. 진화하면 영구 유지됩니다."),
        TEXT("유미봇이 적을 공격하고 경험치를 수집합니다. 진화하면 회복 아이템을 떨어뜨립니다."),TEXT("넓은 범위의 일반 적을 처치하고 강한 적에게 큰 피해를 줍니다. 진화하면 추가 경험치와 골드를 얻습니다.")};
    const auto Type=static_cast<EGeneralWeapon>(Weapon);const bool Evolved=IsGeneralWeaponEvolved(Weapon);
    const auto S=GetGeneralWeaponStats(Weapon,GetGeneralWeaponRank(Weapon),Evolved);const auto& Info=GetGeneralWeaponInfo(Weapon);
    FString Result=FString::Printf(TEXT("%s · %d레벨%s\n%s"),Evolved?Info.EvolvedName:Info.Name,GetGeneralWeaponRank(Weapon),Evolved?TEXT(" · 진화"):TEXT(""),Descriptions[Weapon]);
    if(Type==EGeneralWeapon::Cyclone&&Evolved) { Result+=TEXT("\n영구 유지 · 재사용 대기시간 없음"); }
    else {Result+=FString::Printf(TEXT("\n현재 발동 간격 %.2f초 · 남은 시간 %.1f초"),GetGeneralWeaponCooldown(Weapon),GetGeneralWeaponRemaining(Weapon));}
    if(Type==EGeneralWeapon::Trail||Type==EGeneralWeapon::Radiance||Type==EGeneralWeapon::Glove) {Result+=TEXT("\n발동 간격은 스킬 가속의 영향을 받지 않습니다.");}
    if(Type==EGeneralWeapon::IceArmor&&!Evolved) {Result+=TEXT("\n준비되면 피격 시 발동합니다.");}
    Result+=FString::Printf(TEXT("\n무기 기본 피해 %.0f · 피해량 배율 %.0f%%"),S.Damage,GetCombatDamageMultiplier()*100.f);
    if(S.bUsesProjectileStat) {int32 Shots=S.Projectiles+GetCombatBonusProjectiles();if(Type==EGeneralWeapon::Crescent||(Type==EGeneralWeapon::Glove&&Evolved)) {Shots*=2;}if(Type==EGeneralWeapon::Crossbow&&Evolved) {Shots+=30;}Result+=FString::Printf(TEXT("\n투사체 %d개"),Shots);}
    if(S.Duration>0.f&&!(Type==EGeneralWeapon::Cyclone&&Evolved)) {Result+=FString::Printf(TEXT("\n기본 지속시간 %.1f초 · 지속시간 배율 %.0f%%"),S.Duration,GetCombatDurationMultiplier()*100.f);}
    return Result;
}
FString AAnimaPreviewCharacter::GetPassiveTooltip() const
{
    static const TCHAR* Effects[]={TEXT("일정 수의 적을 처치하면 이동 속도와 스킬 가속이 증가합니다."),TEXT("일정 횟수의 공격마다 음파를 추가 발사합니다."),TEXT("추가 방어력·체력 재생·최대 체력에 비례해 공격 범위가 증가합니다."),TEXT("가까운 촉수 하나마다 초당 체력을 회복합니다."),TEXT("잃은 체력이 많을수록 이동 속도와 회복 효과가 증가합니다."),TEXT("이동으로 기류를 충전하고 충전이 완료되면 전용 무기가 발동합니다."),TEXT("이동으로 충전하고 충전이 완료되면 전용 무기가 발동합니다. 충전량에 따라 이동 속도가 증가합니다."),TEXT("레벨 상승에 따라 영혼 공격이 강화됩니다. 저주를 쌓아 적에게 추가 공격을 가합니다."),TEXT("전리품을 수집하면 전용 무기 공격이 강화됩니다. 획득 반경이 증가합니다.")};
    FString Result=FString::Printf(TEXT("캐릭터 패시브\n%s\n조건 충족 시 발동 · 별도 재사용 대기시간 없음"),Effects[FMath::Clamp(CharacterChoice,0,8)]);
    if(CharacterChoice==0) {Result+=FString::Printf(TEXT("\n발동까지 %d회 처치 · 강화 남은 시간 %.1f초"),GetJinxKillsUntilPassive(),GetJinxPassiveRemaining());}
    if(CharacterChoice==8) {Result+=FString::Printf(TEXT("\n강화 남은 시간 %.1f초"),GetXayahBuffRemaining());}
    return Result;
}
