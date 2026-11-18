#include "SwarmArenaGate.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ASwarmArenaGate::ASwarmArenaGate()
{
    bReplicates = true;
    SetReplicateMovement(true);
    bAlwaysRelevant = true;
    auto* Mesh = GetStaticMeshComponent();
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Surface(
        TEXT("/Game/TroubleSquad/Maps/ArenaMaterials/M_ArenaSteel.M_ArenaSteel"));
    if (Shape.Succeeded()) { Mesh->SetStaticMesh(Shape.Object); }
    if (Surface.Succeeded()) { Mesh->SetMaterial(0, Surface.Object); }
    Mesh->SetRelativeScale3D(FVector(.48f, 76.f, 2.6f));
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Mesh->SetMobility(EComponentMobility::Movable);
}
