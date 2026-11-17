#pragma once

#include "GameFramework/HUD.h"
#include "CombatHUD.generated.h"

class UTexture2D;
class UFont;

UCLASS()
class ACombatHUD : public AHUD
{
    GENERATED_BODY()

public:
    void DrawHUD() override;
    void NotifyHitBoxClick(FName BoxName) override;

private:
    UPROPERTY(Transient) TObjectPtr<UTexture2D> Portrait;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> Minimap;
    UPROPERTY(Transient) TObjectPtr<UFont> HudFont;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> CombatIcons[45];
    bool bLoadedMinimap = false;
    int32 PortraitIndex = INDEX_NONE;
    bool bSettingsOpen = false;
};
