#include "SwarmBolt.h"
#include "AnimaPreview.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ASwarmBolt::ASwarmBolt()
{
    bReplicates = true;
    SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick = true;
    InitialLifeSpan = 1.9f;
    Tags.Add(TEXT("EnemyProjectile"));
    auto* Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoltVisual"));
    SetRootComponent(Visual);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(
        TEXT("/Game/TroubleSquad/Maps/ArenaMaterials/M_ArenaCoral.M_ArenaCoral"));
    Visual->SetStaticMesh(Shape.Object);
    Visual->SetMaterial(0, Glow.Object);
    Visual->SetRelativeScale3D(FVector(.48f, .20f, .20f));
}

void ASwarmBolt::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority()) { return; }
    const FVector Start = GetActorLocation();
    const FVector End = Start + GetActorForwardVector() * (825.f * DeltaSeconds);
    constexpr float ProjectileRadius = 15.f;
    float FirstWallHit = TNumericLimits<float>::Max();
    for (TActorIterator<AAnimaPreviewCharacter> It(GetWorld()); It; ++It)
    {
        float WallHit = 0.f;
        if (It->TraceWindWall(Start, End, ProjectileRadius, WallHit))
        {
            FirstWallHit = FMath::Min(FirstWallHit, WallHit);
        }
    }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SwarmBolt), false, this);
    Query.AddIgnoredActor(GetOwner());
    FCollisionObjectQueryParams Objects(ECC_Pawn);
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    TArray<FHitResult> Hits;
    GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, Objects,
        FCollisionShape::MakeSphere(ProjectileRadius), Query);
    for (const FHitResult& Hit : Hits)
    {
        if (Hit.Time >= FirstWallHit) { break; }
        if (auto* Player = Cast<AAnimaPreviewCharacter>(Hit.GetActor()); Player && Player->GetCombatHealth() > 0.f)
        {
            UGameplayStatics::ApplyDamage(Player, Damage, nullptr, this, UDamageType::StaticClass());
            Destroy();
            return;
        }
        if (Hit.GetComponent() && Hit.GetComponent()->GetCollisionObjectType() == ECC_WorldStatic)
        {
            Destroy();
            return;
        }
    }
    if (FirstWallHit <= 1.f) { Destroy(); }
    else { SetActorLocation(End); }
}
