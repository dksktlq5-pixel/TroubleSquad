#include "AnimaPreview.h"
#include "SwarmEncounter.h"
#include "SwarmLoot.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

namespace
{
const FSwarmAugmentInfo Catalog[] = {
    {TEXT("부분적 채식주의"),TEXT("처치한 적이 8% 확률로 회복 팩을 드롭합니다.")},
    {TEXT("죽음과 세금"),TEXT("처치한 적이 10% 확률로 골드를 드롭합니다.")},
    {TEXT("비축가"),TEXT("골드 1당 최대 체력의 0.5%를 회복합니다.")},
    {TEXT("전속력 돌진"),TEXT("이동 속도 +10%. 추가 이동 속도에 비례해 피해량 증가.")},
    {TEXT("궁극기 속도 상승"),TEXT("궁극기에 스킬 가속 100이 추가 적용됩니다.")},
    {TEXT("성쇠"),TEXT("8초마다 스킬 가속이 0~80 사이에서 변합니다.")},
    {TEXT("카드 수집가"),TEXT("획득 후 접근 카드마다 피해량 +5%.")},
    {TEXT("장거리"),TEXT("600 거리 밖의 적에게 피해량 +30%.")},
    {TEXT("영점 사격"),TEXT("300 거리 안의 적에게 피해량 +30%.")},
    {TEXT("한입 크기"),TEXT("크기 -20%, 최대 체력 -25%, 이동 속도 +15%, 가속 +20.")},
    {TEXT("생명 그 이상"),TEXT("크기 +20%, 이동 속도 -10%. 30초마다 체력 +30, 재생 +1.")},
    {TEXT("신진 대사 폭주"),TEXT("최대 체력 -60%. 초당 최대 체력의 20% 회복.")},
    {TEXT("속행"),TEXT("스킬 가속 1당 이동 속도 +0.2%.")},
    {TEXT("피로 회복제"),TEXT("경험치 +15%, 획득 반경 -100%. 60초마다 경험치와 골드 흡수.")},
    {TEXT("크기 교란기"),TEXT("8초마다 추가 범위 크기가 0~60%로 변합니다.")},
    {TEXT("유리 대포"),TEXT("피해량 +50%, 최대 체력 -30%.")},
    {TEXT("강타자"),TEXT("피해량 +50%, 스킬 가속 -30.")},
    {TEXT("속도 수집"),TEXT("이동 속도 -15%. 획득 후 레벨업마다 +3%.")},
    {TEXT("정서 보조 애완동물"),TEXT("촉수·티버·유미봇 피해량 +50%.")},
    {TEXT("탄환 애호가"),TEXT("피해량 -15%. 획득 후 6레벨마다 투사체 +1.")},
    {TEXT("전쟁기계"),TEXT("방어력 +10. 방어력의 50%만큼 피해량 비율 증가.")},
    {TEXT("중요 임무"),TEXT("치명타 확률 +10%, 치명타 배율 +25%포인트. 비치명타 피해 -20%.")},
    {TEXT("노련한 싸움꾼"),TEXT("경험치 +15%. 경험치 획득 후 4초간 피해·이동 속도 증가.")},
    {TEXT("축배!"),TEXT("레벨업 후 8초간 피해·범위·이동 속도 +30%, 가속 +30.")},
    {TEXT("능력치의 순환"),TEXT("8초마다 이동 속도·가속·범위·투사체·재생을 순환 강화.")},
    {TEXT("무차별 난사"),TEXT("투사체 +4, 피해량 -35%.")},
    {TEXT("대장정"),TEXT("2000 거리 이동마다 피해 +1%, 체력 +10, 범위 +1%.")},
    {TEXT("정예 폭격수"),TEXT("정예·미니 보스 피해 +30%. 처치 시 주변 폭발.")},
    {TEXT("방어력 상승"),TEXT("피격 시 방어력 +8, 최대 10회.")},
    {TEXT("치명적인 확장"),TEXT("최근 5초 치명타마다 범위 +1%, 최대 +60%.")},
    {TEXT("재생형 조직"),TEXT("회복 팩을 획득할 때마다 초당 체력 재생 +5.")},
    {TEXT("부동의 제물"),TEXT("정지 시 가속이 최대 +100. 초당 최대 체력 5% 소모.")}
};
static_assert(UE_ARRAY_COUNT(Catalog) == static_cast<int32>(ESwarmAugment::Count));
int32 Cycle(float Now) { return FMath::FloorToInt(Now / 8.f) % 5; }
}

const FSwarmAugmentInfo& GetSwarmAugmentInfo(uint8 Index)
{ return Catalog[FMath::Clamp<int32>(Index,0,UE_ARRAY_COUNT(Catalog)-1)]; }

void AAnimaPreviewCharacter::OnRep_Augments()
{
    SetActorScale3D(FVector((HasAugment(ESwarmAugment::BiteSize)?.8f:1.f)*(HasAugment(ESwarmAugment::Growing)?1.2f:1.f)));
}

TArray<uint8> AAnimaPreviewCharacter::GetAvailableAugmentChoices() const
{
    TArray<uint8> Pool;
    for(uint8 I=0;I<static_cast<uint8>(ESwarmAugment::Count);++I)
    { if(!OwnedAugments.Contains(I)&&(I<18||(I<26?bStoryAugmentsUnlocked:bExtremeAugmentsUnlocked))) { Pool.Add(FirstAugmentChoice+I); } }
    return Pool;
}

void AAnimaPreviewCharacter::GrantAugmentReward()
{
    if (!HasAuthority() || !GetController() || bDistrictFinished || OwnedAugments.Num()+PendingAugmentRewards>=4) { return; }
    ++PendingAugmentRewards;
    if (CombatHealth>0.f && (LevelChoices.IsEmpty() || !bAugmentRewardOpen)) { RollLevelChoices(); }
}

void AAnimaPreviewCharacter::ApplySwarmAugment(uint8 Index)
{
    if (!HasAuthority() || Index>=UE_ARRAY_COUNT(Catalog) || OwnedAugments.Contains(Index) || OwnedAugments.Num()>=4) { return; }
    const float Fraction=CombatHealth/FMath::Max(1.f,MaxCombatHealth);
    OwnedAugments.Add(Index);
    const auto Type=static_cast<ESwarmAugment>(Index);
    if(Type==ESwarmAugment::BiteSize) { MaxCombatHealth*=.75f; }
    if(Type==ESwarmAugment::Metabolic) { MaxCombatHealth*=.4f; }
    if(Type==ESwarmAugment::GlassCannon) { MaxCombatHealth*=.7f; }
    if(Type==ESwarmAugment::BiteSize) { SetActorScale3D(GetActorScale3D()*.8f); }
    if(Type==ESwarmAugment::Growing) { SetActorScale3D(GetActorScale3D()*1.2f); }
    CombatHealth=FMath::Clamp(MaxCombatHealth*Fraction,0.f,MaxCombatHealth);
    if(Type==ESwarmAugment::Magnet) { AugmentNextMagnetAt=GetCombatTime()+60.f; }
    if(Type==ESwarmAugment::Growing) { AugmentGrowthAt=GetCombatTime()+30.f; }
    // Each level-dependent augment starts accumulating when selected.
    if(Type==ESwarmAugment::SpeedCollector) { AugmentLevels=0; }
    if(Type==ESwarmAugment::BulletLover) { BulletLoverLevels=0; }
    ForceNetUpdate();
}

float AAnimaPreviewCharacter::GetAugmentHaste() const
{
    const float Now=GetCombatTime();
    return (HasAugment(ESwarmAugment::BiteSize)?20.f:0.f)-(HasAugment(ESwarmAugment::HeavyHitter)?30.f:0.f)
        +(HasAugment(ESwarmAugment::EbbFlow)?20.f*(FMath::FloorToInt(Now/8.f)%5):0.f)
        +(HasAugment(ESwarmAugment::Celebrate)&&Now<AugmentCelebrateEnd?30.f:0.f)
        +(HasAugment(ESwarmAugment::Cycle)&&Cycle(Now)==1?50.f:0.f)
        +(HasAugment(ESwarmAugment::Stationary)?FMath::Min(100.f,AugmentStationaryTime*20.f):0.f);
}

float AAnimaPreviewCharacter::GetAugmentMoveBonus() const
{
    const float Now=GetCombatTime();
    const float Haste=PermanentAbilityHaste+10.f*(RunWeaponStats.IsValidIndex(1)?RunWeaponStats[1]:0)+GetAugmentHaste();
    return (HasAugment(ESwarmAugment::FullSpeed)?.1f:0.f)+(HasAugment(ESwarmAugment::BiteSize)?.15f:0.f)
        -(HasAugment(ESwarmAugment::Growing)?.1f:0.f)+(HasAugment(ESwarmAugment::HasteMove)?.002f*FMath::Max(0.f,Haste):0.f)
        +(HasAugment(ESwarmAugment::SpeedCollector)?-.15f+.03f*AugmentLevels:0.f)
        +(HasAugment(ESwarmAugment::Experienced)&&Now<AugmentXpEnd?AugmentXpBonus:0.f)
        +(HasAugment(ESwarmAugment::Celebrate)&&Now<AugmentCelebrateEnd?.3f:0.f)
        +(HasAugment(ESwarmAugment::Cycle)&&Cycle(Now)==0?.4f:0.f);
}

float AAnimaPreviewCharacter::GetAugmentDamageMultiplier() const
{
    float Bonus=(HasAugment(ESwarmAugment::GlassCannon)?.5f:0.f)+(HasAugment(ESwarmAugment::HeavyHitter)?.5f:0.f)
        +(HasAugment(ESwarmAugment::CardCollector)?.05f*AugmentCardStacks:0.f)
        +(HasAugment(ESwarmAugment::Journey)?.01f*AugmentJourneyStacks:0.f)
        +(HasAugment(ESwarmAugment::WarMachine)?.005f*GetCombatArmor():0.f)
        +(HasAugment(ESwarmAugment::Celebrate)&&GetCombatTime()<AugmentCelebrateEnd?.3f:0.f)
        +(HasAugment(ESwarmAugment::Experienced)&&GetCombatTime()<AugmentXpEnd?AugmentXpBonus:0.f);
    if(HasAugment(ESwarmAugment::FullSpeed))
    {
        const float Speed=GetCharacterMovement()?GetCharacterMovement()->MaxWalkSpeed:BaseMoveSpeed;
        Bonus+=FMath::Max(0.f,Speed/FMath::Max(1.f,BaseMoveSpeed)-1.f);
    }
    return FMath::Max(.1f,1.f+Bonus)*(HasAugment(ESwarmAugment::BulletLover)?.85f:1.f)*(HasAugment(ESwarmAugment::Spray)?.65f:1.f);
}

float AAnimaPreviewCharacter::GetAugmentTargetMultiplier(AActor* Target) const
{
    if(!IsValid(Target)) { return 1.f; }
    const float Distance=FVector::Dist2D(GetActorLocation(),Target->GetActorLocation());
    float Bonus=(HasAugment(ESwarmAugment::LongRange)&&Distance>=600.f?.3f:0.f)
        +(HasAugment(ESwarmAugment::PointBlank)&&Distance<=300.f?.3f:0.f);
    const auto* Enemy=Cast<ASwarmEnemy>(Target);
    if(HasAugment(ESwarmAugment::EliteBomber)&&Enemy&&(Enemy->Kind==1||Enemy->Kind==2)) { Bonus+=.3f; }
    return (1.f+Bonus)*(HasAugment(ESwarmAugment::CriticalMission)?.8f:1.f);
}

float AAnimaPreviewCharacter::ResolveCriticalMultiplier(bool bCritical)
{
    if(bCritical)
    {
        if(HasAuthority()) { ++CareerCrits; }
        if(HasAuthority()&&HasAugment(ESwarmAugment::CriticalExpansion)) { AugmentCritTimes.Add(GetCombatTime()); }
        return HasAugment(ESwarmAugment::CriticalMission)?2.f/.8f:1.75f;
    }
    return 1.f;
}

float AAnimaPreviewCharacter::GetUltimateCooldown(float Base) const
{
    const float Normal=GetCombatCooldown(Base);
    if(!HasAugment(ESwarmAugment::UltimateHaste) || Base<=0.f) { return Normal; }
    const float Existing=Base/FMath::Max(.001f,Normal);
    return Base/(Existing+1.f);
}

float AAnimaPreviewCharacter::GetAugmentHealthMultiplier() const
{
    return (HasAugment(ESwarmAugment::BiteSize)?.75f:1.f)*(HasAugment(ESwarmAugment::Metabolic)?.4f:1.f)
        *(HasAugment(ESwarmAugment::GlassCannon)?.7f:1.f);
}

void AAnimaPreviewCharacter::NotifyAugmentEnemyDeath(FVector Position,bool bElite)
{
    if(!HasAuthority()) { return; }
    if(bElite&&HasAugment(ESwarmAugment::EliteBomber))
    {
        // Delay to avoid recursively entering the same death handler through a blast chain.
        GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this,Position]()
        {
            if(CombatHealth<=0.f||bDistrictFinished) { return; }
            ShowWeaponVfx(static_cast<uint8>(EGeneralWeapon::Bomb),Position,Position,600.f,1,true);
            for(AActor* Target:FindCombatTargets(Position,600.f)) { HitAdditionalEnemy(Target,500.f); }
        }));
    }
}

void AAnimaPreviewCharacter::NotifyAugmentExperience(int32 Amount)
{
    if(HasAugment(ESwarmAugment::Experienced))
    { AugmentXpBonus=GetCombatTime()<AugmentXpEnd?FMath::Min(.5f,AugmentXpBonus+.002f*Amount):FMath::Min(.5f,.002f*Amount); AugmentXpEnd=GetCombatTime()+4.f; }
}
void AAnimaPreviewCharacter::NotifyAugmentLevelUp(int32 Levels)
{
    if(Levels<=0) { return; }
    AugmentLevels+=Levels;
    BulletLoverLevels+=Levels;
    if(HasAugment(ESwarmAugment::Celebrate)) { AugmentCelebrateEnd=GetCombatTime()+8.f; }
}
void AAnimaPreviewCharacter::NotifyAugmentHealPickup()
{ if(HasAugment(ESwarmAugment::RegenTissue)) { AugmentRegenStacks+=5; } }

void AAnimaPreviewCharacter::TickAugments(float DeltaTime)
{
    if(!HasAuthority()||bDistrictFinished||CombatHealth<=0.f||!LevelChoices.IsEmpty()) { return; }
    const float Now=GetCombatTime();
    const FVector Position=GetActorLocation();
    const float Distance=AugmentPreviousPosition.IsZero()?0.f:FVector::Dist2D(Position,AugmentPreviousPosition);
    AugmentPreviousPosition=Position;
    // Teleports and arena transitions do not grant movement stacks.
    const bool bMoving=GetVelocity().SizeSquared2D()>25.f;
    AugmentStationaryTime=bMoving?0.f:AugmentStationaryTime+DeltaTime;
    if(HasAugment(ESwarmAugment::Journey)&&Distance<500.f)
    {
        AugmentJourneyDistance+=Distance;
        while(AugmentJourneyDistance>=2000.f) { AugmentJourneyDistance-=2000.f; ++AugmentJourneyStacks; MaxCombatHealth+=10.f; CombatHealth+=10.f; }
    }
    if(HasAugment(ESwarmAugment::Growing)&&Now>=AugmentGrowthAt)
    { AugmentGrowthAt=Now+30.f; MaxCombatHealth+=30.f; CombatHealth+=30.f; ++AugmentRegenStacks; }
    float Regen=(WorldBoonType==2&&GetWorldBoonRemaining()>0.f?.1f*MaxCombatHealth:0.f)+AugmentRegenStacks+(HasAugment(ESwarmAugment::Metabolic)?.2f*MaxCombatHealth:0.f)
        +(HasAugment(ESwarmAugment::Cycle)&&Cycle(Now)==4?20.f:0.f);
    if(HasAugment(ESwarmAugment::Stationary)&&!bMoving) { Regen-=MaxCombatHealth*.05f; }
    // The sacrifice cannot kill the player during a transition or a brief stop.
    CombatHealth=FMath::Clamp(CombatHealth+Regen*DeltaTime,1.f,MaxCombatHealth);
    AugmentCritTimes.RemoveAll([Now](float Time){return Now-Time>=5.f;});
    AugmentRecentCrits=FMath::Min(60,AugmentCritTimes.Num());
    if(HasAugment(ESwarmAugment::Magnet)&&Now>=AugmentNextMagnetAt)
    {
        AugmentNextMagnetAt=Now+60.f;
        BeginWorldVacuum(6.f*GetCombatDurationMultiplier());
    }
}

void AAnimaPreviewMode::RegisterYuumiQuestKill(FVector Position)
{
    if(YuumiKills<0 || YuumiType!=0 || FVector::DistSquared2D(Position,YuumiCenter)>FMath::Square(650.f)) { return; }
    YuumiKills=FMath::Min(40,YuumiKills+1);
}

void AAnimaPreviewMode::TickYuumiQuest(float Now)
{
    // Provisional cadence and defense objective; these are not measured video timings.
    static constexpr float Starts[]={80.f,320.f,480.f};
    if(YuumiKills<0 && NextYuumiQuest<UE_ARRAY_COUNT(Starts) && Now>=Starts[NextYuumiQuest] && Now<550.f)
    {
        for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
        {
            if(It->GetController()&&It->GetCombatHealth()>0.f)
            {
                YuumiCenter=It->GetActorLocation(); YuumiKills=0; YuumiEndsAt=Now+90.f;
                YuumiType=NextYuumiQuest%3; YuumiGoal=40; ++NextYuumiQuest;
                if(YuumiType!=0) { SpawnYuumiTokens(); }
                break;
            }
        }
    }
    if(YuumiKills>=0&&YuumiType!=0&&Now<YuumiEndsAt)
    {
        if(YuumiType==1)
        {
            for(TActorIterator<AAnimaPreviewCharacter> Player(GetWorld());Player;++Player)
            {
                if(Player->bYuumiCarryingToken)
                {
                    if(Player->GetCombatHealth()<=0.f||!Player->GetController()) { DropYuumiCargo(*Player); }
                    else if(FVector::DistSquared2D(Player->GetActorLocation(),YuumiCenter)<=FMath::Square(140.f))
                    { Player->bYuumiCarryingToken=false; ++YuumiKills; Player->ClientPlayCombatCue(4); }
                }
            }
        }
        for(int32 I=0;I<YuumiTokens.Num();++I)
        {
            auto* Token=YuumiTokens[I].Get();
            if(!IsValid(Token)||Token->IsActorBeingDestroyed()||(YuumiType==2&&I!=YuumiKills)) { continue; }
            for(TActorIterator<AAnimaPreviewCharacter> Player(GetWorld());Player;++Player)
            {
                if(Player->GetController()&&Player->GetCombatHealth()>0.f&&!Player->bYuumiCarryingToken&&FVector::DistSquared2D(Player->GetActorLocation(),Token->GetActorLocation())<=FMath::Square(105.f))
                { Token->Destroy(); if(YuumiType==1) { Player->bYuumiCarryingToken=true; } else { ++YuumiKills; } Player->ClientPlayCombatCue(0); break; }
            }
            if(YuumiType==2) { break; }
        }
        if(YuumiType==2&&YuumiTokens.IsValidIndex(YuumiKills)&&YuumiTokens[YuumiKills].IsValid())
        { YuumiCenter=YuumiTokens[YuumiKills]->GetActorLocation(); YuumiTokens[YuumiKills]->SetActorHiddenInGame(false); }
    }
    const bool bSuccess=YuumiKills>=YuumiGoal;
    const bool bEnd=YuumiKills>=0 && (bSuccess||Now>=YuumiEndsAt||Now>=550.f);
    for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It)
    {
        It->YuumiQuestKills=bEnd?-1:YuumiKills;
        It->YuumiQuestRemaining=YuumiKills<0||bEnd?0.f:FMath::Max(0.f,YuumiEndsAt-Now);
        It->YuumiQuestCenter=YuumiCenter;
        It->YuumiQuestType=YuumiType; It->YuumiQuestGoal=YuumiGoal;
        if(bSuccess&&!It->HasFinishedDistrict()) { It->GrantAugmentReward(); }
    }
    if(bEnd) { YuumiKills=-1; ClearYuumiTokens(); }
}

void AAnimaPreviewMode::ClearYuumiTokens()
{
    for(auto Token:YuumiTokens) { if(Token.IsValid()) { Token->Destroy(); } }
    YuumiTokens.Reset();
    for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It) { It->bYuumiCarryingToken=false; }
}

void AAnimaPreviewMode::DropYuumiCargo(AAnimaPreviewCharacter* Player)
{
    if(!Player||!Player->bYuumiCarryingToken) { return; }
    Player->bYuumiCarryingToken=false;
    if(YuumiType==1&&YuumiKills>=0)
    { if(auto* Token=GetWorld()->SpawnActor<ASwarmQuestToken>(Player->GetActorLocation()-FVector(0,0,40),FRotator::ZeroRotator)) { YuumiTokens.Add(Token); } }
}

void AAnimaPreviewMode::SpawnYuumiTokens()
{
    ClearYuumiTokens();
    const int32 Count=YuumiType==1?5:6;
    for(int32 I=0;I<Count;++I)
    {
        for(int32 Attempt=0;Attempt<8;++Attempt)
        {
            const float Angle=I*360.f/Count+Attempt*11.f;
            const FVector Candidate=YuumiCenter+FVector::ForwardVector.RotateAngleAxis(Angle,FVector::UpVector)*(450.f-Attempt*40.f);
            FHitResult Hit;
            if(!GetWorld()->LineTraceSingleByObjectType(Hit,Candidate+FVector(0,0,400),Candidate-FVector(0,0,500),FCollisionObjectQueryParams(ECC_WorldStatic))||Hit.ImpactNormal.Z<.8f) { continue; }
            if(auto* Token=GetWorld()->SpawnActor<ASwarmQuestToken>(Hit.ImpactPoint+FVector(0,0,45),FRotator(0,Angle,0)))
            { Token->SetActorHiddenInGame(YuumiType==2&&I>0); YuumiTokens.Add(Token); break; }
        }
    }
    YuumiGoal=YuumiTokens.Num();
    if(YuumiGoal==0) { YuumiType=0; YuumiGoal=40; }
    else if(YuumiType==2) { YuumiCenter=YuumiTokens[0]->GetActorLocation(); }
}

#if WITH_EDITOR
void AAnimaPreviewCharacter::RunAugmentAcceptanceTests()
{
    InvulnerableEnd=GetCombatTime()+600.f;
    auto Reset=[this]()
    {
        LevelChoices.Reset(); PendingLevelChoices=PendingAccessCards=PendingAugmentRewards=0;
        bAugmentRewardOpen=bAccessRewardOpen=false; EndLevelChoiceSelection();
        OwnedAugments.Reset(); AugmentCardStacks=AugmentArmorStacks=AugmentJourneyStacks=AugmentRegenStacks=0;
        AugmentLevels=BulletLoverLevels=0; AugmentCelebrateEnd=AugmentXpEnd=0.f;
        RunWeaponStats.Init(0,8); RunArmorBonus=0.f; PermanentAbilityHaste=0.f;
        MaxCombatHealth=1000.f; CombatHealth=1000.f; SetActorScale3D(FVector::OneVector);
    };
    int32 Configurations=0;
    for(uint8 Id=0;Id<static_cast<uint8>(ESwarmAugment::Count);++Id)
    {
        Reset(); ApplySwarmAugment(Id);
        const bool Valid=OwnedAugments.Num()==1&&HasAugment(static_cast<ESwarmAugment>(Id))
            &&FMath::IsFinite(GetCombatDamageMultiplier())&&GetCombatDamageMultiplier()>0.f
            &&FMath::IsFinite(GetAugmentMoveBonus())&&GetCombatCooldown(10.f)>0.f;
        Configurations+=Valid?1:0;
        ApplySwarmAugment(Id);
        ensureAlwaysMsgf(OwnedAugments.Num()==1,TEXT("Duplicate augment accepted"));
    }
    Reset(); ApplySwarmAugment(static_cast<uint8>(ESwarmAugment::GlassCannon));
    const bool Glass=FMath::IsNearlyEqual(MaxCombatHealth,700.f)&&FMath::IsNearlyEqual(GetAugmentDamageMultiplier(),1.5f);
    Reset(); ApplySwarmAugment(static_cast<uint8>(ESwarmAugment::Spray));
    const bool Spray=GetCombatBonusProjectiles()==4&&FMath::IsNearlyEqual(GetAugmentDamageMultiplier(),.65f);
    Reset(); ApplySwarmAugment(static_cast<uint8>(ESwarmAugment::UltimateHaste));
    const bool Ultimate=FMath::IsNearlyEqual(GetUltimateCooldown(90.f),45.f)&&FMath::IsNearlyEqual(GetCombatCooldown(90.f),90.f);
    Reset(); ApplySwarmAugment(static_cast<uint8>(ESwarmAugment::CardCollector)); CollectAccessCard();
    const bool Cards=AugmentCardStacks==1&&FMath::IsNearlyEqual(GetAugmentDamageMultiplier(),1.05f);
    Reset(); ApplySwarmAugment(static_cast<uint8>(ESwarmAugment::BulletLover)); NotifyAugmentLevelUp(5);
    const bool BeforeSix=GetCombatBonusProjectiles()==0; NotifyAugmentLevelUp(1);
    const bool Levels=BeforeSix&&GetCombatBonusProjectiles()==1;
    Reset(); ApplySwarmAugment(static_cast<uint8>(ESwarmAugment::CriticalMission));
    auto* Enemy=GetWorld()->SpawnActor<ASwarmEnemy>(GetActorLocation()+FVector(300.f,0,0),FRotator::ZeroRotator);
    const float NonCrit=Enemy?GetAugmentTargetMultiplier(Enemy)*ResolveCriticalMultiplier(false):0.f;
    const float Crit=Enemy?GetAugmentTargetMultiplier(Enemy)*ResolveCriticalMultiplier(true):0.f;
    const bool Critical=FMath::IsNearlyEqual(NonCrit,.8f)&&FMath::IsNearlyEqual(Crit,2.f);
    if(Enemy) { Enemy->Destroy(); }
    Reset(); PendingLevelChoices=1; GrantAugmentReward();
    const bool Three=IsAugmentRewardOpen()&&LevelChoices.Num()==3&&LevelChoices[0]!=LevelChoices[1]&&LevelChoices[1]!=LevelChoices[2]&&LevelChoices[0]!=LevelChoices[2];
    const int32 Sequence=LevelChoiceSequence; ServerSelectLevelChoice_Implementation(0,Sequence);
    const bool Queue=OwnedAugments.Num()==1&&PendingLevelChoices==1&&!LevelChoices.IsEmpty()&&!bAugmentRewardOpen;
    const int32 Count=OwnedAugments.Num(); ServerSelectLevelChoice_Implementation(0,Sequence);
    const bool Replay=OwnedAugments.Num()==Count&&PendingLevelChoices==1;
    Reset(); for(uint8 I=0;I<5;++I) { ApplySwarmAugment(I); }
    const bool Cap=OwnedAugments.Num()==4;
    auto* Orb=GetWorld()->SpawnActor<ASwarmExperienceOrb>(GetActorLocation()+FVector(1000.f,0,0),FRotator::ZeroRotator);
    const bool Persistent=Orb&&Orb->GetLifeSpan()==0.f; if(Orb) { Orb->Destroy(); }
    UE_LOG(LogTemp,Display,TEXT("Augment acceptance: catalog=%d/32 glass=%d spray=%d ultimate=%d cards=%d levels=%d critical=%d three=%d queue=%d replay=%d cap=%d xpPersistent=%d"),
        Configurations,Glass,Spray,Ultimate,Cards,Levels,Critical,Three,Queue,Replay,Cap,Persistent);
    Reset();
}

void AAnimaPreviewMode::RunYuumiAcceptanceTest()
{
    NextYuumiQuest=0; YuumiKills=-1; TickYuumiQuest(120.f);
    const bool Started=YuumiKills==0&&NextYuumiQuest==1;
    RegisterYuumiQuestKill(YuumiCenter+FVector(2000.f,0,0));
    const bool Outside=YuumiKills==0;
    for(int32 I=0;I<40;++I) { RegisterYuumiQuestKill(YuumiCenter); }
    TickYuumiQuest(121.f);
    int32 Rewards=0; for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It) { Rewards+=It->IsAugmentRewardOpen()?1:0; }
    const bool Complete=YuumiKills==-1&&Rewards>0;
    TickYuumiQuest(480.f); TickYuumiQuest(571.f);
    const bool TimedOut=YuumiKills==-1&&NextYuumiQuest==2;
    UE_LOG(LogTemp,Display,TEXT("Yuumi acceptance: started=%d outside=%d complete=%d timeout=%d rewards=%d"),Started,Outside,Complete,TimedOut,Rewards);
}

void AAnimaPreviewMode::RunYuumiVariantAcceptanceTest()
{
    AAnimaPreviewCharacter* Player=nullptr;
    for(TActorIterator<AAnimaPreviewCharacter> It(GetWorld());It;++It) { if(It->GetController()) { Player=*It; break; } }
    if(!Player) { UE_LOG(LogTemp,Error,TEXT("Yuumi variants: no player")); return; }
    const FVector Original=Player->GetActorLocation();
    NextYuumiQuest=1; YuumiKills=-1; TickYuumiQuest(480.f);
    const bool CollectStarted=YuumiType==1&&YuumiGoal>0;
    const FVector Base=YuumiCenter; const auto Tokens=YuumiTokens;
    bool Carry=true;
    for(auto Token:Tokens)
    {
        if(!Token.IsValid()) { Carry=false; continue; }
        Player->SetActorLocation(Token->GetActorLocation()+FVector(0,0,44)); TickYuumiQuest(481.f);
        Carry &= Player->bYuumiCarryingToken;
        Player->SetActorLocation(Base); TickYuumiQuest(482.f);
    }
    const bool Delivered=YuumiKills==-1&&Player->PendingAugmentRewards==1;
    NextYuumiQuest=2; TickYuumiQuest(720.f);
    const bool RaceStarted=YuumiType==2&&YuumiGoal>0;
    const int32 Goal=YuumiGoal;
    for(int32 I=0;I<Goal&&YuumiKills>=0;++I)
    { Player->SetActorLocation(YuumiCenter+FVector(0,0,44)); TickYuumiQuest(721.f+I); }
    const bool Race=YuumiKills==-1&&Player->PendingAugmentRewards==2;
    NextYuumiQuest=1; TickYuumiQuest(480.f);
    const int32 Before=YuumiTokens.Num();
    if(YuumiTokens.Num()>0&&YuumiTokens[0].IsValid())
    { Player->SetActorLocation(YuumiTokens[0]->GetActorLocation()+FVector(0,0,44)); TickYuumiQuest(481.f); }
    DropYuumiCargo(Player);
    const bool Dropped=!Player->bYuumiCarryingToken&&YuumiTokens.Num()==Before+1;
    ClearYuumiTokens(); YuumiKills=-1; Player->SetActorLocation(Original);
    UE_LOG(LogTemp,Display,TEXT("Yuumi variants: collectStarted=%d carrying=%d delivered=%d raceStarted=%d race=%d drop=%d"),CollectStarted,Carry,Delivered,RaceStarted,Race,Dropped);
}
#endif
