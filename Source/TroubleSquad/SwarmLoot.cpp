#include "SwarmLoot.h"
#include "SwarmEncounter.h"
#include "AnimaPreview.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"
#include "Materials/MaterialInterface.h"

ASwarmLootCrate::ASwarmLootCrate()
{
    bReplicates=true;
    Tags={FName(TEXT("Enemy")),FName(TEXT("LootCrate"))};
    Collision=CreateDefaultSubobject<USphereComponent>(TEXT("CrateHitVolume"));
    SetRootComponent(Collision); Collision->InitSphereRadius(45.f);
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Collision->SetCollisionObjectType(ECC_Pawn);
    Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
    Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SupplyCase")); Body->SetupAttachment(Collision);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Case(TEXT("/Game/ModSci_EngiProps/Meshes/SM_Case_A.SM_Case_A"));
    if(Case.Succeeded())
    {
        Body->SetStaticMesh(Case.Object);
        const FBox Bounds=Case.Object->GetBoundingBox();
        const float Scale=80.f/FMath::Max(1.f,Bounds.GetSize().GetMax());
        Body->SetRelativeScale3D(FVector(Scale));
        Body->SetRelativeLocation(FVector(0.f,0.f,-40.f-Bounds.Min.Z*Scale));
    }
}

float ASwarmLootCrate::TakeDamage(float Damage,const FDamageEvent& Event,AController* EventInstigator,AActor* Causer)
{
    if(!HasAuthority()||Health<=0.f||Damage<=0.f||!FMath::IsFinite(Damage)) { return 0.f; }
    auto* Attacker=Cast<AAnimaPreviewCharacter>(Causer);
    if(Attacker) { Damage*=Attacker->GetCombatDamageMultiplier()*Attacker->GetCombatTargetDamageMultiplier(this); }
    const float Applied=FMath::Min(Damage,Health); Health-=Applied;
    if(Attacker) { Attacker->NotifyChampionDamageDealt(Applied); }
    if(Health<=0.f)
    {
        const int32 Drop=SwarmLoot::CrateDrop(FMath::FRand());
        if(Drop!=INDEX_NONE) { ASwarmExperienceOrb::SpawnPickup(GetWorld(),GetActorLocation()+FVector(0,0,38),static_cast<uint8>(Drop)); }
        Destroy();
    }
    return Applied;
}

void ASwarmLootCrate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ASwarmLootCrate,Health); }

ASwarmQuestToken::ASwarmQuestToken()
{
    bReplicates=true;
    Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("QuestSupplyToken")); SetRootComponent(Body);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision); Body->SetCastShadow(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Game/ModSci_EngiProps/Meshes/SM_Toolbox.SM_Toolbox"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/TroubleSquad/CombatVisuals/M_SummonCyan.M_SummonCyan"));
    if(Mesh.Succeeded())
    {
        Body->SetStaticMesh(Mesh.Object); const FBox Bounds=Mesh.Object->GetBoundingBox();
        Body->SetRelativeScale3D(FVector(70.f/FMath::Max(1.f,Bounds.GetSize().GetMax())));
        if(Glow.Succeeded()) { Body->SetMaterial(0,Glow.Object); }
    }
}
