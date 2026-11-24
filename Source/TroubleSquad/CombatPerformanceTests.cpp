#include "AnimaPreview.h"
#include "SwarmEncounter.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "TimerManager.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"

#if WITH_EDITOR
void AAnimaPreviewCharacter::StartRenderStressTest()
{
    InvulnerableEnd=GetCombatTime()+120.f;
    SignatureWeaponLevel=5; bSignatureWeaponEvolved=true; ExtraSkillCharges=2;
    GeneralWeapons={static_cast<uint8>(EGeneralWeapon::Launcher),static_cast<uint8>(EGeneralWeapon::Blade),
        static_cast<uint8>(EGeneralWeapon::Crossbow),static_cast<uint8>(EGeneralWeapon::Fire)};
    if(FParse::Param(FCommandLine::Get(),TEXT("WeaponModelStressTest"))){GeneralWeapons={1,2,4,6,7,11,17};}
    GeneralWeaponRanks.Init(5,GeneralWeaponCount); EvolvedGeneralWeapons=GeneralWeapons;
    RunWeaponStats.Init(5,8); PermanentAbilityHaste=20.f; PermanentProjectiles=2;
    OwnedAugments={static_cast<uint8>(ESwarmAugment::Spray),static_cast<uint8>(ESwarmAugment::CriticalExpansion),static_cast<uint8>(ESwarmAugment::Pets)};
    for(TActorIterator<ASwarmEnemy> It(GetWorld());It;++It) { It->Destroy(); }
    int32 Spawned=0;
    for(int32 I=0;I<180;++I)
    {
        const FVector Offset=FVector::ForwardVector.RotateAngleAxis(I*137.508f,FVector::UpVector)*(500.f+I%7*75.f);
        const FVector Candidate=GetActorLocation()+Offset;
        FHitResult Ground;
        if(!GetWorld()->LineTraceSingleByObjectType(Ground,Candidate+FVector(0,0,300),Candidate-FVector(0,0,500),FCollisionObjectQueryParams(ECC_WorldStatic))||Ground.ImpactNormal.Z<.8f) { continue; }
        FTransform Transform(Ground.ImpactPoint+FVector(0,0,68));
        auto* Enemy=GetWorld()->SpawnActorDeferred<ASwarmEnemy>(ASwarmEnemy::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if(Enemy) { Enemy->Tags.Add(TEXT("RenderStressTarget")); Enemy->ModelVariant=I%10; Enemy->FinishSpawning(Transform); ++Spawned; }
    }
    if(auto* ViewController=Cast<APlayerController>(GetController())) { ViewController->ConsoleCommand(TEXT("t.MaxFPS 60"),false); }
    RenderStressFrames.Reset(); RenderStressStart=FPlatformTime::Seconds(); bRenderStressRunning=true;
    UE_LOG(LogTemp,Display,TEXT("Render stress started: spawned=%d character=%d targetFPS=60 warmupSeconds=8 sampleSeconds=20"),Spawned,CharacterChoice);
}

void AAnimaPreviewCharacter::TickRenderStressTest()
{
    if(!bRenderStressRunning) { return; }
    const double Elapsed=FPlatformTime::Seconds()-RenderStressStart;
    if(Elapsed>=8.0) { RenderStressFrames.Add(FApp::GetDeltaTime()*1000.0); }
    if(GetCombatTime()>=RenderStressNextCast)
    {
        RenderStressNextCast=GetCombatTime()+.5f;
        const FVector Aim=GetActorLocation()+GetActorForwardVector()*350.f;
        ServerCastCharacterSkill_Implementation(false,Aim); ServerCastCharacterSkill_Implementation(true,Aim);
    }
    if(Elapsed<28.0) { return; }
    bRenderStressRunning=false; RenderStressFrames.Sort();
    double Total=0.0; for(double Frame:RenderStressFrames) { Total+=Frame; }
    const int32 N=RenderStressFrames.Num();
    const double Median=N?RenderStressFrames[N/2]:0.0;
    const double P95=N?RenderStressFrames[FMath::Min(N-1,FMath::FloorToInt(N*.95))]:0.0;
    int32 Enemies=0; for(TActorIterator<ASwarmEnemy> It(GetWorld());It;++It) { ++Enemies; }
    UE_LOG(LogTemp,Display,TEXT("Render stress complete: samples=%d medianMs=%.2f p95Ms=%.2f averageFPS=%.1f enemies=%d projectiles=%d"),N,Median,P95,Total>0.f?1000.0*N/Total:0.0,Enemies,GeneralProjectiles.Num());
    FScreenshotRequest::RequestScreenshot(false);
    FTimerHandle ExitTimer;
    GetWorldTimerManager().SetTimer(ExitTimer,[](){FPlatformMisc::RequestExit(false);},2.f,false);
}
#endif
