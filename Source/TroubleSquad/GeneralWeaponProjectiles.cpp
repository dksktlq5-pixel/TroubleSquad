#include "AnimaPreview.h"
#include "Engine/World.h"

void AAnimaPreviewCharacter::LaunchGeneralProjectile(uint8 Weapon,FVector Start,FVector End,float Width,float Damage,bool bFirstOnly)
{
    const FVector Delta=End-Start;
    if(Delta.SizeSquared2D()<1.f) { return; }
    const auto Type=static_cast<EGeneralWeapon>(Weapon);
    const float Speed=Type==EGeneralWeapon::Launcher?2800.f:Type==EGeneralWeapon::Crossbow?2100.f
        :Type==EGeneralWeapon::Glove?1800.f:Type==EGeneralWeapon::Blade?1650.f:1800.f;
    if(GeneralProjectiles.Num()>=256) { GeneralProjectiles.RemoveAt(0,1,EAllowShrinking::No); }
    FGeneralProjectile P;
    P.Weapon=Weapon; P.Position=Start; P.LastVisual=Start; P.Direction=Delta.GetSafeNormal2D();
    P.Remaining=Delta.Size2D(); P.Damage=Damage; P.Radius=Width; P.Speed=Speed;
    P.bFirstOnly=bFirstOnly; P.bEvolved=IsGeneralWeaponEvolved(Weapon);
    P.bCanSplit=Type==EGeneralWeapon::Boomerang&&P.bEvolved;
    GeneralProjectiles.Add(MoveTemp(P));
}

void AAnimaPreviewCharacter::TickGeneralProjectiles(float Now,float DeltaTime)
{
    TArray<FGeneralProjectile> SplitProjectiles;
    for(int32 I=GeneralProjectiles.Num()-1;I>=0;--I)
    {
        FGeneralProjectile& P=GeneralProjectiles[I];
        if(P.bReturning)
        {
            if(FVector::DistSquared2D(P.Position,GetActorLocation())<=FMath::Square(30.f)) { GeneralProjectiles.RemoveAtSwap(I,1,EAllowShrinking::No); continue; }
            P.Direction=(GetActorLocation()-P.Position).GetSafeNormal2D();
        }
        const float Step=FMath::Min(P.Remaining,FMath::Min(P.Speed*DeltaTime,P.bReturning?FVector::Dist2D(P.Position,GetActorLocation()):TNumericLimits<float>::Max()));
        FVector End=P.Position+P.Direction*Step;
        const auto Type=static_cast<EGeneralWeapon>(P.Weapon);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(GeneralWeaponFlight),false,this);
        FHitResult Wall;
        const bool bWall=Type==EGeneralWeapon::Blade && GetWorld()->LineTraceSingleByObjectType(Wall,P.Position,End,
            FCollisionObjectQueryParams(ECC_WorldStatic),Query);
        if(bWall) { End=Wall.ImpactPoint; }
        for(const auto& Hit:P.HitActors) { if(Hit.IsValid()) { Query.AddIgnoredActor(Hit.Get()); } }
        TArray<FHitResult> Hits;
        GetWorld()->SweepMultiByObjectType(Hits,P.Position,End,FQuat::Identity,FCollisionObjectQueryParams(ECC_Pawn),
            FCollisionShape::MakeSphere(P.Radius),Query);
        Hits.Sort([](const FHitResult& A,const FHitResult& B) { return A.Time<B.Time; });
        bool bFinished=false;
        for(const FHitResult& Hit:Hits)
        {
            AActor* Target=Hit.GetActor();
            if(!IsValid(Target)||!Target->ActorHasTag(TEXT("Enemy"))||P.HitActors.Contains(Target)) { continue; }
            P.HitActors.Add(Target);
            const bool bCritical=ApplyGeneralWeaponDamage(Target,P.Damage,P.Weapon,true,false);
            if(P.bCanSplit) { End=Hit.Location; bFinished=true; break; }
            if(P.bFirstOnly || (Type==EGeneralWeapon::Crossbow && !bCritical))
            {
                End=Hit.Location; bFinished=true; break;
            }
        }
        if(Now>=P.NextVisualAt || bFinished || bWall || P.Remaining<=Step)
        {
            ShowWeaponVfx(P.Weapon,P.LastVisual,End,P.Radius,0,P.bEvolved);
            P.LastVisual=End; P.NextVisualAt=Now+.045f;
        }
        P.Remaining-=FVector::Dist2D(P.Position,End);
        P.Position=End;
        if(P.bCanSplit&&(bFinished||P.Remaining<=1.f))
        {
            ShowWeaponVfx(P.Weapon,End,End,140.f,1,true);
            for(int32 J=-1;J<=1;++J)
            {
                FGeneralProjectile Child=P; Child.Position=Child.LastVisual=End;
                Child.Direction=P.Direction.RotateAngleAxis(J*20.f,FVector::UpVector);
                Child.Remaining=400.f; Child.Speed=2000.f; Child.Radius=P.Radius*.6f; Child.Damage=P.Damage*.45f;
                Child.bCanSplit=false; Child.bFirstOnly=true; Child.NextVisualAt=0.f;
                SplitProjectiles.Add(MoveTemp(Child));
            }
            bFinished=true;
        }
        if(Type==EGeneralWeapon::Boomerang&&!P.bEvolved&&!P.bReturning&&P.Remaining<=1.f&&!bFinished)
        {
            P.bReturning=true; P.Remaining=GetGeneralWeaponStats(P.Weapon,GetGeneralWeaponRank(P.Weapon),false).Range*2.f;
            P.HitActors.Reset(); P.LastVisual=P.Position; continue;
        }
        if(P.bReturning&&FVector::DistSquared2D(P.Position,GetActorLocation())<=FMath::Square(30.f)) { bFinished=true; }
        if(bWall && !bFinished)
        {
            ++P.Bounces;
            if(P.Bounces>=(P.bEvolved?5:3)) { bFinished=true; }
            else
            {
                P.Direction=FMath::GetReflectionVector(P.Direction,Wall.ImpactNormal).GetSafeNormal2D();
                if(P.Direction.IsNearlyZero()) { bFinished=true; }
                P.Position+=P.Direction*6.f;
                P.LastVisual=P.Position;
                P.Remaining=GetGeneralWeaponStats(P.Weapon,GetGeneralWeaponRank(P.Weapon),P.bEvolved).Range;
                P.HitActors.Reset();
                if(P.bEvolved) { P.Damage*=1.2f; }
            }
        }
        if(bFinished || P.Remaining<=1.f) { GeneralProjectiles.RemoveAtSwap(I,1,EAllowShrinking::No); }
    }
    for(auto& Child:SplitProjectiles)
    {
        if(GeneralProjectiles.Num()>=256) { GeneralProjectiles.RemoveAt(0,1,EAllowShrinking::No); }
        GeneralProjectiles.Add(MoveTemp(Child));
    }
}
