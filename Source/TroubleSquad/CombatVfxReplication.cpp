#include "AnimaPreview.h"

void AAnimaPreviewCharacter::QueueCombatVfx(FCombatVfxEvent Event)
{
    if(GetNetMode()!=NM_DedicatedServer) { DispatchCombatVfx(Event); }
    if(HasAuthority() && GetNetMode()!=NM_Standalone)
    {
        Event.CreatedAt=GetCombatTime();
        if(PendingCombatVfx.Num()>=192) { PendingCombatVfx.RemoveAt(0,1,EAllowShrinking::No); }
        PendingCombatVfx.Add(MoveTemp(Event));
    }
}

void AAnimaPreviewCharacter::DispatchCombatVfx(const FCombatVfxEvent& E)
{
    switch(E.Kind)
    {
    case 0: RenderWeaponVfx(E.Weapon,E.Start,E.End,E.Radius,E.Variant,E.bEnhanced); break;
    case 1: RenderJinxTracer(E.Start,E.End,E.Variant); break;
    case 2: RenderSeraphineEffect(E.Start,E.Radius,E.bEnhanced); break;
    case 3: RenderJinxExplosion(E.Start,E.Radius,E.bEnhanced); break;
    case 4: RenderLeonaShieldStrike(E.Start,E.Direction,E.Radius,E.bEnhanced); break;
    case 5: RenderTentacleSlam(E.Start,E.End,E.Radius,E.bEnhanced); break;
    default: break;
    }
}

void AAnimaPreviewCharacter::FlushCombatVfx(float Now)
{
    if(!HasAuthority() || PendingCombatVfx.IsEmpty() || Now<NextVfxBatchAt) { return; }
    NextVfxBatchAt=Now+.05f;
    PendingCombatVfx.RemoveAll([Now](const FCombatVfxEvent& E) { return Now-E.CreatedAt>.2f; });
    if(PendingCombatVfx.IsEmpty()) { return; }
    const int32 Count=FMath::Min(24,PendingCombatVfx.Num());
    TArray<FCombatVfxEvent> Batch;
    Batch.Append(PendingCombatVfx.GetData(),Count);
    PendingCombatVfx.RemoveAt(0,Count,EAllowShrinking::No);
    MulticastCombatVfxBatch(Batch);
    if(FParse::Param(FCommandLine::Get(),TEXT("VfxNetworkReview")))
    { UE_LOG(LogTemp,Display,TEXT("VFX batch sent: events=%d"),Count); }
}

void AAnimaPreviewCharacter::MulticastCombatVfxBatch_Implementation(const TArray<FCombatVfxEvent>& Events)
{
    if(HasAuthority()) { return; }
    for(const FCombatVfxEvent& E:Events)
    {
        DispatchCombatVfx(E);
        if(E.ReviewToken && FParse::Param(FCommandLine::Get(),TEXT("VfxNetworkReview")))
        { UE_LOG(LogTemp,Display,TEXT("VFX network sample received: token=%d"),E.ReviewToken); }
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("VfxNetworkReview")))
    { UE_LOG(LogTemp,Display,TEXT("VFX batch received: events=%d"),Events.Num()); }
}

void AAnimaPreviewCharacter::ShowWeaponVfx(uint8 Weapon,FVector_NetQuantize Start,FVector_NetQuantize End,float Radius,uint8 Phase,bool bEvolved)
{
    FCombatVfxEvent E; E.Kind=0; E.Weapon=Weapon; E.Start=Start; E.End=End;
    E.Radius=Radius; E.Variant=Phase; E.bEnhanced=bEvolved; QueueCombatVfx(E);
}
void AAnimaPreviewCharacter::ShowJinxTracer(FVector_NetQuantize Start,FVector_NetQuantize End,uint8 Variant)
{
    FCombatVfxEvent E; E.Kind=1; E.Start=Start; E.End=End; E.Variant=Variant; QueueCombatVfx(E);
}
void AAnimaPreviewCharacter::ShowSeraphineEffect(FVector_NetQuantize Center,float Radius,bool bUltimate)
{
    FCombatVfxEvent E; E.Kind=2; E.Start=Center; E.Radius=Radius; E.bEnhanced=bUltimate; QueueCombatVfx(E);
}
void AAnimaPreviewCharacter::ShowJinxExplosion(FVector_NetQuantize Position,float Radius,bool bUltimate)
{
    FCombatVfxEvent E; E.Kind=3; E.Start=Position; E.Radius=Radius; E.bEnhanced=bUltimate; QueueCombatVfx(E);
}
void AAnimaPreviewCharacter::ShowLeonaShieldStrike(FVector_NetQuantize Center,FVector_NetQuantizeNormal Direction,float Radius,bool bEvolved)
{
    FCombatVfxEvent E; E.Kind=4; E.Start=Center; E.Direction=Direction; E.Radius=Radius; E.bEnhanced=bEvolved; QueueCombatVfx(E);
}

void AAnimaPreviewCharacter::ShowTentacleSlam(FVector Start,FVector End,float HalfWidth,bool bGiant)
{
    FCombatVfxEvent E; E.Kind=5; E.Start=Start; E.End=End; E.Radius=HalfWidth; E.bEnhanced=bGiant; QueueCombatVfx(E);
}
