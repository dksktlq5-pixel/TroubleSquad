#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatHealth.h"
#include "SwarmLoot.generated.h"

class USphereComponent;
class UStaticMeshComponent;

namespace SwarmLoot
{
    constexpr uint8 Bomb=8,Magnet=9,Boon=10,Supercar=11,LastKind=11;
    // Tunable drop weights; -1 is an empty crate.
    inline int32 CrateDrop(float Roll)
    { return Roll<.04f?Magnet:Roll<.08f?Bomb:Roll<.38f?6:Roll<.88f?5:INDEX_NONE; }
}

UCLASS()
class ASwarmLootCrate : public AActor, public ICombatHealth
{
    GENERATED_BODY()
public:
    ASwarmLootCrate();
    float TakeDamage(float Damage,const FDamageEvent& Event,AController* EventInstigator,AActor* Causer) override;
    float GetMissingHealthFraction_Implementation() const override { return 1.f-Health/64.f; }
    void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
    UPROPERTY() TObjectPtr<USphereComponent> Collision;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(Replicated) float Health=64.f;
};

UCLASS()
class ASwarmQuestToken : public AActor
{
    GENERATED_BODY()
public:
    ASwarmQuestToken();
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
};
