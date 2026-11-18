#include "AnimaPreview.h"
#include "CombatVfx.h"
#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
#include "CombatHUD.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"

#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"

namespace
{
enum ECombatParticle : uint8
{
    Spark, Electric, Air, Ice, Metal, Petal, Burst, LargeBurst
};

constexpr const TCHAR* ParticlePaths[] = {
    TEXT("Sparks/P_Sparks_A.P_Sparks_A"),
    TEXT("Destruction/P_Destruction_Electric.P_Destruction_Electric"),
    TEXT("Hit/P_Default.P_Default"),
    TEXT("Hit/P_Ice.P_Ice"),
    TEXT("Destruction/P_Destruction_Metal.P_Destruction_Metal"),
    TEXT("Hit/P_Vegetation.P_Vegetation"),
    TEXT("Explosion/P_Explosion_Side.P_Explosion_Side"),
    TEXT("Explosion/P_Explosion_Big_A.P_Explosion_Big_A")
};

struct FChampionEffectProfile
{
    ECombatParticle Trail;
    ECombatParticle Impact;
    ECombatParticle Ultimate;
};

ACombatHUD* FindCombatHUD(const AActor* Actor)
{
    if (Actor->GetNetMode() == NM_DedicatedServer) { return nullptr; }
    APlayerController* Controller = UGameplayStatics::GetPlayerController(Actor, 0);
    return Controller ? Cast<ACombatHUD>(Controller->GetHUD()) : nullptr;
}

constexpr FChampionEffectProfile Profiles[] = {
    {Spark, Burst, LargeBurst},
    {Electric, Ice, Electric},
    {Metal, Spark, Burst},
    {Petal, Air, Metal},
    {Spark, Air, Burst},
    {Air, Air, Ice},
    {Petal, Spark, Electric},
    {Ice, Electric, Ice},
    {Petal, Petal, Electric}
};
}

void AAnimaPreviewCharacter::SpawnCombatParticle(uint8 Effect, FVector Location, FRotator Rotation, FVector Scale)
{
    if (GetNetMode() == NM_DedicatedServer || Effect >= UE_ARRAY_COUNT(ParticlePaths)) { return; }
    static TWeakObjectPtr<UParticleSystem> Cache[UE_ARRAY_COUNT(ParticlePaths)];
    UParticleSystem* Template = Cache[Effect].Get();
    if (!Template)
    {
        const FString Path = FString(TEXT("/Game/Realistic_Starter_VFX_Pack_Vol2/Particles/")) + ParticlePaths[Effect];
        Template = LoadObject<UParticleSystem>(nullptr, *Path);
        Cache[Effect] = Template;
    }
    if (Template)
    {
        auto* Component = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Template, Location, Rotation,
            Scale, true, EPSCPoolMethod::None);
        if (!Component) { return; }
        const float Lifetime = Effect == LargeBurst ? 1.35f : Effect == Burst ? .85f : .45f;
        FTimerHandle Cleanup;
        GetWorldTimerManager().SetTimer(Cleanup, FTimerDelegate::CreateWeakLambda(Component, [Component]()
        {
            Component->DestroyComponent();
        }), Lifetime, false);
    }
}


namespace
{
FVector GroundFxPosition(const AActor* Actor, FVector Position)
{
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CombatVfxGround),false,Actor);
    if(Actor->GetWorld()->LineTraceSingleByObjectType(Hit,Position+FVector(0,0,180),Position-FVector(0,0,500),
        FCollisionObjectQueryParams(ECC_WorldStatic),Query) && Hit.ImpactNormal.Z>.5f)
    { Position.Z=Hit.ImpactPoint.Z+8.f; }
    return Position;
}

FLinearColor ChampionColor(int32 Choice)
{
    static const FLinearColor Colors[] = {
        {1.f,.16f,.48f,1.f}, {.18f,.8f,1.f,1.f}, {1.f,.72f,.14f,1.f},
        {1.f,.48f,.10f,1.f}, {1.f,.06f,.27f,1.f}, {.22f,.65f,1.f,1.f},
        {.25f,1.f,.35f,1.f}, {.15f,.9f,1.f,1.f}, {.4f,.35f,1.f,1.f}
    };
    static_assert(UE_ARRAY_COUNT(Colors)==9);
    return Colors[FMath::Clamp(Choice,0,8)];
}
}

void AAnimaPreviewCharacter::PreloadCombatVfxAssets()
{
    if(GetNetMode()==NM_DedicatedServer) { return; }
    for(const TCHAR* Name : {TEXT("M_ArcadeRibbon"),TEXT("M_WindWall"),TEXT("SM_IllaoiTentacle"),TEXT("SM_XayahFeather"),TEXT("SM_RobotTibber"),TEXT("SM_YuumiDrone")})
    {
        const FString Path=FString(TEXT("/Game/TroubleSquad/CombatVisuals/"))+Name+TEXT(".")+Name;
        if(UObject* Asset=LoadObject<UObject>(nullptr,*Path)) { CombatVfxAssets.Add(Asset); }
    }
#if WITH_EDITOR
    if(FParse::Param(FCommandLine::Get(),TEXT("NiagaraVfxReview")))
    {
        for(int32 I=1;I<=15;++I)
        {
            const FString Path=FString::Printf(TEXT("/Game/Basic_VFX/Niagara/NS_Basic_%d.NS_Basic_%d"),I,I);
            if(UObject* Asset=LoadObject<UObject>(nullptr,*Path)) { CombatVfxAssets.Add(Asset); }
        }
    }
#endif
}

ACombatVfx* AAnimaPreviewCharacter::GetCombatVfx()
{
    if (GetNetMode()==NM_DedicatedServer) { return nullptr; }
    if (!IsValid(CombatVfx))
    {
        FActorSpawnParameters P; P.Owner=this;
        P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        CombatVfx=GetWorld()->SpawnActor<ACombatVfx>(FVector::ZeroVector,FRotator::ZeroRotator,P);
    }
    return CombatVfx;
}

void AAnimaPreviewCharacter::SpawnCombatCastEffect(uint8 Motion)
{
    if (Motion==0) { return; }
    if (auto* Vfx=GetCombatVfx())
    {
        const FVector P=GroundFxPosition(this,GetActorLocation());
        Vfx->Emit(Motion==2?ECombatVfxShape::Pulse:ECombatVfxShape::Slash,P,P+GetActorForwardVector(),
            Motion==2?160.f:95.f,ChampionColor(CharacterChoice),Motion==2?.48f:.24f,Motion==2);
    }
}

void AAnimaPreviewCharacter::RenderJinxTracer(FVector_NetQuantize Start,FVector_NetQuantize End,uint8 Variant)
{
    if (auto* Vfx=GetCombatVfx())
    {
        ECombatVfxShape Shape=CharacterChoice==8?ECombatVfxShape::Feather:ECombatVfxShape::Bolt;
        if(CharacterChoice==3) { Shape=ECombatVfxShape::Tentacle; }
        if(CharacterChoice==5) { Shape=ECombatVfxShape::Tornado; }
        if(CharacterChoice==6) { Shape=ECombatVfxShape::Disc; }
        if(Variant==3) { Shape=ECombatVfxShape::Bolt; }
        Vfx->Emit(Shape,Start,End,FMath::Min(FVector::Dist2D(Start,End),380.f),
            ChampionColor(CharacterChoice),Variant==3?.35f:.16f,Variant==1 || IsSignatureWeaponEvolved());
        if(CharacterChoice==1)
        { Vfx->Emit(ECombatVfxShape::Music,End,End,43.f,ChampionColor(1),.2f,IsSignatureWeaponEvolved()); }
        if(CharacterChoice==7)
        { Vfx->Emit(ECombatVfxShape::Pulse,End,End,18.f,FLinearColor(.7f,.2f,1.f),.15f); }
    }
}

void AAnimaPreviewCharacter::RenderJinxExplosion(FVector_NetQuantize Position,float Radius,bool bUltimate)
{
    if(auto* Vfx=GetCombatVfx())
    {
        Vfx->Emit(ECombatVfxShape::Pulse,Position,Position,Radius,
            bUltimate?FLinearColor(1.f,.52f,.08f):ChampionColor(0),bUltimate?.65f:.35f,bUltimate);
    }
    // Keep a small physical impact; the readable radius comes from the authored shock rings.
    SpawnCombatParticle(bUltimate?Burst:Spark,Position,FRotator::ZeroRotator,FVector(bUltimate?.28f:.12f));
}

void AAnimaPreviewCharacter::RenderLeonaShieldStrike(FVector_NetQuantize Center,
    FVector_NetQuantizeNormal Direction,float Radius,bool bEvolved)
{
    const FVector Ground=GroundFxPosition(this,Center);
    if(auto* Vfx=GetCombatVfx())
    {
        Vfx->Emit(bEvolved?ECombatVfxShape::Pulse:ECombatVfxShape::Slash,Ground,Ground+FVector(Direction),
            Radius,ChampionColor(2),.3f,bEvolved);
    }
}

void AAnimaPreviewCharacter::RenderSeraphineEffect(FVector_NetQuantize Center,float Radius,bool bUltimate)
{
    if(auto* Vfx=GetCombatVfx())
    {
        ECombatVfxShape Shape=CharacterChoice==1?ECombatVfxShape::Music:ECombatVfxShape::Pulse;
        if(CharacterChoice==3 || CharacterChoice==4 || CharacterChoice==6) { Shape=bUltimate?ECombatVfxShape::Pulse:ECombatVfxShape::Slash; }
        const FVector P=GroundFxPosition(this,Center);
        Vfx->Emit(Shape,P,P+GetActorForwardVector(),Radius,ChampionColor(CharacterChoice),bUltimate?.52f:.3f,bUltimate);
    }
}

void AAnimaPreviewCharacter::RenderWeaponVfx(uint8 Weapon,FVector_NetQuantize Start,
    FVector_NetQuantize End,float Radius,uint8 Phase,bool bEvolved)
{
    if(Weapon>=GeneralWeaponCount) { return; }
    static const FLinearColor Colors[] = {
        {1,.12f,.8f}, {.1f,.8f,1}, {.5f,1,.35f}, {.45f,1,.08f}, {.15f,.6f,1},
        {1,.24f,.8f}, {.9f,.08f,.7f}, {.6f,.2f,1}, {.8f,.16f,1}, {1,.38f,.08f},
        {1,.4f,.03f}, {.22f,.7f,1}, {.35f,.25f,1}, {.25f,.85f,1}, {1,.85f,.08f},
        {1,.58f,.1f}, {.15f,1,.68f}, {.7f,.22f,1}, {.12f,.72f,1}, {1,.16f,.6f}
    };
    static_assert(UE_ARRAY_COUNT(Colors)==GeneralWeaponCount);
    ECombatVfxShape Shape=Phase==0?ECombatVfxShape::Bolt:ECombatVfxShape::Pulse;
    const auto Type=static_cast<EGeneralWeapon>(Weapon);
    if(Type==EGeneralWeapon::Statikk) { Shape=Phase==0?ECombatVfxShape::Lightning:ECombatVfxShape::Orbital; }
    if(Type==EGeneralWeapon::Boomerang || Type==EGeneralWeapon::Crescent || Type==EGeneralWeapon::Blade) { Shape=Phase==0?ECombatVfxShape::Disc:ECombatVfxShape::Slash; }
    if(Type==EGeneralWeapon::Train) { Shape=ECombatVfxShape::Train; }
    if(Type==EGeneralWeapon::Gatling) { Shape=ECombatVfxShape::Gatling; }
    if(Type==EGeneralWeapon::IceArmor) { Shape=ECombatVfxShape::Shield; }
    if(Type==EGeneralWeapon::Crossbow) { Shape=ECombatVfxShape::Feather; }
    if(Type==EGeneralWeapon::Orbital || Type==EGeneralWeapon::Annihilator) { Shape=ECombatVfxShape::Orbital; }
    if(Type==EGeneralWeapon::Fire || Type==EGeneralWeapon::Trail) { Shape=Phase==0?ECombatVfxShape::Bolt:ECombatVfxShape::Flame; }
    if(Type==EGeneralWeapon::Cyclone) { Shape=ECombatVfxShape::Slash; }
    if(Type==EGeneralWeapon::Tibber || Type==EGeneralWeapon::Yuumi) { Shape=ECombatVfxShape::Drone; }
    if(Phase==2) { Shape=ECombatVfxShape::Pulse; }
    if(auto* Vfx=GetCombatVfx())
    {
        const FVector P=Phase?GroundFxPosition(this,Start):FVector(Start)-FVector(0,0,15.f);
        const FVector Q=Phase?GroundFxPosition(this,End):FVector(End)-FVector(0,0,15.f);
        const float Lifetime=Phase==0?(Type==EGeneralWeapon::Train?.4f:Type==EGeneralWeapon::Boomerang?.35f:.16f)
            : Type==EGeneralWeapon::Trail?.52f:Type==EGeneralWeapon::Fire?.48f
            : Type==EGeneralWeapon::Tibber || Type==EGeneralWeapon::Yuumi?.7f:.32f;
        Vfx->Emit(Shape,P,Q,Radius,Colors[Weapon],FParse::Param(FCommandLine::Get(),TEXT("ArcadeVfxGallery"))?3.f:Lifetime,bEvolved);
        if(bEvolved && Phase && Type!=EGeneralWeapon::Cyclone)
        { Vfx->Emit(ECombatVfxShape::Ring,P,Q,Radius*.9f,Colors[Weapon],.24f,true); }
    }
}

void AAnimaPreviewCharacter::UpdateSkillVfx()
{
    if(GetNetMode()==NM_DedicatedServer) { return; }
    if(CombatHealth<=0.f || bDistrictFinished)
    { if(IsValid(CombatVfx)) { CombatVfx->Clear(); } return; }
    const float Now=GetCombatTime();
    const FVector Ground=GetActorLocation()-FVector::UpVector*(GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-8.f);
    auto State=[&](int32 Key,FVector P,float Radius,FLinearColor Color)
    {
        if(auto* Vfx=GetCombatVfx()) { Vfx->Emit(ECombatVfxShape::Ring,P,P,Radius,Color,.16f,false,Key); }
    };
    if(HasSeraphineShield(Now)) { State(1,Ground,75.f,ChampionColor(CharacterChoice)); }
    FVector P; float Radius;
    if(GetLeonaEclipseZone(Now,P,Radius)) { State(2,P+FVector(0,0,8),Radius,ChampionColor(2)); }
    if(GetLeonaSolarZone(Now,P,Radius)) { State(3,P+FVector(0,0,8),Radius,FLinearColor(1,.52f,.12f)); }
    if(CharacterChoice==4 && Now<BriarFrenzyEnd) { State(4,Ground,90.f,ChampionColor(4)); }
    if(CharacterChoice==6 && Now<RivenUltimateEnd) { State(6,Ground,110.f,ChampionColor(6)); }
    if(CharacterChoice==7 && Now<AuroraVeilEnd) { State(7,Ground,85.f,ChampionColor(7)); }
    if(GetAuroraRealmBoundary(Now,P)) { State(8,P+FVector(0,0,10),AuroraRealmRadius,ChampionColor(7)); }
    if(CharacterChoice==8 && Now<XayahBuffEnd) { State(9,Ground,80.f,ChampionColor(8)); }
    FVector Normal;
    if(GetWindWallBoundary(Now,P,Normal))
    {
        const FVector Side=FVector::CrossProduct(FVector::UpVector,Normal)*WindWallHalfWidth;
        if(auto* Vfx=GetCombatVfx())
        {
            Vfx->Emit(ECombatVfxShape::WindCurtain,P-Side-FVector::UpVector*WindWallHalfHeight,
                P+Side-FVector::UpVector*WindWallHalfHeight,WindWallHalfHeight*2.f,ChampionColor(5),.16f,false,20);
        }
    }
}

void AAnimaPreviewCharacter::UpdateGeneralSummonMeshes()
{
    if(GetNetMode()==NM_DedicatedServer) { return; }
    const float Now=GetCombatTime();
    TArray<const FGeneralSummonVisual*> Visible;
    if(CombatHealth>0.f && !bDistrictFinished)
    { for(const auto& V:GeneralSummonVisuals) { if(Now<V.EndAt) { Visible.Add(&V); } } }
    while(GeneralSummonMeshes.Num()>Visible.Num())
    { GeneralSummonMeshes.Last()->DestroyComponent(); GeneralSummonMeshes.Pop(); }
    if(Visible.IsEmpty()) { return; }
    if(!TibberSummonAsset) { TibberSummonAsset=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/TroubleSquad/CombatVisuals/SM_RobotTibber.SM_RobotTibber")); }
    if(!YuumiSummonAsset) { YuumiSummonAsset=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/TroubleSquad/CombatVisuals/SM_YuumiDrone.SM_YuumiDrone")); }
    while(GeneralSummonMeshes.Num()<Visible.Num())
    {
        auto* C=NewObject<UStaticMeshComponent>(this);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(false);
        C->RegisterComponent(); GeneralSummonMeshes.Add(C);
    }
    for(int32 I=0;I<Visible.Num();++I)
    {
        const FGeneralSummonVisual& V=*Visible[I];
        const bool bCat=V.Weapon==static_cast<uint8>(EGeneralWeapon::Yuumi);
        auto* C=GeneralSummonMeshes[I].Get(); UStaticMesh* Asset=bCat?YuumiSummonAsset.Get():TibberSummonAsset.Get();
        const bool bNew=C->GetStaticMesh()!=Asset;
        if(bNew) { C->SetStaticMesh(Asset); }
        FVector Position=GroundFxPosition(this,V.Position);
        Position.Z+=(bCat?14.f:2.f)+FMath::Sin(Now*(bCat?3.4f:8.f)+I)*(bCat?5.f:2.f);
        C->SetWorldLocation(bNew||FVector::DistSquared(C->GetComponentLocation(),Position)>250000.f?Position
            :FMath::VInterpTo(C->GetComponentLocation(),Position,GetWorld()->GetDeltaSeconds(),12.f));
        FRotator Rotation=FVector(V.Facing).Rotation();
        Rotation.Pitch=FMath::Sin(Now*4.f+I)*(bCat?4.f:1.5f);
        C->SetWorldRotation(Rotation);
        const float Dissolve=FMath::Clamp((V.EndAt-Now)/.25f,0.f,1.f);
        C->SetWorldScale3D(FVector(V.Size*(bCat?.7f:1.f)*Dissolve));
    }
}

void AAnimaPreviewCharacter::UpdateTentacleMeshes()
{
    if (GetNetMode() == NM_DedicatedServer) { return; }
    const int32 Count = CharacterChoice == 3 && !bDistrictFinished ? TentacleVisuals.Num() : 0;
    while (TentacleMeshes.Num() > Count)
    {
        TentacleMeshes.Last()->DestroyComponent();
        TentacleMeshes.Pop();
    }
    if (Count == 0) { return; }
    if (!TentacleMeshAsset)
    {
        TentacleMeshAsset = LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/TroubleSquad/CombatVisuals/SM_IllaoiTentacle.SM_IllaoiTentacle"));
    }
    if (!TentacleMeshAsset) { return; }
    while (TentacleMeshes.Num() < Count)
    {
        UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
        Component->SetStaticMesh(TentacleMeshAsset);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetCastShadow(true);
        Component->RegisterComponent();
        TentacleMeshes.Add(Component);
    }

    const float Now = GetCombatTime();
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FIllaoTentacleVisual& Visual = TentacleVisuals[Index];
        UStaticMeshComponent* Component = TentacleMeshes[Index];
        const FVector Ground = Visual.Position - FVector::UpVector * 55.f;
        const float Size = Visual.bGiant ? 1.65f : 1.f;
        const float UntilAttack = Visual.NextAttackAt - Now;
        const float Windup = FMath::Clamp(1.f - UntilAttack / .55f, 0.f, 1.f);
        const float Recovery = FMath::Clamp(1.f - (3.f - UntilAttack) / .28f, 0.f, 1.f);
        const float Sway = FMath::Sin(Now * 2.8f + Index * 1.7f) * 4.f;
        Component->SetWorldLocation(Ground);
        Component->SetWorldRotation(FRotator(-18.f * Windup + 46.f * Recovery + Sway, Index * 137.5f, 0.f));
        Component->SetWorldScale3D(FVector(Size));
        Component->SetVisibility(!bDistrictFinished);
    }
}
void AAnimaPreviewCharacter::UpdateWindWallVisual()
{
    if (GetNetMode() == NM_DedicatedServer) { return; }
    FVector Center, Normal;
    const bool bActive = GetWindWallBoundary(GetCombatTime(), Center, Normal);
    if (!bActive)
    {
        if (WindWallVisual)
        {
            WindWallVisual->DestroyComponent();
            WindWallVisual = nullptr;
        }
        return;
    }
    if (!WindWallVisual)
    {
        UStaticMesh* WallMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        UMaterialInterface* WallMaterial = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/TroubleSquad/CombatVisuals/M_WindWall.M_WindWall"));
        if (!WallMesh || !WallMaterial) { return; }
        WindWallVisual = NewObject<UStaticMeshComponent>(this);
        WindWallVisual->SetStaticMesh(WallMesh);
        WindWallVisual->SetMaterial(0, WallMaterial);
        WindWallVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        WindWallVisual->SetCastShadow(false);
        WindWallVisual->RegisterComponent();
    }
    WindWallVisual->SetWorldLocation(Center);
    WindWallVisual->SetWorldRotation(Normal.Rotation());
    WindWallVisual->SetWorldScale3D(FVector(.08f, WindWallHalfWidth / 50.f, WindWallHalfHeight / 50.f));
}
void AAnimaPreviewCharacter::UpdateXayahFeatherMeshes()
{
    if (GetNetMode() == NM_DedicatedServer) { return; }
    const int32 Count = CharacterChoice == 8 && !bDistrictFinished ? LandedXayahFeathers.Num() : 0;
    while (XayahFeatherMeshes.Num() > Count)
    {
        XayahFeatherMeshes.Last()->DestroyComponent();
        XayahFeatherMeshes.Pop();
    }
    if (Count == 0) { return; }
    if (!XayahFeatherMeshAsset)
    {
        XayahFeatherMeshAsset = LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/TroubleSquad/CombatVisuals/SM_XayahFeather.SM_XayahFeather"));
    }
    if (!XayahFeatherMeshAsset) { return; }
    while (XayahFeatherMeshes.Num() < Count)
    {
        UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
        Component->SetStaticMesh(XayahFeatherMeshAsset);
        Component->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/TroubleSquad/CombatVisuals/M_FeatherVane.M_FeatherVane")));
        Component->SetMaterial(1, LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/TroubleSquad/CombatVisuals/M_FeatherQuill.M_FeatherQuill")));
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetCastShadow(false);
        Component->RegisterComponent();
        XayahFeatherMeshes.Add(Component);
    }
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FLandedXayahFeather& Feather = LandedXayahFeathers[Index];
        FRotator Rotation = FVector(Feather.Direction).Rotation();
        Rotation.Pitch = -12.f;
        XayahFeatherMeshes[Index]->SetWorldLocation(Feather.Position - FVector::UpVector * 48.f);
        XayahFeatherMeshes[Index]->SetWorldRotation(Rotation);
        XayahFeatherMeshes[Index]->SetWorldScale3D(FVector(.9f));
    }
}
