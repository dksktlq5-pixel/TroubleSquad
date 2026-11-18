#include "AnimaPreview.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraEmitterHandle.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"

#if WITH_EDITOR
void AAnimaPreviewCharacter::ReviewNiagaraAssets()
{
    InvulnerableEnd=GetCombatTime()+60.f; JinxWeaponReadyAt=GetCombatTime()+60.f;
    for(int32 I=1;I<=15;++I)
    {
        const FString Name=FString::Printf(TEXT("/Game/Basic_VFX/Niagara/NS_Basic_%d.NS_Basic_%d"),I,I);
        auto* System=LoadObject<UNiagaraSystem>(nullptr,*Name);
        if(!System) { continue; }
        System->WaitForCompilationComplete(true,false);
        TArray<FNiagaraVariable> Vars; System->GetExposedParameters().GetParameters(Vars);
        FString Names;
        for(const auto& V:Vars) { Names+=V.GetName().ToString()+TEXT(";"); }
        int32 Enabled=0;
        for(const auto& H:System->GetEmitterHandles()) { Enabled+=H.GetIsEnabled()?1:0; }
        UE_LOG(LogTemp,Display,TEXT("Niagara review: asset=%d emitters=%d enabled=%d ready=%d params=%s"),I,System->GetEmitterHandles().Num(),Enabled,System->IsReadyToRun(),*Names);
        const FVector P=GetActorLocation()+FVector(((I-1)%5-2)*190.f,((I-1)/5-1)*230.f,-70.f);
        if(auto* Component=UNiagaraFunctionLibrary::SpawnSystemAtLocation(this,System,P,FRotator::ZeroRotator,FVector(1.f),true,false,ENCPoolMethod::None,false))
        {
            Component->SetAllowScalability(false); Component->SetForceSolo(true); Component->Activate(true);
            Component->SetSystemFixedBounds(FBox(FVector(-500.f),FVector(500.f)));
            if(I==1)
            {
                FTimerHandle Inspect;
                GetWorldTimerManager().SetTimer(Inspect,FTimerDelegate::CreateWeakLambda(Component,[Component]()
                { UE_LOG(LogTemp,Display,TEXT("Niagara component: active=%d complete=%d bounds=%.2f"),Component->IsActive(),Component->IsComplete(),Component->Bounds.SphereRadius); }),.1f,false);
            }
            FTimerHandle Cleanup;
            GetWorldTimerManager().SetTimer(Cleanup,FTimerDelegate::CreateWeakLambda(Component,[Component]()
            { Component->DestroyComponent(); }),3.f,false);
        }
        DrawDebugString(GetWorld(),P+FVector(0,0,60),FString::FromInt(I),nullptr,FColor::White,3.f,false);
    }
    for(float Delay:{.05f,.3f,1.f})
    {
        FTimerHandle Shot;
        GetWorldTimerManager().SetTimer(Shot,FTimerDelegate::CreateWeakLambda(this,[this]()
        { if(auto* P=Cast<APlayerController>(GetController())) { P->ConsoleCommand(TEXT("Shot ShowUI"),true); } }),Delay,false);
    }
}
#endif
