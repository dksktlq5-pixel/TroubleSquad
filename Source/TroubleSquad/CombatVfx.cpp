#include "CombatVfx.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"

ACombatVfx::ACombatVfx()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = false;
    Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BatchedSkillRibbons"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCastShadow(false);
    Mesh->SetReceivesDecals(false);
    Mesh->bUseAsyncCooking = true;
}

void ACombatVfx::Emit(ECombatVfxShape Shape, FVector Start, FVector End, float Radius,
    FLinearColor Color, float Lifetime, bool bEvolved, int32 StateKey)
{
    if (GetNetMode() == NM_DedicatedServer || Start.ContainsNaN() || End.ContainsNaN()
        || !FMath::IsFinite(Radius) || !FMath::IsFinite(Lifetime)) { return; }
    if (StateKey != INDEX_NONE)
    {
        for (FEffect& E : Effects)
        {
            if (E.Key != StateKey) { continue; }
            E.Start = Start; E.End = End; E.Radius = Radius; E.Color = Color;
            E.Lifetime = E.Age + .16f;
            return;
        }
    }
    // Sustained states get priority over optional hit sparks in dense late-game combat.
    if (Effects.Num() >= 96)
    {
        const int32 Oldest = Effects.IndexOfByPredicate([](const FEffect& E) { return E.Key == INDEX_NONE; });
        if (Oldest == INDEX_NONE) { return; }
        Effects.RemoveAt(Oldest, 1, EAllowShrinking::No);
    }
    Effects.Add({Shape, Start, End, Color, FMath::Max(Radius, 2.f), 0.f,
        FMath::Clamp(Lifetime, .06f, 12.f), bEvolved, StateKey});
}

void ACombatVfx::Clear()
{
    Effects.Reset();
    if (LastVertexCount) { Mesh->ClearAllMeshSections(); LastVertexCount=0; }
}

void ACombatVfx::Sprite(FVector Center, float Radius, FLinearColor Color)
{
    const int32 N=Vertices.Num();
    Vertices.Append({Center+FVector(-Radius,-Radius,0),Center+FVector(-Radius,Radius,0),
        Center+FVector(Radius,Radius,0),Center+FVector(Radius,-Radius,0)});
    Triangles.Append({N,N+1,N+2,N,N+2,N+3});
    // UV.x above two selects a soft radial particle in the shared material.
    UVs.Append({FVector2D(2,0),FVector2D(2,1),FVector2D(3,1),FVector2D(3,0)});
    Colors.Append({Color,Color,Color,Color});
}

void ACombatVfx::Ribbon(FVector A, FVector B, float Width, FLinearColor Color)
{
    if ((B - A).IsNearlyZero()) { return; }
    FVector Side = FVector::CrossProduct((B - A).GetSafeNormal(), FVector::UpVector).GetSafeNormal();
    if (Side.IsNearlyZero()) { Side = FVector::RightVector; }
    Side *= Width * .5f;
    const int32 N = Vertices.Num();
    Vertices.Append({A-Side, A+Side, B+Side, B-Side});
    Triangles.Append({N,N+1,N+2,N,N+2,N+3});
    UVs.Append({FVector2D(0,0),FVector2D(0,1),FVector2D(1,1),FVector2D(1,0)});
    Colors.Append({Color,Color,Color,Color});
}

void ACombatVfx::Arc(FVector Center, float Radius, float Width, float StartAngle, float Sweep, FLinearColor Color, float Height)
{
    const int32 Count = FMath::Clamp(FMath::CeilToInt(FMath::Abs(Sweep) / 8.f), 4, 48);
    for (int32 I = 0; I < Count; ++I)
    {
        const float A = FMath::DegreesToRadians(StartAngle + Sweep * I / Count);
        const float B = FMath::DegreesToRadians(StartAngle + Sweep * (I+1) / Count);
        Ribbon(Center + FVector(FMath::Cos(A)*Radius,FMath::Sin(A)*Radius,Height),
            Center + FVector(FMath::Cos(B)*Radius,FMath::Sin(B)*Radius,Height), Width, Color);
    }
}

void ACombatVfx::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!IsValid(GetOwner())) { Destroy(); return; }
    if (!Mesh->GetMaterial(0))
    {
        Mesh->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/TroubleSquad/CombatVisuals/M_ArcadeRibbon.M_ArcadeRibbon")));
    }
    Vertices.Reset(); Triangles.Reset(); UVs.Reset(); Colors.Reset();
    for (int32 I=Effects.Num()-1; I>=0; --I)
    {
        FEffect& E = Effects[I]; E.Age += DeltaSeconds;
        if (E.Age >= E.Lifetime) { Effects.RemoveAtSwap(I,1,EAllowShrinking::No); continue; }
        const float T = FMath::Clamp(E.Age/E.Lifetime,0.f,1.f);
        const bool State = E.Key != INDEX_NONE;
        const float Fade = State ? FMath::Clamp((E.Lifetime-E.Age)/.10f,0.f,1.f) : FMath::Min(E.Age/.025f,1.f)*FMath::Square(1.f-T);
        FLinearColor C=E.Color; C.A*=Fade;
        const FVector Direction=(E.End-E.Start).GetSafeNormal2D();
        const float Yaw=Direction.IsNearlyZero()?0.f:Direction.Rotation().Yaw;
        const float R=E.Radius;
        const float Spin=E.Age*150.f;
        if(E.Shape!=ECombatVfxShape::Bolt && E.Shape!=ECombatVfxShape::Lightning && E.Shape!=ECombatVfxShape::Feather && E.Shape!=ECombatVfxShape::WindCurtain)
        {
            FLinearColor Haze=C; Haze.A*=State?.06f:.16f;
            const float HazeRadius=E.Shape==ECombatVfxShape::Disc?FMath::Min(R,65.f):E.Shape==ECombatVfxShape::Tentacle?45.f:R;
            Sprite(E.Start+FVector(0,0,3),HazeRadius*(.4f+.6f*T),Haze);
        }
        switch(E.Shape)
        {
        case ECombatVfxShape::WindCurtain:
        {
            const FVector Normal=FVector::CrossProduct(Direction,FVector::UpVector);
            for(int32 J=0;J<20;++J)
            {
                const float U=J/20.f, V=(J+1)/20.f;
                const FVector A=FMath::Lerp(E.Start,E.End,U)+Normal*(FMath::Sin(U*12.f+E.Age*6.f)*12.f);
                const FVector B=FMath::Lerp(E.Start,E.End,V)+Normal*(FMath::Sin(V*12.f+E.Age*6.f)*12.f);
                FLinearColor Veil=C; Veil.A*=.11f;
                const int32 N=Vertices.Num();
                Vertices.Append({A,B,B+FVector(0,0,R),A+FVector(0,0,R)});
                Triangles.Append({N,N+1,N+2,N,N+2,N+3});
                UVs.Append({FVector2D(0,0),FVector2D(1,0),FVector2D(1,1),FVector2D(0,1)});
                Colors.Append({Veil,Veil,Veil,Veil});
                for(int32 Row=1;Row<5;++Row)
                {
                    const float Z=R*(Row/5.f)+FMath::Sin(U*10.f+E.Age*8.f+Row)*14.f;
                    const float ZB=R*(Row/5.f)+FMath::Sin(V*10.f+E.Age*8.f+Row)*14.f;
                    FLinearColor Wisp=C; Wisp.A*=.35f;
                    Ribbon(A+FVector(0,0,Z),B+FVector(0,0,ZB),4.f,Wisp);
                }
            }
            break;
        }
        case ECombatVfxShape::Tornado:
            for(int32 J=0;J<5;++J)
            { Arc(E.End,18.f+J*10.f,5.f,Spin*4.f+J*65.f,250.f,C,J*18.f); }
            Ribbon(E.Start,E.End,7.f,C);
            break;
        case ECombatVfxShape::Train:
        {
            const FVector Side=FVector::CrossProduct(Direction,FVector::UpVector);
            Ribbon(E.Start+Side*R*.65f,E.End+Side*R*.65f,4.f,C);
            Ribbon(E.Start-Side*R*.65f,E.End-Side*R*.65f,4.f,C);
            const FVector P=FMath::Lerp(E.Start,E.End,T);
            for(int32 Car=0;Car<3;++Car)
            {
                const FVector Back=P-Direction*(Car*95.f);
                Ribbon(Back-Direction*80.f,Back,R*.9f,C*.65f);
                Ribbon(Back-Direction*72.f+Side*R*.22f,Back-Direction*12.f+Side*R*.22f,8.f,C);
            }
            break;
        }
        case ECombatVfxShape::Disc:
        {
            const FVector P=FMath::Lerp(E.Start,E.End,T);
            const float DiscRadius=FMath::Clamp(R,20.f,65.f);
            Arc(P,DiscRadius,7.f,Spin*4.f,260.f,C,10.f);
            Arc(P,DiscRadius*.55f,4.f,-Spin*5.f,260.f,C,16.f);
            Ribbon(FMath::Lerp(E.Start,E.End,FMath::Max(0.f,T-.3f)),P,5.f,C);
            break;
        }
        case ECombatVfxShape::Music:
            Arc(E.Start,R*(.25f+.75f*T),7.f,Spin,340.f,C,8.f);
            for(int32 J=0;J<8;++J)
            {
                const float A=J*PI/4.f;
                const FVector P=E.Start+FVector(FMath::Cos(A),FMath::Sin(A),0)*R*(.3f+.6f*T);
                Ribbon(P,P+FVector(0,0,25),5.f,C);
                Ribbon(P+FVector(-7,-4,0),P+FVector(7,4,0),10.f,C);
            }
            break;
        case ECombatVfxShape::Tentacle:
        {
            const FVector Side=FVector::CrossProduct(Direction,FVector::UpVector);
            FVector Last=E.Start;
            for(int32 J=1;J<=16;++J)
            {
                const FVector Next=FMath::Lerp(E.Start,E.End,J/16.f)+Side*FMath::Sin(J*.4f)*18.f*(1.f-T)
                    +FVector(0,0,FMath::Sin(J*PI/16.f)*80.f*(1.f-T));
                Ribbon(Last,Next,35.f*(1.f-J/20.f),C); Last=Next;
            }
            break;
        }
        case ECombatVfxShape::Gatling:
        {
            for(int32 J=0;J<7;++J)
            {
                const FVector D=FVector::ForwardVector.RotateAngleAxis(Yaw-45.f+J*15.f,FVector::UpVector);
                Ribbon(E.Start+D*R*.15f,E.Start+D*R,3.f,C);
            }
            Arc(E.Start,30.f,8.f,Spin,270.f,C,25.f);
            break;
        }
        case ECombatVfxShape::Shield:
            for(int32 J=0;J<3;++J)
            { Arc(E.Start,R*FMath::Sqrt(1.f-FMath::Square(J*.28f)),4.f,Spin+J*45.f,300.f,C,J*R*.28f); }
            break;
        case ECombatVfxShape::Bolt:
        case ECombatVfxShape::Feather:
        {
            // Tracers describe the segment that already dealt damage; do not imply a delayed hit.
            Ribbon(E.Start,E.End,E.Shape==ECombatVfxShape::Feather?9.f:5.f,C);
            Sprite(E.End,9.f,C);
            FLinearColor Core=FLinearColor::LerpUsingHSV(C,FLinearColor::White,.65f); Core.A=C.A;
            Ribbon(E.Start,E.End,2.f,Core);
            if(E.Shape==ECombatVfxShape::Feather)
            {
                const FVector Tip=FMath::Lerp(E.Start,E.End,.82f);
                const FVector Side=FVector::CrossProduct(Direction,FVector::UpVector)*18.f;
                Ribbon(Tip-Direction*50.f-Side,Tip,6.f,C);
                Ribbon(Tip-Direction*50.f+Side,Tip,6.f,C);
            }
            break;
        }
        case ECombatVfxShape::Lightning:
        {
            FVector Last=E.Start;
            const FVector Side=FVector::CrossProduct(Direction,FVector::UpVector);
            for(int32 J=1;J<=12;++J)
            {
                const FVector Next=FMath::Lerp(E.Start,E.End,J/12.f)+Side*(J==12?0.f:FMath::Sin(J*17.13f)*22.f);
                Ribbon(Last,Next,E.bEvolved?7.f:4.f,C); Last=Next;
            }
            break;
        }
        case ECombatVfxShape::Slash:
            Arc(E.Start,R*(.7f+.3f*T),FMath::Max(7.f,R*.055f)*(1.f-T),Yaw-65.f+T*75.f,130.f,C,16.f);
            Arc(E.Start,R*.84f,3.f,Yaw-55.f+T*75.f,90.f,C,22.f);
            break;
        case ECombatVfxShape::Flame:
            Arc(E.Start,R*.88f,6.f,Spin,260.f,C);
            for(int32 J=0;J<12;++J)
            {
                const float A=J*2.39996f+Spin*.01f;
                const float D=R*FMath::Sqrt((J+.5f)/12.f);
                const FVector P=E.Start+FVector(FMath::Cos(A)*D,FMath::Sin(A)*D,6.f);
                Ribbon(P,P+FVector(FMath::Sin(A)*8.f,FMath::Cos(A)*8.f,25.f+18.f*FMath::Sin(Spin*.08f+J)),9.f,C);
            }
            break;
        case ECombatVfxShape::Orbital:
            Ribbon(E.Start+FVector(0,0,650.f*(1.f-T)),E.Start,12.f*(1.f-T),C);
            Arc(E.Start,R,4.f,Spin,360.f,C);
            Arc(E.Start,R*(.2f+.8f*T),12.f,0,360.f,C);
            break;
        case ECombatVfxShape::Drone:
            Arc(E.Start,R*.35f,7.f,Spin,270.f,C,30.f);
            Ribbon(E.Start+FVector(-20,0,30),E.Start+FVector(20,0,30),12.f,C);
            break;
        case ECombatVfxShape::Ring:
            Arc(E.Start,R,4.f,Spin,360.f,C);
            Arc(E.Start,R*.93f,2.f,-Spin,280.f,C,5.f);
            for(int32 J=0;J<8;++J)
            {
                const FVector D=FVector::ForwardVector.RotateAngleAxis(J*45.f+Spin*.1f,FVector::UpVector);
                Ribbon(E.Start+D*R*.84f,E.Start+D*R*.93f,3.f,C);
            }
            break;
        default:
            Arc(E.Start,R*(.18f+.82f*(1.f-FMath::Square(1.f-T))),E.bEvolved?13.f:8.f,0,360.f,C);
            Arc(E.Start,R*(.12f+.72f*T),3.f,Spin,300.f,C,8.f);
            break;
        }
        // Deterministic motes: bounded particle count, no world lights or smoke clouds.
        if(E.Shape!=ECombatVfxShape::Bolt && E.Shape!=ECombatVfxShape::Ring && E.Shape!=ECombatVfxShape::WindCurtain)
        {
            for(int32 J=0;J<(E.bEvolved?10:6);++J)
            {
                const float A=J*2.39996f;
                const FVector D(FMath::Cos(A),FMath::Sin(A),0.f);
                const FVector P=E.Start+D*R*(.2f+.7f*T)+FVector(0,0,18.f+35.f*FMath::Sin(PI*T));
                Ribbon(P,P+D*(6.f+R*.025f),3.f,C);
                Sprite(P,E.bEvolved?4.f:2.5f,C);
            }
        }
    }
    if(Vertices.IsEmpty())
    {
        if(LastVertexCount) { Mesh->ClearAllMeshSections(); LastVertexCount=0; }
        return;
    }
    if(LastVertexCount==Vertices.Num())
    { Mesh->UpdateMeshSection_LinearColor(0,Vertices,{},UVs,Colors,{}); }
    else
    { Mesh->CreateMeshSection_LinearColor(0,Vertices,Triangles,{},UVs,Colors,{},false); }
    LastVertexCount=Vertices.Num();
    if (!bReportedGeometry && FParse::Param(FCommandLine::Get(),TEXT("ArcadeVfxGallery")))
    {
        bReportedGeometry=true;
        UE_LOG(LogTemp,Display,TEXT("VFX mesh: verts=%d material=%s point=%s owner=%s alpha=%.3f hidden=%d"),
            Vertices.Num(),*GetNameSafe(Mesh->GetMaterial(0)),*Vertices[0].ToString(),*GetNameSafe(GetOwner()),Colors[0].A,IsHidden());
    }
}
