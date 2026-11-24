#pragma once
#include "CoreMinimal.h"

enum class ESwarmBehavior : uint8 { Chaser,Ranged,Dasher,Exploder,Spawner,Divider,Shield,Heavy };
struct FSwarmArchetype { float Health,Speed; int32 Experience; ESwarmBehavior Behavior; };
inline FSwarmArchetype GetSwarmArchetype(uint8 Kind,uint8 Variant)
{
    using B=ESwarmBehavior;
    static const FSwarmArchetype Common[]={
        {.6f,1.35f,2,B::Chaser},{.75f,1.1f,3,B::Ranged},{1.f,1.f,4,B::Chaser},{.85f,1.1f,4,B::Chaser},
        {1.8f,.85f,6,B::Chaser},{.7f,1.65f,4,B::Dasher},{.65f,1.15f,4,B::Exploder},{.8f,1.1f,4,B::Ranged},
        {1.2f,.9f,5,B::Divider},{2.2f,.6f,8,B::Spawner}
    };
    static const FSwarmArchetype Heavy[]={
        {1.5f,.8f,12,B::Heavy},{1.6f,.8f,12,B::Heavy},{2.f,.7f,14,B::Shield},{1.25f,1.1f,10,B::Dasher},
        {1.3f,1.f,10,B::Ranged},{1.1f,1.3f,9,B::Dasher},{1.5f,1.f,12,B::Dasher}
    };
    return Kind==0?Common[Variant%UE_ARRAY_COUNT(Common)]:Kind==1?Heavy[Variant%UE_ARRAY_COUNT(Heavy)]:FSwarmArchetype{1.f,1.f,Kind==2?(Variant>=8?6+Variant%2:28):0,B::Chaser};
}
