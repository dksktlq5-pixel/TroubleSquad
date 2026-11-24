#include "CombatVfx.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

void ACombatVfx::EmitModel(uint8 Kind,FVector Start,FVector End,float Size,float Lifetime,bool bTravel,bool bEvolved)
{
    if(GetNetMode()==NM_DedicatedServer||Kind>=7||Start.ContainsNaN()||End.ContainsNaN()||!FMath::IsFinite(Size)||!FMath::IsFinite(Lifetime)){return;}
    if(ModelPools.IsEmpty()){ModelPools.SetNum(7);}
    if(!ModelPools[Kind])
    {
        static const TCHAR* Names[]={TEXT("Train"),TEXT("Boomerang"),TEXT("Crescent"),TEXT("Blade"),TEXT("Tibber"),TEXT("Crossbow"),TEXT("Cyclone")};
        const FString Path=FString::Printf(TEXT("/Game/TroubleSquad/CombatVisuals/GeneralWeapons/Models/SM_GW_%s.SM_GW_%s"),Names[Kind],Names[Kind]);
        auto* Asset=LoadObject<UStaticMesh>(nullptr,*Path);if(!Asset){return;}
        auto* Pool=NewObject<UInstancedStaticMeshComponent>(this);Pool->SetStaticMesh(Asset);
        Pool->SetCollisionEnabled(ECollisionEnabled::NoCollision);Pool->SetCastShadow(false);Pool->SetCanEverAffectNavigation(false);
        Pool->SetMobility(EComponentMobility::Movable);Pool->RegisterComponent();ModelPools[Kind]=Pool;
        UE_LOG(LogTemp,Display,TEXT("General weapon model loaded: kind=%d asset=%s"),Kind,*Asset->GetName());
    }
    // Reuse the same physical projectile between its existing replicated trail snapshots.
    // Separate projectiles keep separate slots unless their segment endpoints coincide.
    if(bTravel&&Lifetime<.2f)
    {
        for(auto& E:ModelEffects)
        {
            if(E.Kind==Kind&&E.bTravel&&E.Lifetime<.2f&&FVector::DistSquared(E.End,Start)<64.f
                &&FVector::DotProduct((E.End-E.Start).GetSafeNormal2D(),(End-Start).GetSafeNormal2D())>.8f)
            {E.Start=Start;E.End=End;E.Age=0;E.Size=Size;E.Lifetime=Lifetime;return;}
        }
    }
    if(ModelEffects.Num()>=48){ModelEffects.RemoveAt(0,1,EAllowShrinking::No);}
    ModelEffects.Add({Kind,Start,End,FMath::Clamp(Size,20.f,480.f),0,FMath::Clamp(Lifetime,.05f,4.f),0,bTravel,bEvolved});
}

int32 ACombatVfx::GetModelInstanceCount() const
{
    int32 Count=0;for(const auto& Pool:ModelPools){if(Pool){Count+=Pool->GetInstanceCount();}}return Count;
}

void ACombatVfx::TickModels(float DeltaSeconds)
{
    TArray<FTransform> Transforms[7];
    for(int32 I=ModelEffects.Num()-1;I>=0;--I)
    {
        auto& E=ModelEffects[I];E.Age+=DeltaSeconds;E.SpinAge+=DeltaSeconds;
        if(E.Age>=E.Lifetime){ModelEffects.RemoveAtSwap(I,1,EAllowShrinking::No);continue;}
        if(!ModelPools.IsValidIndex(E.Kind)||!ModelPools[E.Kind]){continue;}
        FVector Position=E.bTravel?FMath::Lerp(E.Start,E.End,FMath::Clamp(E.Age/E.Lifetime,0.f,1.f)):E.Start;
        FVector Direction=(E.End-E.Start).GetSafeNormal2D();float Yaw=Direction.IsNearlyZero()?0:Direction.Rotation().Yaw;
        if(E.Kind==1||E.Kind==2||E.Kind==6){Yaw+=E.SpinAge*(E.Kind==6?720.f:540.f);}
        if(E.Kind==6&&GetOwner()&&!FParse::Param(FCommandLine::Get(),TEXT("WeaponModelGallery"))&&!FParse::Param(FCommandLine::Get(),TEXT("ArcadeVfxGallery")))
        {
            const FVector Center=GetOwner()->GetActorLocation();
            const FVector Offset=(E.Start-Center).RotateAngleAxis(E.Age*132.f,FVector::UpVector);Position=Center+Offset;
        }
        const float Extent=ModelPools[E.Kind]->GetStaticMesh()->GetBounds().BoxExtent.GetMax()*2.f;
        const float Fade=FMath::Clamp((E.Lifetime-E.Age)/.025f,0.f,1.f);
        const float Scale=E.Size/FMath::Max(1.f,Extent)*(E.bEvolved?1.12f:1.f)*Fade;
        Transforms[E.Kind].Add(FTransform(FRotator(0,Yaw,0),Position,FVector(Scale)));
    }
    for(int32 Kind=0;Kind<ModelPools.Num();++Kind)
    {
        auto* Pool=ModelPools[Kind].Get();if(!Pool){continue;}
        const auto& Poses=Transforms[Kind];
        if(Pool->GetInstanceCount()!=Poses.Num())
        {Pool->ClearInstances();if(!Poses.IsEmpty()){Pool->AddInstances(Poses,false,true,false);}}
        else if(!Poses.IsEmpty()){Pool->BatchUpdateInstancesTransforms(0,Poses,true,true,true);}
    }
}
