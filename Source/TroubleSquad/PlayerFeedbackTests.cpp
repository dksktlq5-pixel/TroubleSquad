#include "AnimaPreview.h"
#include "SwarmEncounter.h"
#include "SwarmArenaGate.h"
#include "SwarmArenas.h"
#include "CombatHUD.h"
#include "ClientPreferences.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UnrealClient.h"
#include "HAL/FileManager.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/Paths.h"
#if WITH_EDITOR
void AAnimaPreviewCharacter::RunChampionMotionTest()
{
    int32 Loaded=0,Moving=0,Closed=0; bool Priority=true,Stable=true,Returned=true;
    for(int32 Champion=0;Champion<9;++Champion)
    {
        ApplyCharacterChoice(Champion);
        for(uint8 Slot=0;Slot<3;++Slot)
        {
            CombatMotionTime=0.f;
            if(!AttackMotions.IsValidIndex(Slot)||!AttackMotions[Slot]) { continue; }
            ++Loaded;PlayCombatMotion_Implementation(Slot);
            auto* Node=GetMesh()->GetSingleNodeInstance(); if(!Node) { continue; }
            auto Sample=[&](float Time)
            {
                Node->SetPosition(Time,false);GetMesh()->TickAnimation(0.f,false);GetMesh()->RefreshBoneTransforms();
                return FTransform(GetMesh()->GetBoneQuaternion(TEXT("R_Hand"),EBoneSpaces::ComponentSpace),GetMesh()->GetBoneLocation(TEXT("R_Hand"),EBoneSpaces::ComponentSpace));
            };
            const FTransform First=Sample(0.f),Strike=Sample(AttackMotions[Slot]->GetPlayLength()*.28f),Last=Sample(AttackMotions[Slot]->GetPlayLength());
            const bool Changed=!First.Equals(Strike,.01f),Seam=First.Equals(Last,.15f);
            Moving+=Changed;Closed+=Seam;
            const float Remaining=CombatMotionTime;PlayCombatMotion_Implementation(0);
            Stable&=FMath::IsNearlyEqual(Remaining,CombatMotionTime,.001f);
            Priority&=CurrentCombatMotion==Slot;
            UpdateCombatMotion(5.f,false);Returned&=CombatMotionTime<=0.f;
            UE_LOG(LogTemp,Display,TEXT("Champion motion clip: champion=%d slot=%d duration=%.3f moving=%d seam=%d"),Champion,Slot,AttackMotions[Slot]->GetPlayLength(),Changed,Seam);
        }
    }
    ApplyCharacterChoice(0);
    UE_LOG(LogTemp,Display,TEXT("Champion motion acceptance: loaded=%d moving=%d closed=%d priority=%d stableFire=%d returned=%d"),Loaded,Moving,Closed,Priority,Stable,Returned);
}

void AAnimaPreviewCharacter::RunLabTraversalTest()
{
    for(TActorIterator<ASwarmArenaGate> It(GetWorld());It;++It) { It->Destroy(); }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(LabTraversal),false,this);
    for(TActorIterator<ASwarmEnemy> It(GetWorld());It;++It) { Query.AddIgnoredActor(*It); }
    auto Clear=[&](FVector A,FVector B)
    {
        A.Z=B.Z=145.f; FHitResult Hit;
        const bool Blocked=GetWorld()->SweepSingleByChannel(Hit,A,B,FQuat::Identity,ECC_WorldStatic,FCollisionShape::MakeCapsule(35.f,88.f),Query);
        if(Blocked) { UE_LOG(LogTemp,Display,TEXT("Lab traversal obstacle: %s at %s"),*Hit.GetActor()->GetActorLabel(),*Hit.ImpactPoint.ToString()); }
        return !Blocked;
    };
    int32 Passed=0,Total=0,Supported=0,Samples=0;
    auto Segment=[&](FVector A,FVector B)
    {
        ++Total; Passed+=Clear(A,B);
        const int32 Steps=FMath::CeilToInt(FVector::Dist2D(A,B)/100.f);
        for(int32 I=0;I<=Steps;++I)
        {
            FVector P=FMath::Lerp(A,B,float(I)/FMath::Max(1,Steps)); FHitResult Ground;
            ++Samples; const bool Floor=GetWorld()->LineTraceSingleByChannel(Ground,P+FVector(0,0,500),P-FVector(0,0,500),ECC_WorldStatic,Query); Supported+=Floor;
            if(!Floor) { UE_LOG(LogTemp,Display,TEXT("Lab unsupported: %s"),*P.ToString()); }
        }
    };
    const TArray<FVector> Route={FVector(-4250,-560,0),FVector(1900,-560,0),FVector(2150,0,0),FVector(3100,0,0),FVector(4000,0,0),FVector(4300,-800,0),FVector(4300,-1150,0)};
    for(int32 I=1;I<Route.Num();++I) { Segment(Route[I-1],Route[I]); }
    for(float X:{-2600.f,-400.f,1800.f}) { for(float Y:{-1200.f,1200.f}) { Segment(FVector(X,Y-550,0),FVector(X,Y+550,0)); } }
    Segment(FVector(-3850,-2550,0),FVector(-3400,-2550,0)); Segment(FVector(2950,2550,0),FVector(3400,2550,0));
    const bool WallBlocked=!Clear(FVector(-3700,-3250,0),FVector(-3700,-2650,0));
    UE_LOG(LogTemp,Display,TEXT("Lab traversal: clear=%d/%d supported=%d/%d negativeControl=%d"),Passed,Total,Supported,Samples,WallBlocked);
    for(TActorIterator<ASwarmEnemy> It(GetWorld());It;++It) {It->Destroy();}
    int32 HeroPass=0,EnemyPass=0;bool WaterSafe=true;
    const auto Walk=[&](ACharacter* Walker,float X,float Y,int32 Sign)
    {
        auto* Movement=Walker->GetCharacterMovement();Movement->bRunPhysicsWithNoController=true;Movement->StopMovementImmediately();
        Walker->SetActorLocation(FVector(X,Y-Sign*600.f,Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+15.f),false,nullptr,ETeleportType::TeleportPhysics);
        Movement->SetMovementMode(MOVE_Falling);Movement->MaxWalkSpeed=600.f;
        for(int32 Step=0;Step<300;++Step)
        {
            Walker->AddMovementInput(FVector(0,Sign,0),1.f,true);Movement->TickComponent(.02f,LEVELTICK_All,nullptr);
            WaterSafe&=!SwarmArenas::IsLabWater(Walker->GetActorLocation());
            if(Sign*(Walker->GetActorLocation().Y-Y)>560.f) {return true;}
        }
        UE_LOG(LogTemp,Display,TEXT("Lab walking blocked: actor=%s x=%.0f y=%.0f sign=%d end=%s mode=%d"),*Walker->GetName(),X,Y,Sign,*Walker->GetActorLocation().ToString(),Movement->MovementMode.GetValue());return false;
    };
    for(int32 Champion=0;Champion<9;++Champion)
    {ApplyCharacterChoice(Champion);for(float X:{-2600.f,-400.f,1800.f}) {for(float Y:{-1200.f,1200.f}) {for(int32 Sign:{-1,1}) {HeroPass+=Walk(this,X,Y,Sign);}}}}
    SetActorLocation(FVector(-1200,0,110),false);
    for(int32 Kind=0;Kind<3;++Kind)
    {
        const FTransform T(FVector(-1200,0,120));auto* Enemy=GetWorld()->SpawnActorDeferred<ASwarmEnemy>(ASwarmEnemy::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);Enemy->Kind=Kind;Enemy->ModelVariant=Kind==2?6:0;Enemy->FinishSpawning(T);
        for(float X:{-2600.f,-400.f,1800.f}) {for(float Y:{-1200.f,1200.f}) {for(int32 Sign:{-1,1}) {EnemyPass+=Walk(Enemy,X,Y,Sign);}}}Enemy->Destroy();
    }
    bool WaterBlocked=true;
    for(float Y:{-1200.f,1200.f})
    {FHitResult Hit;WaterBlocked&=GetWorld()->SweepSingleByChannel(Hit,FVector(-1500,Y-500,110),FVector(-1500,Y+500,110),FQuat::Identity,ECC_WorldStatic,FCollisionShape::MakeCapsule(31,88),Query);}
    const FVector InWater(-1500,-1200,110),Safe=SwarmArenas::LabPositionOnLand(InWater,72);
    UE_LOG(LogTemp,Display,TEXT("Lab movement: heroes=%d/108 enemies=%d/36 waterSafe=%d waterBlocked=%d corrected=%d"),HeroPass,EnemyPass,WaterSafe,WaterBlocked,!SwarmArenas::IsLabWater(Safe)&&!Safe.Equals(InWater));
    ApplyCharacterChoice(0);SetActorLocation(FVector(-1200,0,110),false);

}

void AAnimaPreviewCharacter::RunPlayerFeedbackTest()
{
    if(FParse::Param(FCommandLine::Get(),TEXT("SwarmAttackMotionTest")))
    {
        bool Loaded=true,Playing=true,Restored=true,Priority=true;int32 Clips=0;
        for(int I=0;I<9;++I)
        {
            ApplyCharacterChoice(I);
            for(int Slot=0;Slot<3;++Slot)
            {
                CombatMotionTime=0;Loaded&=AttackMotions.IsValidIndex(Slot)&&AttackMotions[Slot]!=nullptr;
                if(!AttackMotions.IsValidIndex(Slot)||!AttackMotions[Slot]){Playing=false;continue;}
                PlayCombatMotion(Slot);
                auto* Node=GetMesh()->GetSingleNodeInstance();Playing&=Node&&Node->GetCurrentAsset()==AttackMotions[Slot];++Clips;
                if(Slot>0){PlayCombatMotion(0);Priority&=CurrentCombatMotion==Slot;}
                UpdateCombatMotion(AttackMotions[Slot]->GetPlayLength()+.01f,false);
                Node=GetMesh()->GetSingleNodeInstance();Restored&=Node&&Node->GetCurrentAsset()==Idle;
            }
        }
        UE_LOG(LogTemp,Display,TEXT("Attack motion acceptance: loaded=%d playing=%d restored=%d priority=%d clips=%d"),Loaded,Playing,Restored,Priority,Clips);
        return;
    }

    bool Sizes=true,Directions=true;
    for(int I=0;I<9;++I){ApplyCharacterChoice(I);const auto* M=GetMesh()->GetSkeletalMeshAsset();Sizes&=M&&FMath::IsNearlyEqual(M->GetBounds().BoxExtent.Z*2.f*GetMesh()->GetRelativeScale3D().Z,180.f,.1f);Directions&=FMath::IsNearlyEqual(GetMesh()->GetRelativeRotation().Yaw,-90.f,.1f);}
    ApplyCharacterChoice(0);const float Now=GetCombatTime();const FVector Aim=GetActorLocation()+FVector(1000,0,0);
    CombatLevel=5;JinxRReadyAt=0;ServerCastCharacterSkill(true,Aim);const bool Locked=JinxRReadyAt==0;
    CombatLevel=6;ServerCastCharacterSkill(true,Aim);const bool Unlocked=JinxRReadyAt>Now;
    PermanentAbilityHaste=0;RunWeaponStats.Init(0,8);OwnedAugments.Reset();JinxPassiveEnd=XayahBuffEnd=0;WorldBoonEnd=0;
    PreviousSkillCooldownFactor=PreviousUltimateCooldownFactor=-1;RefreshSkillHaste();JinxEReadyAt=JinxRReadyAt=ExtraChargeReadyAt=Now+10.f;
    RunWeaponStats[1]=5;PermanentAbilityHaste=50;RefreshSkillHaste();
    const bool Haste=FMath::IsNearlyEqual(JinxEReadyAt-Now,5.f,.02f)&&FMath::IsNearlyEqual(JinxRReadyAt-Now,5.f,.02f)&&FMath::IsNearlyEqual(ExtraChargeReadyAt-Now,5.f,.02f);
    bool Settings=false;auto* PC=Cast<APlayerController>(GetController());auto* HUD=PC?Cast<ACombatHUD>(PC->GetHUD()):nullptr;
    if(HUD){const FString Before=UGameplayStatics::GetCurrentLevelName(this);HUD->ToggleSettings();Settings=HUD->IsSettingsOpen()&&Before==UGameplayStatics::GetCurrentLevelName(this);HUD->ToggleSettings();Settings&=!HUD->IsSettingsOpen();}
    for(TActorIterator<ASwarmEnemy> It(GetWorld());It;++It){It->Destroy();}
    bool Rigs=true,Footing=true;int32 RigCount=0;
    for(int32 I=0;I<25;++I){FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;FTransform T(GetActorLocation()+FVector(500+(I/5)*150,(I%5-2)*180,0));auto* Enemy=GetWorld()->SpawnActorDeferred<ASwarmEnemy>(ASwarmEnemy::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);if(!Enemy){Rigs=false;continue;}Enemy->Kind=I<10?0:I<17?1:2;Enemy->ModelVariant=I<10?I:I<17?I-10:I-17;Enemy->FinishSpawning(T);Rigs&=Enemy->bAnimatedLocomotion;RigCount+=Enemy->bAnimatedLocomotion?1:0;Footing&=FMath::IsNearlyEqual(Enemy->GetMesh()->GetRelativeLocation().Z+Enemy->BodyMinZ*Enemy->BodyScale,-Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),.1f);Enemy->Freeze(10.f);Enemy->GetMesh()->SetPlayRate(1.f);}
    FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Hit=GetWorld()->SpawnActor<ASwarmEnemy>(GetActorLocation()+FVector(150,0,0),FRotator::ZeroRotator,P);
    const int32 BeforeNumbers=HUD?HUD->DamageNumbers.Num():0;
    if(Hit){UGameplayStatics::ApplyDamage(Hit,7.f,GetController(),this,UDamageType::StaticClass());}
    const bool Damage=HUD&&HUD->DamageNumbers.Num()>BeforeNumbers;
    UE_LOG(LogTemp,Display,TEXT("Player feedback: sizes=%d directions=%d ultimateLocked=%d ultimateUnlocked=%d haste=%d settings=%d rigs=%d footing=%d damage=%d rigCount=%d"),Sizes,Directions,Locked,Unlocked,Haste,Settings,Rigs,Footing,Damage,RigCount);
    InvulnerableEnd=Now+30.f;
    if(FParse::Param(FCommandLine::Get(),TEXT("SettingsVisualReview"))){ClientPreferences::Toggle(PC);}
    FTimerHandle Shot;GetWorldTimerManager().SetTimer(Shot,[](){FScreenshotRequest::RequestScreenshot(true);},.4f,false);
}

void AAnimaPreviewCharacter::RunEnemyMotionTest()
{
    for(TActorIterator<ASwarmEnemy> It(GetWorld());It;++It){It->Destroy();}
    InvulnerableEnd=JinxWeaponReadyAt=GetCombatTime()+120.f;GeneralWeapons.Reset();
    TArray<TWeakObjectPtr<ASwarmEnemy>> Targets;TArray<FVector> Starts;TArray<FQuat> Poses;
    for(int I=0;I<3;++I)
    {
        FTransform T(GetActorLocation()+FVector(600,(I-1)*200,0));
        auto* E=GetWorld()->SpawnActorDeferred<ASwarmEnemy>(ASwarmEnemy::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        E->Kind=0;E->ModelVariant=I+2;E->FinishSpawning(T);
        E->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        E->GetMesh()->TickAnimation(0,false);E->GetMesh()->RefreshBoneTransforms();
        Targets.Add(E);Starts.Add(E->GetActorLocation());
        Poses.Add(E->GetMesh()->GetBoneQuaternion(E->GetMesh()->GetBoneName(E->GetMesh()->GetNumBones()-1),EBoneSpaces::ComponentSpace));
    }
    FTimerHandle First,Check;
    GetWorldTimerManager().SetTimer(First,[](){FScreenshotRequest::RequestScreenshot(true);},.05f,false);
    GetWorldTimerManager().SetTimer(Check,FTimerDelegate::CreateWeakLambda(this,[this,Targets,Starts,Poses]()
    {
        bool Upright=true,Facing=true,Moving=true,Gait=true;
        for(int I=0;I<Targets.Num();++I)
        {
            const auto* E=Targets[I].Get();if(!E){Upright=Facing=Moving=Gait=false;continue;}
            const auto* M=E->GetMesh();
            Upright&=M->GetUpVector().Z>.99f;
            const FVector ToPlayer=(GetActorLocation()-E->GetActorLocation()).GetSafeNormal2D();
            Facing&=FVector::DotProduct(E->GetActorForwardVector(),ToPlayer)>.98f;
            Moving&=FVector::Dist2D(E->GetActorLocation(),Starts[I])>15.f;
            Gait&=FQuat::ErrorAutoNormalize(M->GetBoneQuaternion(M->GetBoneName(M->GetNumBones()-1),EBoneSpaces::ComponentSpace),Poses[I])>.001f;
        }
        UE_LOG(LogTemp,Display,TEXT("Enemy motion: upright=%d facing=%d moving=%d animated=%d count=%d"),Upright,Facing,Moving,Gait,Targets.Num());
        FScreenshotRequest::RequestScreenshot(true);
    }),.65f,false);
}
void AAnimaPreviewCharacter::RunHealthMinimapReview()
{
    if(auto* Display=UGameUserSettings::GetGameUserSettings()) { Display->SetScreenResolution(FIntPoint(1920,1080));Display->SetFullscreenMode(EWindowMode::Windowed);Display->ApplyResolutionSettings(false); }
    ApplyCharacterChoice(5); CombatLevel=11; MaxCombatHealth=1610.f; CombatHealth=1111.f;
    InvulnerableEnd=GetCombatTime()+30.f; JinxWeaponReadyAt=GetCombatTime()+30.f;
    OwnedAugments.Reset(); PermanentRegen=0.f; RunRegen=0.f; SetActorLocation(FVector(0,UGameplayStatics::GetCurrentLevelName(this)==TEXT("L_SubterraneanLab_Rebuilt")?0.f:1500.f,120),false,nullptr,ETeleportType::TeleportPhysics);
    IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("HealthMinimapReview")/UGameplayStatics::GetCurrentLevelName(this)),true);
    auto Capture=[this](const TCHAR* Name)
    {
        UE_LOG(LogTemp,Display,TEXT("Health minimap review: %s health=%.0f max=%.0f fraction=%.4f"),Name,CombatHealth,MaxCombatHealth,GetCombatHealthFraction());
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("HealthMinimapReview")/UGameplayStatics::GetCurrentLevelName(this)/(FString(Name)+TEXT(".png")),true,false);
    };
    FTimerHandle First,Damage,Second,Heal,Third;
    GetWorldTimerManager().SetTimer(First,FTimerDelegate::CreateWeakLambda(this,[Capture](){Capture(TEXT("Health69"));}),3.f,false);
    GetWorldTimerManager().SetTimer(Damage,FTimerDelegate::CreateWeakLambda(this,[this](){CombatHealth=322.f;}),5.3f,false);
    GetWorldTimerManager().SetTimer(Second,FTimerDelegate::CreateWeakLambda(this,[Capture](){Capture(TEXT("Health20"));}),5.4f,false);
    GetWorldTimerManager().SetTimer(Heal,FTimerDelegate::CreateWeakLambda(this,[this](){CombatHealth=1288.f;}),8.f,false);
    GetWorldTimerManager().SetTimer(Third,FTimerDelegate::CreateWeakLambda(this,[Capture](){Capture(TEXT("Health80"));}),9.f,false);
}
#endif
