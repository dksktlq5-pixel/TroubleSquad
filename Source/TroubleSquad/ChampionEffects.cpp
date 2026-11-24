#include "AnimaPreview.h"
#include "CombatVfx.h"
#include "CombatAudio.h"
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
        {.25f,1.f,.35f,1.f}, {.28f,.06f,.9f,1.f}, {.4f,.35f,1.f,1.f}
    };
    static_assert(UE_ARRAY_COUNT(Colors)==9);
    return Colors[FMath::Clamp(Choice,0,8)];
}
}

void AAnimaPreviewCharacter::PreloadCombatVfxAssets()
{
    if(GetNetMode()==NM_DedicatedServer) { return; }
    for(const TCHAR* Name : {TEXT("M_ArcadeRibbon"),TEXT("M_WindWall"),TEXT("SM_IllaoiTentacle"),TEXT("SM_XayahFeather"),TEXT("SM_RobotTibber"),TEXT("SM_YuumiDrone"),TEXT("SM_Supercar")})
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
    if(Motion==0 || Motion>2) { return; }
    auto* Vfx=GetCombatVfx(); if(!Vfx) { return; }
    const FVector P=GroundFxPosition(this,GetActorLocation());
    const FVector Forward=GetActorForwardVector(),Side=GetActorRightVector();
    const FLinearColor Color=ChampionColor(CharacterChoice);
    const bool Ultimate=Motion==2;
    // Brief cast accents identify the gesture. Actual skill range is drawn by the skill itself.
    auto Accent=[&](ECombatVfxShape Shape,FVector A,FVector B,float Size,float Life)
    { Vfx->Emit(Shape,A,B,Size,Color,Life,Ultimate,INDEX_NONE,2); };
    switch(CharacterChoice)
    {
    case 0: // Muzzle exhaust rather than a radial spell circle.
        Accent(ECombatVfxShape::Flame,P+Forward*65.f,P+Forward*140.f,Ultimate?70.f:40.f,.20f);
        Accent(ECombatVfxShape::Bolt,P+Forward*70.f,P+Forward*165.f,16.f,.14f); break;
    case 1:
        Accent(ECombatVfxShape::Music,P,P,Ultimate?125.f:80.f,Ultimate?.55f:.35f); break;
    case 2:
        Accent(ECombatVfxShape::Shield,P+Forward*35.f,P+Forward*80.f,Ultimate?115.f:85.f,.38f); break;
    case 3:
        Accent(ECombatVfxShape::TentacleSlam,P-Forward*30.f,P+Forward*(Ultimate?190.f:135.f),Ultimate?62.f:38.f,.36f); break;
    case 4:
        for(float Sign:{-1.f,1.f})
        { Accent(ECombatVfxShape::Slash,P+Side*Sign*45.f,P+Forward*90.f-Side*Sign*55.f,Ultimate?125.f:85.f,.26f); } break;
    case 5:
        Accent(Ultimate?ECombatVfxShape::WindCurtain:ECombatVfxShape::Slash,
            Ultimate?P-Side*75.f:P,Ultimate?P+Side*75.f:P+Forward*160.f,Ultimate?90.f:95.f,.30f); break;
    case 6:
        Accent(ECombatVfxShape::Slash,P-Forward*30.f,P+Forward*125.f,Ultimate?145.f:90.f,.30f);
        if(Ultimate) { Accent(ECombatVfxShape::Shield,P,P,80.f,.45f); } break;
    case 7:
        Accent(ECombatVfxShape::Orbital,P,P,Ultimate?120.f:70.f,.45f); break;
    case 8:
        for(int32 I=0;I<(Ultimate?5:3);++I)
        {
            const FVector D=Forward.RotateAngleAxis((I-(Ultimate?2.f:1.f))*25.f,FVector::UpVector);
            Accent(ECombatVfxShape::Feather,Ultimate?P:P+D*130.f,Ultimate?P+D*130.f:P,32.f,.28f);
        } break;
    default:break;
    }
}

void AAnimaPreviewCharacter::RenderJinxTracer(FVector_NetQuantize Start,FVector_NetQuantize End,uint8 Variant)
{
    if (auto* Vfx=GetCombatVfx())
    {
        ECombatVfxShape Shape=CharacterChoice==8?ECombatVfxShape::Feather:ECombatVfxShape::Bolt;
        if(CharacterChoice==1) { Shape=ECombatVfxShape::SoundWave; }
        if(CharacterChoice==7) { Shape=ECombatVfxShape::SpiritBolt; }
        if(CharacterChoice==3) { Shape=ECombatVfxShape::Tentacle; }
        if(CharacterChoice==5) { Shape=ECombatVfxShape::Tornado; }
        if(CharacterChoice==6) { Shape=ECombatVfxShape::Disc; }
        if(Variant==3) { Shape=ECombatVfxShape::Bolt; }
        Vfx->Emit(Shape,Start,End,FMath::Min(FVector::Dist2D(Start,End),380.f),
            ChampionColor(CharacterChoice),Variant==3?.45f:.24f,Variant==1 || IsSignatureWeaponEvolved(),INDEX_NONE,1);
        if(CharacterChoice==1)
        { Vfx->Emit(ECombatVfxShape::Music,End,End,43.f,ChampionColor(1),.2f,IsSignatureWeaponEvolved()); }
        if(CharacterChoice==7)
        { Vfx->Emit(ECombatVfxShape::Pulse,End,End,18.f,FLinearColor(.7f,.2f,1.f),.15f); }
    }
}

void AAnimaPreviewCharacter::RenderJinxExplosion(FVector_NetQuantize Position,float Radius,bool bUltimate)
{
    CombatAudio::PlayWorld(GetWorld(),bUltimate?TEXT("Champions/Jinx_RImpact"):TEXT("Champions/Jinx_WeaponImpact"),Position,bUltimate?.24f:.12f);
    if(auto* Vfx=GetCombatVfx())
    {
        Vfx->Emit(ECombatVfxShape::Pulse,Position,Position,Radius,
            bUltimate?FLinearColor(1.f,.52f,.08f):ChampionColor(0),bUltimate?.8f:.5f,bUltimate,INDEX_NONE,2);
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
            Radius,ChampionColor(2),.5f,bEvolved,INDEX_NONE,1);
    }
}

void AAnimaPreviewCharacter::RenderSeraphineEffect(FVector_NetQuantize Center,float Radius,bool bUltimate)
{
    if(auto* Vfx=GetCombatVfx())
    {
        ECombatVfxShape Shape=CharacterChoice==1?ECombatVfxShape::Music:ECombatVfxShape::Pulse;
        if(CharacterChoice==3 || CharacterChoice==6) { Shape=bUltimate?ECombatVfxShape::Pulse:ECombatVfxShape::Slash; }
        if(CharacterChoice==4) { Shape=ECombatVfxShape::Slash; }
        const FVector P=GroundFxPosition(this,Center);
        Vfx->Emit(Shape,P,P+GetActorForwardVector(),Radius,ChampionColor(CharacterChoice),bUltimate?.75f:.5f,bUltimate,INDEX_NONE,2);
        if(CharacterChoice==4)
        {
            const FVector Side=GetActorRightVector()*22.f;
            for(int32 I : {-1,1})
            { const FVector Q=P+Side*I;Vfx->Emit(ECombatVfxShape::Slash,Q,Q+GetActorForwardVector(),Radius*.92f,ChampionColor(4),.28f,false,INDEX_NONE,1); }
        }
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
    if(Type==EGeneralWeapon::Mine && Phase==0) { Shape=ECombatVfxShape::SpikedMine; }
    if(Type==EGeneralWeapon::Train) { Shape=Phase==0?ECombatVfxShape::Train:ECombatVfxShape::Pulse; }
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
        const bool Gallery=FParse::Param(FCommandLine::Get(),TEXT("ArcadeVfxGallery"))||FParse::Param(FCommandLine::Get(),TEXT("WeaponModelGallery"));
        const int32 ModelKind=Type==EGeneralWeapon::Train?0:Type==EGeneralWeapon::Boomerang?1:Type==EGeneralWeapon::Crescent?2:Type==EGeneralWeapon::Blade?3:Type==EGeneralWeapon::Tibber?4:Type==EGeneralWeapon::Crossbow?5:Type==EGeneralWeapon::Cyclone?6:INDEX_NONE;
        if(ModelKind!=INDEX_NONE&&Phase!=2&&(Gallery||Phase==0||Type==EGeneralWeapon::Cyclone))
        {
            const float Size=Gallery?(ModelKind==0?220.f:ModelKind==4?180.f:130.f)
                :ModelKind==0?FMath::Clamp(Radius*2.2f,180.f,380.f):FMath::Clamp(Radius*1.8f,70.f,170.f);
            const float ModelLife=Gallery?3.f:ModelKind==0?.48f:ModelKind==6?.14f:.08f;
            const FVector ModelStart=Gallery?(P+Q)*.5f:P;
            Vfx->EmitModel(ModelKind,ModelStart,Gallery?ModelStart:Q,Size,ModelLife,!Gallery&&Phase==0,bEvolved);
            if(!Gallery&&Phase==0){Vfx->Emit(ECombatVfxShape::ModelTrail,P,Q,Radius,Colors[Weapon],.12f,bEvolved);}
            return;
        }
        if(Type==EGeneralWeapon::Tibber&&Phase==1){Shape=ECombatVfxShape::Slash;}
        const float Lifetime=Phase==0?(Type==EGeneralWeapon::Train?.4f:Type==EGeneralWeapon::Boomerang?.35f:.24f)
            : Type==EGeneralWeapon::Trail?.52f:Type==EGeneralWeapon::Fire?.48f
            : Type==EGeneralWeapon::Tibber || Type==EGeneralWeapon::Yuumi?.7f:.32f;
        Vfx->Emit(Shape,P,Q,Radius,Colors[Weapon],FParse::Param(FCommandLine::Get(),TEXT("ArcadeVfxGallery"))?3.f:Lifetime,bEvolved);
        if(bEvolved && Phase && Type!=EGeneralWeapon::Cyclone)
        { Vfx->Emit(ECombatVfxShape::Ring,P,Q,Radius*.9f,Colors[Weapon],.24f,true); }
    }
}

void AAnimaPreviewCharacter::RenderTentacleSlam(FVector Start,FVector End,float HalfWidth,bool bGiant)
{
    if(auto* Vfx=GetCombatVfx())
    { Vfx->Emit(ECombatVfxShape::TentacleSlam,GroundFxPosition(this,Start),GroundFxPosition(this,End),HalfWidth,ChampionColor(3),.3f,bGiant); }
    if(GetCombatTime()>=NextTentacleSoundAt)
    { NextTentacleSoundAt=GetCombatTime()+.12f; CombatAudio::PlayWorld(GetWorld(),TEXT("Champions/Illaoi_Weapon"),Start,.16f); }
}

void AAnimaPreviewCharacter::RenderEnemyBurst(FVector Position,float Radius)
{
    if(auto* Vfx=GetCombatVfx()) { Vfx->Emit(ECombatVfxShape::Pulse,GroundFxPosition(this,Position),Position,Radius,FLinearColor(1.f,.12f,.48f),.4f,true); }
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
        if(auto* Vfx=GetCombatVfx()) { Vfx->Emit(Key==1?ECombatVfxShape::Shield:ECombatVfxShape::Ring,P,P,Radius,Color,.16f,false,Key); }
    };
    if(IsLocallyControlled() && YuumiQuestKills>=0 && YuumiQuestRemaining>0.f) { State(250,GroundFxPosition(this,YuumiQuestCenter),YuumiQuestType==2?100.f:YuumiQuestType==1?140.f:650.f,FLinearColor(.12f,.8f,1.f)); }
    if(IsLocallyControlled() && BoonSpireKills>=0 && BoonSpireRemaining>0.f) { State(251,GroundFxPosition(this,BoonSpireCenter),650.f,FLinearColor(1,.68f,.1f)); }
    if(bYuumiCarryingToken) { State(254,Ground,45.f,FLinearColor(.1f,.75f,1.f)); }
    if(GetWorldBoonRemaining()>0.f)
    {
        State(252,Ground,WorldBoonType==3?150.f:100.f,WorldBoonType==3?FLinearColor(.05f,.8f,1.f):FLinearColor(1,.65f,.12f));
        if(WorldBoonType==3 && GetVelocity().SizeSquared2D()>100.f)
        { if(auto* Vfx=GetCombatVfx()) { const FVector Tail=Ground-GetActorForwardVector()*90.f; Vfx->Emit(ECombatVfxShape::Bolt,Tail,Tail-GetActorForwardVector()*120.f,25.f,FLinearColor(.12f,.65f,1.f),.16f,false,253); } }
    }
    if(HasSeraphineShield(Now)) { State(1,Ground,75.f,ChampionColor(1)); }
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
    if(!TibberSummonAsset) { TibberSummonAsset=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/TroubleSquad/CombatVisuals/GeneralWeapons/Models/SM_GW_Tibber.SM_GW_Tibber")); }
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
        C->SetWorldScale3D(FVector(V.Size*(bCat?.7f:.42f)*Dissolve));
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
        Component->SetWorldRotation(FRotator(-18.f * Windup + 46.f * Recovery + Sway, FVector(Visual.Direction).Rotation().Yaw, 0.f));
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

FString AAnimaPreviewCharacter::GetSignatureWeaponName(bool bEvolved) const
{
    static const TCHAR* Base[] = {TEXT("야옹 야옹"), TEXT("음향의 물결"), TEXT("방패 타격"), TEXT("촉수 후려치기"), TEXT("족쇄 할퀴기"), TEXT("강철 폭풍"), TEXT("토끼뜀"), TEXT("인도의 저주"), TEXT("날개 달린 단검")};
    static const TCHAR* Evolved[] = {TEXT("전투 고양이 총알 세례"), TEXT("동물 메아리"), TEXT("사자의 광명"), TEXT("곰의 강타"), TEXT("포악한 베기"), TEXT("떠도는 폭풍"), TEXT("당근 격돌"), TEXT("고양된 저주"), TEXT("연인의 도탄")};
    const int32 Index=FMath::Clamp(int32(CharacterChoice),0,8);
    return bEvolved?Evolved[Index]:Base[Index];
}
FString AAnimaPreviewCharacter::GetSignatureWeaponTooltip() const
{
    static const TCHAR* Requirements[] = {TEXT("스킬 가속"), TEXT("투사체 개수"), TEXT("방어력"), TEXT("지속시간"), TEXT("최대 체력"), TEXT("치명타"), TEXT("이동 속도"), TEXT("경험치"), TEXT("획득 반경")};
    static const TCHAR* Effects[] = {
        TEXT("전방으로 탄환을 연사합니다."),TEXT("적을 관통하는 음파를 발사합니다."),
        TEXT("주변 적을 방패로 타격합니다. 방어력에 비례해 피해가 증가합니다."),
        TEXT("전방을 공격하고 촉수를 소환합니다."),TEXT("전방을 베어 공격합니다. 최대 체력에 비례해 피해가 증가합니다."),
        TEXT("기류가 충전되면 회오리를 발사합니다."),TEXT("이동으로 충전하여 주변에 피해를 줍니다."),
        TEXT("적에게 주술을 발사하고 저주를 폭발시킵니다."),TEXT("깃털 단검을 발사합니다.")};
    const int32 Index=FMath::Clamp(int32(CharacterChoice),0,8);
    const FString Timing=Index==5?FString::Printf(TEXT("기류 충전: %.0f / 100"),GetYasuoFlow()):Index==6?FString::Printf(TEXT("이동 충전: %.0f / 100"),GetRivenCharge()):FString::Printf(TEXT("다음 공격까지: %.1f초 (스킬 가속 적용)"),GetJinxWeaponRemaining());
    return FString::Printf(TEXT("%s | 레벨 %d\n%s\n%s\n진화 조건: 무기 5레벨 + %s\n진화 무기: %s"),*GetSignatureWeaponName(IsSignatureWeaponEvolved()),GetSignatureWeaponLevel(),Effects[Index],*Timing,Requirements[Index],*GetSignatureWeaponName(true));
}

void AAnimaPreviewCharacter::EmitGeneralWeaponAudio(uint8 Weapon,FVector Position,bool bImpact)
{
    if(!HasAuthority()||Weapon>=GeneralWeaponCount){return;}
    const bool Evolved=IsGeneralWeaponEvolved(Weapon);
    const FString Cue=FString::Printf(TEXT("General/%s_%s%s"),GetGeneralWeaponInfo(Weapon).AssetSuffix,bImpact?TEXT("Impact"):TEXT("Weapon"),Evolved?TEXT("Evolved"):TEXT(""));
    if(CombatAudio::IsIntentionallySilent(*Cue)){return;}
    const int32 Slot=Weapon*2+(bImpact?1:0);const float Now=GetCombatTime();
    if(Now<NextGeneralAudioAt[Slot]){return;}
    NextGeneralAudioAt[Slot]=Now+(bImpact?.08f:.045f);
#if WITH_EDITOR
    if(bImpact){++AudioTimingImpacts;}else{++AudioTimingLaunches;}
#endif
    PlayGeneralWeaponAudio(Weapon,Position,bImpact,Evolved);
}
void AAnimaPreviewCharacter::PlayGeneralWeaponAudio_Implementation(uint8 Weapon,FVector_NetQuantize Position,bool bImpact,bool bEvolved)
{
    if(Weapon>=GeneralWeaponCount){return;}
    const FString Cue=FString::Printf(TEXT("General/%s_%s%s"),GetGeneralWeaponInfo(Weapon).AssetSuffix,bImpact?TEXT("Impact"):TEXT("Weapon"),bEvolved?TEXT("Evolved"):TEXT(""));
#if WITH_EDITOR
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmAudioEventNetwork")))
    {UE_LOG(LogTemp,Display,TEXT("General audio network: remote=%d impact=%d evolved=%d cue=%s"),!HasAuthority(),bImpact,bEvolved,*Cue);}
#endif
    CombatAudio::PlayWorld(GetWorld(),*Cue,Position,bImpact?.09f:.16f);
}
