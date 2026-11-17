#pragma once

#include "UObject/Interface.h"
#include "CombatHealth.generated.h"

UINTERFACE(BlueprintType)
class UCombatHealth : public UInterface
{
    GENERATED_BODY()
};

class ICombatHealth
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Combat")
    float GetMissingHealthFraction() const;
};
