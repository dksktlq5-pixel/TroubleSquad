#include "AnimaPreview.h"

#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

TArray<AActor*> AAnimaPreviewCharacter::FindCombatTargets(FVector Center, float Radius) const
{
    TArray<FOverlapResult> Hits;
    GetWorld()->OverlapMultiByObjectType(Hits, Center, FQuat::Identity,
        FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius));
    TArray<AActor*> Targets;
    for (const FOverlapResult& Hit : Hits)
    {
        AActor* Target = Hit.GetActor();
        if (IsValid(Target) && Target->ActorHasTag(TEXT("Enemy")))
        {
            Targets.AddUnique(Target);
        }
    }
    return Targets;
}

void AAnimaPreviewCharacter::HitAdditionalEnemy(AActor* Target, float Damage)
{
    if (!IsValid(Target))
    {
        return;
    }
    UGameplayStatics::ApplyDamage(Target, Damage + ConsumeSunlight(Target), GetController(), this,
        UDamageType::StaticClass());
}

bool AAnimaPreviewCharacter::SpendExtraCharge(float Now, float Recovery)
{
    if (ExtraSkillCharges <= 0)
    {
        return false;
    }
    --ExtraSkillCharges;
    if (ExtraChargeReadyAt <= 0.f)
    {
        ExtraChargeReadyAt = Now + Recovery;
    }
    JinxEReadyAt = ExtraSkillCharges > 0 ? Now + 1.f : ExtraChargeReadyAt;
    return true;
}

void AAnimaPreviewCharacter::SpawnTentacle(FVector Position, float Now)
{
    const float Durations[] = {8.f, 10.f, 10.f, 12.f, 12.f};
    const float Damages[] = {70.f, 70.f, 105.f, 105.f, 120.f};
    const bool bEvolved = IsSignatureWeaponEvolved();
    const bool bGiant = bEvolved && FMath::FRand() < .15f;
    CombatHazards.Add({Position, bGiant ? 1400.f : 800.f,
        bGiant ? 240.f : Damages[SignatureWeaponLevel - 1], Now + 3.f,
        Now + (bEvolved ? 16.f : Durations[SignatureWeaponLevel - 1]), 3.f});
    ShowSeraphineEffect(Position, bGiant ? 140.f : 80.f, false);
}

void AAnimaPreviewCharacter::FireAdditionalWeapon(float Now)
{
    const FVector Origin = GetActorLocation();
    const FVector Forward = GetActorForwardVector();
    if (CharacterChoice == 3)
    {
        static const float Damage[] = {125.f, 125.f, 200.f, 200.f, 275.f};
        JinxWeaponReadyAt = Now + 5.f - .25f * (SignatureWeaponLevel - 1);
        const FVector Center = Origin + Forward * 250.f;
        for (AActor* Target : FindCombatTargets(Center, 150.f))
        {
            HitAdditionalEnemy(Target, Damage[SignatureWeaponLevel - 1]);
        }
        SpawnTentacle(Center, Now);
        return;
    }
    if (CharacterChoice == 4)
    {
        const bool bFrenzied = Now < BriarFrenzyEnd;
        JinxWeaponReadyAt = Now + (2.f - .1f * (SignatureWeaponLevel - 1)) /
            (bFrenzied ? (bBriarUltimateFrenzy ? 2.f : 1.65f) : 1.f);
        const float Missing = 1.f - CombatHealth / FMath::Max(1.f, MaxCombatHealth);
        for (AActor* Target : FindCombatTargets(Origin, 300.f))
        {
            if (FVector::DotProduct(Forward, (Target->GetActorLocation() - Origin).GetSafeNormal2D()) < .25f)
            {
                continue;
            }
            HitAdditionalEnemy(Target, (25.f + 30.f * (SignatureWeaponLevel - 1) + .05f * MaxCombatHealth)
                * (1.f + .60f * Missing));
            if (IsSignatureWeaponEvolved() && IsValid(Target))
            {
                FBriarBleed& Bleed = BriarBleeds.FindOrAdd(Target);
                if (Bleed.Expirations.IsEmpty()) { Bleed.NextTick = Now + .25f; }
                Bleed.Expirations.Add(Now + 8.f);
            }
            if (bFrenzied)
            {
                CombatHealth = FMath::Min(MaxCombatHealth, CombatHealth + 18.f * Missing);
            }
        }
        ShowSeraphineEffect(Origin + Forward * 130.f, 145.f, true);
        return;
    }

    if (YasuoFlow < 100.f)
    {
        return;
    }
    YasuoFlow -= 100.f;
    static const int32 Counts[] = {1, 3, 3, 5, 5};
    static const float Damage[] = {120.f, 120.f, 150.f, 150.f, 180.f};
    const int32 Count = Counts[SignatureWeaponLevel - 1];
    const auto Targets = FindCombatTargets(Origin, 850.f);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FVector Direction = Forward.RotateAngleAxis((Index - (Count - 1) * .5f) * 12.f, FVector::UpVector);
        ShowJinxTracer(Origin, Origin + Direction * 850.f);
        for (AActor* Target : Targets)
        {
            if (!IsValid(Target)) { continue; }
            const FVector Offset = Target->GetActorLocation() - Origin;
            const float Along = FVector::DotProduct(Direction, Offset);
            if (Along >= 0.f && Along <= 850.f && (Offset - Direction * Along).SizeSquared2D() <= FMath::Square(65.f))
            {
                const bool bCrit = FMath::FRand() < .20f;
                HitAdditionalEnemy(Target, Damage[SignatureWeaponLevel - 1] * (bCrit ? 1.75f : 1.f));
                YasuoFlow += bCrit ? 3.f : 0.f;
            }
        }
        if (IsSignatureWeaponEvolved())
        {
            const FVector End = Origin + Direction * 850.f;
            CombatHazards.Add({End, 155.f, 62.5f, Now + .25f, Now + 2.5f, .25f});
            ShowSeraphineEffect(End, 155.f, false);
        }
    }
}

void AAnimaPreviewCharacter::CastAdditionalSkill(bool bUltimate, float Now, FVector Aim)
{
    const FVector Origin = GetActorLocation();
    const FVector Offset = Aim - Origin;
    const FVector Direction = Offset.IsNearlyZero() ? GetActorForwardVector() : Offset.GetSafeNormal2D();
    if (CharacterChoice == 3)
    {
        if (bUltimate)
        {
            JinxRReadyAt = Now + 90.f;
            for (AActor* Target : FindCombatTargets(Origin, 550.f)) { HitAdditionalEnemy(Target, 250.f); }
            const int32 Count = 5 + FMath::Clamp(IllaoiBonusProjectiles, 0, 12);
            for (int32 Index = 0; Index < Count; ++Index)
            {
                SpawnTentacle(Origin + Direction.RotateAngleAxis(360.f * Index / Count, FVector::UpVector) * 390.f, Now);
            }
            ShowSeraphineEffect(Origin, 550.f, true);
        }
        else if (SpendExtraCharge(Now, 12.f))
        {
            const FVector Center = Origin + Direction * 650.f;
            for (AActor* Target : FindCombatTargets(Origin + Direction * 330.f, 435.f))
            {
                const FVector Relative = Target->GetActorLocation() - Origin;
                const float Along = FVector::DotProduct(Direction, Relative);
                if (Along < 0.f || Along > 700.f || (Relative - Direction * Along).SizeSquared2D() > FMath::Square(120.f))
                {
                    continue;
                }
                if (ACharacter* Enemy = Cast<ACharacter>(Target)) { Enemy->LaunchCharacter(FVector(0, 0, 480), false, true); }
                HitAdditionalEnemy(Target, 145.f);
            }
            SpawnTentacle(Center, Now);
            ShowJinxTracer(Origin, Center);
        }
        return;
    }
    if (CharacterChoice == 4)
    {
        if (!bUltimate)
        {
            JinxEReadyAt = Now + 20.f;
            BriarFrenzyEnd = Now + 6.f;
            bBriarUltimateFrenzy = false;
            LaunchCharacter(Direction * 850.f, true, true);
            return;
        }

        if (BriarFrenzyEnd > Now)
        {
            return;
        }
        JinxRReadyAt = Now + 120.f;
        const FVector Destination = Origin + Direction * FMath::Min(Offset.Size2D(), 3600.f);
        ShowJinxTracer(Origin, Destination);
        FTimerHandle Arrival;
        GetWorldTimerManager().SetTimer(Arrival, FTimerDelegate::CreateWeakLambda(this, [this, Destination]()
        {
            if (CharacterChoice != 4 || CombatHealth <= 0.f) { return; }
            const float ArrivalTime = GetCombatTime();
            InvulnerableEnd = FMath::Max(InvulnerableEnd, ArrivalTime + .25f);
            SetActorLocation(Destination, true);
            for (AActor* Target : FindCombatTargets(GetActorLocation(), 440.f)) { HitAdditionalEnemy(Target, 220.f); }
            BriarFrenzyEnd = ArrivalTime + 6.f;
            bBriarUltimateFrenzy = true;
            ShowSeraphineEffect(GetActorLocation(), 440.f, true);
        }), FMath::Max(.05f, FVector::Dist2D(Origin, Destination) / 2400.f), false);
        return;
    }

    if (bUltimate)
    {
        JinxRReadyAt = Now + 25.f;
        WindWallCenter = Origin + Direction * 260.f;
        WindWallNormal = Direction;
        WindWallEnd = Now + 4.f;
        const FVector Side = FVector::CrossProduct(FVector::UpVector, Direction) * 360.f;
        ShowJinxTracer(WindWallCenter - Side, WindWallCenter + Side);
    }
    else if (SpendExtraCharge(Now, 10.f))
    {
        SeraphineShield = FMath::Max(Now < SeraphineShieldEnd ? SeraphineShield : 0.f, 150.f + .10f * MaxCombatHealth);
        SeraphineShieldEnd = Now + 2.f;
        InvulnerableEnd = FMath::Max(InvulnerableEnd, Now + .25f);
        YasuoFlow = FMath::Min(100.f, YasuoFlow + 15.f);
        SetActorLocation(Origin + Direction * 370.f, true);
        for (AActor* Target : FindCombatTargets(GetActorLocation(), 260.f)) { HitAdditionalEnemy(Target, 135.f); }
        ShowSeraphineEffect(GetActorLocation(), 260.f, false);
    }
}

void AAnimaPreviewCharacter::FinishBriarFrenzy(float Now)
{
    BriarFrenzyEnd = 0.f;
    JinxEReadyAt = FMath::Max(JinxEReadyAt, Now);
    CombatHealth = FMath::Min(MaxCombatHealth, CombatHealth + (MaxCombatHealth - CombatHealth) * .20f);
    for (AActor* Target : FindCombatTargets(GetActorLocation(), 360.f)) { HitAdditionalEnemy(Target, 110.f); }
    ShowSeraphineEffect(GetActorLocation(), 360.f, true);
}

void AAnimaPreviewCharacter::TickAdditionalCombat(float Now, float DeltaTime)
{
    if (ExtraChargeReadyAt > 0.f && Now >= ExtraChargeReadyAt)
    {
        const int32 Capacity = CharacterChoice == 3 ? 2 : 3;
        if (++ExtraSkillCharges < Capacity) { ExtraChargeReadyAt = Now + (CharacterChoice == 3 ? 12.f : 10.f); }
        else { ExtraChargeReadyAt = 0.f; }
    }
    for (int32 Index = CombatHazards.Num() - 1; Index >= 0; --Index)
    {
        FCombatHazard& Hazard = CombatHazards[Index];
        if (Now > Hazard.End)
        {
            CombatHazards.RemoveAtSwap(Index);
            continue;
        }
        if (CharacterChoice == 3 && CombatHealth > 0.f && FVector::DistSquared2D(GetActorLocation(), Hazard.Position) <= FMath::Square(800.f))
        {
            CombatHealth = FMath::Min(MaxCombatHealth, CombatHealth + 2.f * DeltaTime);
        }
        if (Now >= Hazard.NextTick)
        {
            Hazard.NextTick += Hazard.Interval;
            for (AActor* Target : FindCombatTargets(Hazard.Position, Hazard.Radius)) { HitAdditionalEnemy(Target, Hazard.Damage); }
            ShowSeraphineEffect(Hazard.Position, Hazard.Radius, false);
        }
    }
    if (CharacterChoice == 5)
    {
        YasuoFlow = FMath::Min(100.f, YasuoFlow + (GetVelocity().SizeSquared2D() > 25.f ? 35.f : 25.f) * DeltaTime);
        if (Now < WindWallEnd)
        {
            TArray<FOverlapResult> Projectiles;
            FCollisionObjectQueryParams ProjectileObjects;
            ProjectileObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
            ProjectileObjects.AddObjectTypesToQuery(ECC_PhysicsBody);
            GetWorld()->OverlapMultiByObjectType(Projectiles, WindWallCenter, WindWallNormal.Rotation().Quaternion(),
                ProjectileObjects, FCollisionShape::MakeBox(FVector(40, 360, 135)));
            for (const FOverlapResult& Hit : Projectiles)
            {
                AActor* Projectile = Hit.GetActor();
                if (IsValid(Projectile) && Projectile->ActorHasTag(TEXT("EnemyProjectile")))
                {
                    Projectile->Destroy();
                }
            }
            for (AActor* Target : FindCombatTargets(WindWallCenter, 370.f))
            {
                if (Target->ActorHasTag(TEXT("FinalBoss"))) { continue; }
                const FVector Relative = Target->GetActorLocation() - WindWallCenter;
                const float Distance = FVector::DotProduct(Relative, WindWallNormal);
                if (FMath::Abs(Distance) < 38.f)
                {
                    if (ACharacter* Enemy = Cast<ACharacter>(Target))
                    {
                        Enemy->GetCharacterMovement()->StopMovementImmediately();
                        Enemy->AddActorWorldOffset(WindWallNormal * (Distance >= 0.f ? 12.f : -12.f), true);
                    }
                }
            }
        }
    }
    if (CharacterChoice != 4) { return; }
    if (BriarFrenzyEnd > 0.f && Now >= BriarFrenzyEnd)
    {
        FinishBriarFrenzy(Now);
    }
    else if (BriarFrenzyEnd > Now)
    {
        AActor* Closest = nullptr;
        float Distance = TNumericLimits<float>::Max();
        for (AActor* Target : FindCombatTargets(GetActorLocation(), 1250.f))
        {
            const float Candidate = FVector::DistSquared2D(GetActorLocation(), Target->GetActorLocation());
            if (Candidate < Distance) { Closest = Target; Distance = Candidate; }
        }
        if (Closest)
        {
            const FVector Direction = (Closest->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
            SetActorRotation(Direction.Rotation());
            if (Distance > FMath::Square(135.f)) { AddMovementInput(Direction, 1.f); }
        }
    }
    for (auto It = BriarBleeds.CreateIterator(); It; ++It)
    {
        AActor* Target = It.Key().Get();
        FBriarBleed& Bleed = It.Value();
        Bleed.Expirations.RemoveAll([Now](float Expiration) { return Expiration <= Now; });
        if (!IsValid(Target) || Bleed.Expirations.IsEmpty()) { It.RemoveCurrent(); continue; }
        if (Now >= Bleed.NextTick)
        {
            Bleed.NextTick += .25f;
            HitAdditionalEnemy(Target, 10.f * Bleed.Expirations.Num());
        }
    }
}

void AAnimaPreviewCharacter::CastRivenSkill(bool bUltimate, float Now)
{
    if (bUltimate)
    {
        JinxRReadyAt = Now + 100.f;
        RivenUltimateEnd = Now + 15.f * RivenDurationMultiplier;
        ShowSeraphineEffect(GetActorLocation(), 180.f, false);
    }
    else if (!bRivenDashing && CombatHealth > 0.f)
    {
        JinxEReadyAt = Now + 8.f;
        BeginRivenDash(Now, true);
    }
}

void AAnimaPreviewCharacter::BeginRivenDash(float Now, bool bSkill)
{
    bRivenDashing = true;
    bRivenSkillDash = bSkill;
    RivenDashDirection = GetActorForwardVector().GetSafeNormal2D();
    RivenDashDistance = bSkill ? 280.f : 235.f;
    if (bSkill)
    {
        InvulnerableEnd = FMath::Max(InvulnerableEnd, Now + .28f);
    }
    GetCharacterMovement()->StopMovementImmediately();
}

void AAnimaPreviewCharacter::HitRivenEnemy(AActor* Target, float Damage, float Now)
{
    if (!IsValid(Target))
    {
        return;
    }
    const float Range = FVector::Dist2D(Target->GetActorLocation(), GetActorLocation());
    const float Bonus = .15f * (1.f - FMath::Clamp(Range / 330.f, 0.f, 1.f));
    const float Applied = UGameplayStatics::ApplyDamage(Target, Damage * (1.f + Bonus) + ConsumeSunlight(Target),
        GetController(), this, UDamageType::StaticClass());
    if (Applied > 0.f)
    {
        RivenShield = (Now < RivenShieldEnd ? RivenShield : 0.f)
            + FMath::Min(Applied * .25f, MaxCombatHealth * .02f);
        RivenShieldEnd = Now + RivenDurationMultiplier;
    }
}

void AAnimaPreviewCharacter::FinishRivenDash(float Now)
{
    bRivenDashing = false;
    const FVector Origin = GetActorLocation();
    if (!bRivenSkillDash)
    {
        ++RivenLeapCount;
    }
    for (AActor* Target : FindCombatTargets(Origin, 250.f))
    {
        HitRivenEnemy(Target, bRivenSkillDash ? 110.f : 55.f + 20.f * SignatureWeaponLevel
            + (IsSignatureWeaponEvolved() ? 45.f : 0.f), Now);
        if (!IsValid(Target)) { continue; }
        ACharacter* Enemy = Cast<ACharacter>(Target);
        if (!Enemy) { continue; }
        if (bRivenSkillDash)
        {
            UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement();
            const EMovementMode Mode = Movement->MovementMode;
            const uint8 Custom = Movement->CustomMovementMode;
            if (Mode != MOVE_None)
            {
                Movement->StopMovementImmediately();
                Movement->DisableMovement();
                FTimerHandle Stun;
                GetWorldTimerManager().SetTimer(Stun, FTimerDelegate::CreateWeakLambda(Movement,
                    [Movement, Mode, Custom]()
                    {
                        if (Movement->MovementMode == MOVE_None) { Movement->SetMovementMode(Mode, Custom); }
                    }), 2.f * RivenDurationMultiplier, false);
            }
        }
        else if (RivenLeapCount % 2 == 0)
        {
            Enemy->LaunchCharacter(FVector(0, 0, 420), false, true);
        }
    }
    if (!bRivenSkillDash && IsSignatureWeaponEvolved())
    {
        RivenAfterimageLocation = Origin;
        RivenAfterimageAt = Now + .45f;
    }
    if (Now < RivenUltimateEnd)
    {
        const int32 Count = 3 + FMath::Clamp(RivenBonusProjectiles, 0, 12);
        const auto Targets = FindCombatTargets(Origin, 700.f);
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const FVector Direction = RivenDashDirection.RotateAngleAxis((Index - (Count - 1) * .5f) * 9.f, FVector::UpVector);
            ShowJinxTracer(Origin, Origin + Direction * 700.f);
            for (AActor* Target : Targets)
            {
                if (!IsValid(Target)) { continue; }
                const FVector Offset = Target->GetActorLocation() - Origin;
                const float Along = FVector::DotProduct(Direction, Offset);
                if (Along > 0.f && Along <= 700.f && (Offset - Along * Direction).SizeSquared2D() < FMath::Square(65.f))
                {
                    HitRivenEnemy(Target, 85.f, Now);
                }
            }
        }
    }
    ShowSeraphineEffect(Origin, 250.f, false);
}

void AAnimaPreviewCharacter::TickRivenCombat(float Now, float DeltaTime)
{
    if (CombatHealth <= 0.f)
    {
        bRivenDashing = false;
        return;
    }
    if (RivenAfterimageAt > 0.f && Now >= RivenAfterimageAt)
    {
        RivenAfterimageAt = 0.f;
        for (AActor* Target : FindCombatTargets(RivenAfterimageLocation, 250.f))
        {
            if (ACharacter* Enemy = Cast<ACharacter>(Target)) { Enemy->LaunchCharacter(FVector(0, 0, 420), false, true); }
            HitRivenEnemy(Target, 105.f, Now);
        }
        ShowSeraphineEffect(RivenAfterimageLocation, 250.f, true);
    }
    if (bRivenDashing)
    {
        const float Step = FMath::Min(RivenDashDistance, 1450.f * DeltaTime);
        FHitResult Hit;
        AddActorWorldOffset(RivenDashDirection * Step, true, &Hit);
        RivenDashDistance -= Step;
        if (RivenDashDistance <= KINDA_SMALL_NUMBER || Hit.bBlockingHit)
        {
            FinishRivenDash(Now);
        }
        return;
    }

    const float ChargeRate = 25.f + 4.f * (SignatureWeaponLevel - 1);
    RivenCharge = FMath::Min(100.f, RivenCharge + ChargeRate * DeltaTime
        * FMath::Clamp(GetVelocity().Size2D() / FMath::Max(BaseMoveSpeed, 1.f), 0.f, 2.f));
    if (RivenCharge >= 100.f)
    {
        RivenCharge = 0.f;
        BeginRivenDash(Now, false);
    }
}

void AAnimaPreviewCharacter::SetCombatLevel(int32 NewLevel)
{
    if (!HasAuthority() || NewLevel <= CombatLevel) { return; }
    const int32 SpiritCount = NewLevel / 7 - CombatLevel / 7;
    CombatLevel = NewLevel;
    if (CharacterChoice != 7 || SpiritCount <= 0) { return; }
    const FVector Origin = GetActorLocation();
    auto Targets = FindCombatTargets(Origin, 700.f);
    Targets.Sort([Origin](const AActor& A, const AActor& B)
    {
        return FVector::DistSquared(A.GetActorLocation(), Origin) < FVector::DistSquared(B.GetActorLocation(), Origin);
    });
    for (int32 Index = 0; Index < FMath::Min(SpiritCount, Targets.Num()); ++Index)
    {
        AActor* Target = Targets[Index];
        ShowJinxTracer(Origin, Target->GetActorLocation());
        CurseAuroraEnemy(Target, 80.f, GetCombatTime());
    }
}

void AAnimaPreviewCharacter::ServerToggleAuroraAutoAim_Implementation()
{
    if (CharacterChoice == 7) { bAuroraAutoAim = !bAuroraAutoAim; }
}

void AAnimaPreviewCharacter::CurseAuroraEnemy(AActor* Target, float Damage, float Now)
{
    HitAdditionalEnemy(Target, Damage);
    if (IsValid(Target)) { AuroraCurses.FindOrAdd(Target) = Now + 6.f * AuroraDurationMultiplier; }
}

void AAnimaPreviewCharacter::FireAuroraWeapon(float Now)
{
    JinxWeaponReadyAt = Now + 2.8f - .2f * (SignatureWeaponLevel - 1);
    const bool bEvolved = IsSignatureWeaponEvolved();
    if (++AuroraWeaponCasts % 3 == 0)
    {
        TArray<TWeakObjectPtr<AActor>> Cursed;
        AuroraCurses.GenerateKeyArray(Cursed);
        AuroraCurses.Reset();
        for (const auto& WeakTarget : Cursed)
        {
            if (AActor* Target = WeakTarget.Get())
            {
                const FVector Center = Target->GetActorLocation();
                HitAdditionalEnemy(Target, 90.f);
                if (bEvolved)
                {
                    for (AActor* Nearby : FindCombatTargets(Center, 220.f))
                    {
                        if (Nearby != Target) { HitAdditionalEnemy(Nearby, 65.f); }
                    }
                }
                ShowSeraphineEffect(Center, bEvolved ? 220.f : 65.f, true);
            }
        }
    }
    const int32 Count = bEvolved ? 6 : SignatureWeaponLevel;
    const FVector Origin = GetActorLocation() - FVector(0, 0, 22);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const float Spread = (Index - (Count - 1) * .5f) * 9.f;
        const FVector Direction = GetActorForwardVector().RotateAngleAxis(Spread, FVector::UpVector);
        AuroraBolts.Add({Origin, Direction, 1250.f, 45.f + 10.f * SignatureWeaponLevel,
            bEvolved || !bAuroraAutoAim});
    }
}

void AAnimaPreviewCharacter::CastAuroraSkill(bool bUltimate, float Now)
{
    const FVector Origin = GetActorLocation();
    if (!bUltimate)
    {
        JinxEReadyAt = Now + 15.f;
        AuroraVeilEnd = Now + 3.f * AuroraDurationMultiplier;
        InvulnerableEnd = FMath::Max(InvulnerableEnd, AuroraVeilEnd);
        AuroraContactHits.Reset();
        ShowSeraphineEffect(Origin, 120.f, false);
        return;
    }
    JinxRReadyAt = Now + 90.f;
    AuroraRealmCenter = Origin;
    AuroraRealmEnd = Now + 8.f * AuroraDurationMultiplier;
    AuroraCrossingReady = Now + .4f;
    for (AActor* Target : FindCombatTargets(Origin, 650.f)) { HitAdditionalEnemy(Target, 180.f); }
    ShowSeraphineEffect(Origin, 650.f, true);
}

void AAnimaPreviewCharacter::TickAuroraCombat(float Now, float DeltaTime)
{
    for (auto It = AuroraCurses.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid() || It.Value() <= Now) { It.RemoveCurrent(); }
    }
    for (int32 Index = AuroraBolts.Num() - 1; Index >= 0; --Index)
    {
        auto& Bolt = AuroraBolts[Index];
        if (Bolt.bHoming && !AuroraAimPoint.IsNearlyZero())
        {
            const FVector Desired = (AuroraAimPoint - Bolt.Position).GetSafeNormal2D();
            if (!Desired.IsNearlyZero())
            {
                Bolt.Direction = FMath::VInterpTo(Bolt.Direction, Desired, DeltaTime, 5.f).GetSafeNormal();
            }
        }
        const float Step = FMath::Min(Bolt.Remaining, DeltaTime * (Bolt.bHoming ? 1050.f : 1900.f));
        const FVector End = Bolt.Position + Bolt.Direction * Step;
        const FVector Mid = (Bolt.Position + End) * .5f;
        auto Targets = FindCombatTargets(Mid, Step * .5f + 35.f);
        if (!Targets.IsEmpty())
        {
            CurseAuroraEnemy(Targets[0], Bolt.Damage, Now);
        }
        if (GetNetMode() != NM_DedicatedServer)
        {
            DrawDebugLine(GetWorld(), Bolt.Position, End, FColor::Cyan, false, .08f, 0, 2.f);
        }
        Bolt.Position = End;
        Bolt.Remaining -= Step;
        if (Bolt.Remaining <= KINDA_SMALL_NUMBER || !Targets.IsEmpty()) { AuroraBolts.RemoveAtSwap(Index); }
    }
    if (Now < AuroraVeilEnd)
    {
        for (AActor* Target : FindCombatTargets(GetActorLocation(), 85.f))
        {
            if (!AuroraContactHits.Contains(Target))
            {
                AuroraContactHits.Add(Target);
                CurseAuroraEnemy(Target, 95.f, Now);
            }
        }
    }
    if (Now < AuroraRealmEnd && Now >= AuroraCrossingReady)
    {
        const FVector Offset = GetActorLocation() - AuroraRealmCenter;
        if (Offset.SizeSquared2D() >= FMath::Square(650.f))
        {
            const FVector Destination = AuroraRealmCenter - Offset.GetSafeNormal2D() * 530.f;
            const FVector Previous = GetActorLocation();
            if (TeleportTo(FVector(Destination.X, Destination.Y, Previous.Z), GetActorRotation()))
            {
                for (AActor* Target : FindCombatTargets(GetActorLocation(), 210.f))
                {
                    if (ACharacter* Enemy = Cast<ACharacter>(Target))
                    {
                        Enemy->LaunchCharacter((Enemy->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() * 550.f, true, true);
                    }
                    HitAdditionalEnemy(Target, 100.f);
                }
                ShowSeraphineEffect(GetActorLocation(), 210.f, true);
            }
            AuroraCrossingReady = Now + .5f;
        }
    }
}

void AAnimaPreviewCharacter::PlayCombatMotion_Implementation(uint8 Motion)
{
    CurrentCombatMotion = Motion;
    CombatMotionTime = Motion == 0 ? .23f : .34f;
}

void AAnimaPreviewCharacter::UpdateCombatMotion(float DeltaTime, bool bMoving)
{
    if (GetNetMode() == NM_DedicatedServer || !GetMesh()->GetSkeletalMeshAsset()) { return; }
    GetMesh()->SetPlayRate(bMoving ? FMath::Clamp(GetVelocity().Size2D() / FMath::Max(1.f, BaseMoveSpeed), .65f, 1.7f) : 1.f);
    const bool bWasActing = CombatMotionTime > 0.f;
    CombatMotionTime = FMath::Max(0.f, CombatMotionTime - DeltaTime);
    if (!bWasActing) { return; }
    static const float Lean[] = {-8.f, 4.f, 12.f, 15.f, 18.f, 8.f, 14.f, -4.f, 7.f};
    static const float Twist[] = {0.f, 8.f, 12.f, -14.f, 20.f, -22.f, 16.f, -9.f, -15.f};
    const int32 Index = FMath::Clamp(CharacterChoice, 0, 8);
    const float Duration = CurrentCombatMotion == 0 ? .23f : .34f;
    const float Weight = FMath::Sin(UE_PI * CombatMotionTime / Duration);
    const float Strength = CurrentCombatMotion == 0 ? 1.f : 1.35f;
    const float Recoil = Index == 0 ? -15.f : Index == 1 || Index == 7 ? 0.f : 10.f;
    GetMesh()->SetRelativeLocation(BaseMeshLocation + FVector(Recoil * Weight, 0, 3.f * Strength * Weight));
    GetMesh()->SetRelativeRotation(BaseMeshRotation + FRotator(Lean[Index] * Strength * Weight, Twist[Index] * Weight, 0));
}

float AAnimaPreviewCharacter::GetXayahBuffRemaining() const
{
    return CharacterChoice == 8 ? FMath::Max(0.f, XayahBuffEnd - GetCombatTime()) : 0.f;
}

float AAnimaPreviewCharacter::GetLootPickupRadiusMultiplier() const
{
    return CharacterChoice == 8 ? (GetCombatTime() < XayahPickupEnd ? 3.f : 1.25f) : 1.f;
}

float AAnimaPreviewCharacter::GetXayahCooldown(float BaseCooldown) const
{
    return GetXayahBuffRemaining() > 0.f ? BaseCooldown / 1.8f : BaseCooldown;
}

void AAnimaPreviewCharacter::RegisterLootPickup(FVector LootOrigin, bool bExperienceOrGold)
{
    if (!HasAuthority() || CharacterChoice != 8 || LootOrigin.ContainsNaN())
    {
        return;
    }

    if (++XayahLootCount % 200 == 0)
    {
        XayahBuffEnd = GetCombatTime() + 15.f;
    }
    const FVector Destination = GetActorLocation();
    if (bExperienceOrGold)
    {
        HitXayahPath(LootOrigin, Destination, 18.f + 5.f * CombatLevel, false);
    }
}

void AAnimaPreviewCharacter::HitXayahPath(FVector Start, FVector End, float Damage, bool bDecay)
{
    TArray<FHitResult> Hits;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(XayahFeather), false, this);
    GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity,
        FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(42.f), Query);
    TSet<AActor*> Affected;
    for (const FHitResult& Hit : Hits)
    {
        AActor* Target = Hit.GetActor();
        if (!IsValid(Target) || !Target->ActorHasTag(TEXT("Enemy")) || Affected.Contains(Target)) { continue; }
        Affected.Add(Target);
        HitAdditionalEnemy(Target, Damage + (GetXayahBuffRemaining() > 0.f ? 12.f + 4.f * CombatLevel : 0.f));
        if (bDecay) { Damage *= .75f; }
    }
    ShowJinxTracer(Start, End);
}

void AAnimaPreviewCharacter::ThrowXayahFeather(FVector Direction, float Damage, float Now)
{
    const FVector Start = GetActorLocation() - FVector(0, 0, 24);
    const FVector End = Start + Direction * 1050.f;
    XayahFeathers.Add({Start, End, {}, Damage, Now + 7.f * XayahDurationMultiplier});
}

void AAnimaPreviewCharacter::TickXayahFeathers(float Now, float DeltaTime)
{
    for (int32 Index = XayahFeathers.Num() - 1; Index >= 0; --Index)
    {
        auto& Feather = XayahFeathers[Index];
        if (Now >= Feather.ExpiresAt)
        {
            XayahFeathers.RemoveAtSwap(Index);
            continue;
        }
        if (Feather.bLanded && !Feather.bReturning) { continue; }
        const FVector Destination = Feather.bReturning ? GetActorLocation() - FVector(0, 0, 24) : Feather.Destination;
        const FVector Next = FMath::VInterpConstantTo(Feather.Position, Destination, DeltaTime, Feather.bReturning ? 2600.f : 2100.f);
        TArray<FHitResult> Hits;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(XayahProjectile), false, this);
        GetWorld()->SweepMultiByObjectType(Hits, Feather.Position, Next, FQuat::Identity,
            FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(38.f), Query);
        for (const FHitResult& Hit : Hits)
        {
            AActor* Target = Hit.GetActor();
            if (!IsValid(Target) || !Target->ActorHasTag(TEXT("Enemy")) || Feather.HitActors.Contains(Target)) { continue; }
            Feather.HitActors.Add(Target);
            HitAdditionalEnemy(Target, Feather.Damage + (GetXayahBuffRemaining() > 0.f ? 12.f + 4.f * CombatLevel : 0.f));
            if (!Feather.bReturning) { Feather.Damage *= .75f; }
        }
        if (GetNetMode() != NM_DedicatedServer)
        {
            DrawDebugLine(GetWorld(), Feather.Position, Next, FColor::Magenta, false, .10f, 0, 2.f);
        }
        Feather.Position = Next;
        if (Next.Equals(Destination, 1.f))
        {
            if (Feather.bReturning) { XayahFeathers.RemoveAtSwap(Index); }
            else
            {
                Feather.bLanded = true;
                ShowSeraphineEffect(Next, 36.f, true);
            }
        }
    }
}

void AAnimaPreviewCharacter::FireXayahWeapon(float Now)
{
    JinxWeaponReadyAt = Now + GetXayahCooldown(2.7f - .18f * (SignatureWeaponLevel - 1));
    const FVector Forward = GetActorForwardVector();
    for (int32 Index = 0; Index < SignatureWeaponLevel; ++Index)
    {
        const float Angle = (Index - (SignatureWeaponLevel - 1) * .5f) * 8.f;
        ThrowXayahFeather(Forward.RotateAngleAxis(Angle, FVector::UpVector), 60.f + SignatureWeaponLevel * 12.f, Now);
    }
    if (!IsSignatureWeaponEvolved()) { return; }
    const FVector Origin = GetActorLocation();
    FVector Last = Origin - Forward * 300.f;
    ShowJinxTracer(Origin, Last);
    int32 Bounces = 0;
    for (AActor* Target : FindCombatTargets(Last, 600.f))
    {
        if (!IsValid(Target)) { continue; }
        const FVector Next = Target->GetActorLocation();
        ShowJinxTracer(Last, Next);
        Last = Next;
        HitAdditionalEnemy(Target, 95.f + (GetXayahBuffRemaining() > 0.f ? 12.f + 4.f * CombatLevel : 0.f));
        if (++Bounces >= 4) { break; }
    }
    ShowJinxTracer(Last, Origin);
    RivenShield = FMath::Max(Now < RivenShieldEnd ? RivenShield : 0.f, MaxCombatHealth * .06f);
    RivenShieldEnd = Now + 2.f * XayahDurationMultiplier;
}

void AAnimaPreviewCharacter::CastXayahSkill(bool bUltimate, float Now)
{
    if (!bUltimate)
    {
        JinxEReadyAt = Now + GetXayahCooldown(13.f);
        XayahPickupEnd = Now + 3.f * XayahDurationMultiplier;
        for (auto& Feather : XayahFeathers)
        {
            Feather.bReturning = true;
            Feather.HitActors.Reset();
            Feather.Damage = 75.f;
            Feather.ExpiresAt = FMath::Max(Feather.ExpiresAt, Now + 2.f);
        }
        ShowSeraphineEffect(GetActorLocation(), 275.f, false);
        return;
    }
    JinxRReadyAt = Now + GetXayahCooldown(90.f);
    JinxEReadyAt = Now;
    InvulnerableEnd = FMath::Max(InvulnerableEnd, Now + .8f);
    LaunchCharacter(FVector(0, 0, 360), false, true);
    for (int32 Index = 0; Index < 16; ++Index)
    {
        const FVector Direction = GetActorForwardVector().RotateAngleAxis(22.5f * Index, FVector::UpVector);
        ThrowXayahFeather(Direction, 95.f, Now);
    }
}
