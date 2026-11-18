#pragma once

#include "GameFramework/Actor.h"
#include "SwarmBolt.generated.h"

UCLASS()
class ASwarmBolt : public AActor
{
    GENERATED_BODY()

public:
    ASwarmBolt();
    void Tick(float DeltaSeconds) override;
    float Damage = 0.f;
};
