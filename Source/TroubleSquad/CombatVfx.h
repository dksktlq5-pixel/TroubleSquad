#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatVfx.generated.h"

class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
enum class ECombatVfxShape : uint8 { Pulse, Ring, Slash, Bolt, Lightning, Feather, Flame, Orbital, Drone, Train, Disc, Music, Tentacle, Gatling, Shield, Tornado, WindCurtain, TentacleSlam, SpikedMine, ModelTrail, SoundWave, SpiritBolt };

// Local cosmetic renderer: all ribbons and particles for one pawn share one mesh section.
UCLASS(NotBlueprintable, Transient)
class TROUBLESQUAD_API ACombatVfx : public AActor
{
    GENERATED_BODY()
public:
    ACombatVfx();
    virtual void Tick(float DeltaSeconds) override;
    void Emit(ECombatVfxShape Shape, FVector Start, FVector End, float Radius,
        FLinearColor Color, float Lifetime, bool bEvolved = false, int32 StateKey = INDEX_NONE, int32 Priority = 0);
    void EmitModel(uint8 Kind,FVector Start,FVector End,float Size,float Lifetime,bool bTravel,bool bEvolved=false);
    int32 GetActiveModelCount() const { return ModelEffects.Num(); }
    int32 GetModelInstanceCount() const;
    void Clear();
    int32 GetActiveCount() const { return Effects.Num(); }
private:
    struct FEffect
    {
        ECombatVfxShape Shape;
        FVector Start, End;
        FLinearColor Color;
        float Radius, Age, Lifetime;
        bool bEvolved;
        int32 Key;
        int32 Priority;
    };
    struct FModelEffect
    {
        uint8 Kind; FVector Start,End; float Size,Age,Lifetime,SpinAge; bool bTravel,bEvolved;
    };
    TArray<FModelEffect> ModelEffects;
    UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> ModelPools;
    void TickModels(float DeltaSeconds);
    TArray<FEffect> Effects;
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> Mesh;
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    bool bReportedGeometry = false;
    int32 LastVertexCount = 0;
    void Ribbon(FVector A, FVector B, float Width, FLinearColor Color);
    void Sprite(FVector Center, float Radius, FLinearColor Color);
    void Stamp(uint8 Kind,FVector Position,float Yaw,FVector Scale,FLinearColor Color);
    void Arc(FVector Center, float Radius, float Width, float StartAngle, float Sweep, FLinearColor Color, float Height = 0.f);
};
