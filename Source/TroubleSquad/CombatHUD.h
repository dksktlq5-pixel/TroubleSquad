#pragma once

#include "GameFramework/HUD.h"
#include "CombatHUD.generated.h"

class UTexture2D;
class UFont;
class UTextureRenderTarget2D;
class ASceneCapture2D;

UCLASS()
class ACombatHUD : public AHUD
{
    GENERATED_BODY()
    UPROPERTY() TArray<TObjectPtr<UTexture2D>> GeneralIcons;

public:
    void DrawHUD() override;
    void ToggleSettings();
    bool IsSettingsOpen() const;
    void AddDamageNumber(FVector Position,float Amount);
    struct FDamageNumber { FVector Position; float Amount; double Born; };
    TArray<FDamageNumber> DamageNumbers;
    void DrawDamageNumbers();
    void EndPlay(const EEndPlayReason::Type Reason) override;
    void ShowSkillRange(FVector Center, float Radius, FLinearColor Color, float Duration, float Heading = 0.f, float HalfAngle = 180.f);
    void NotifyHitBoxClick(FName BoxName) override;

private:
    struct FSkillRange
    {
        FVector Center;
        float Radius;
        FLinearColor Color;
        double ExpiresAt;
        float Duration;
        float Heading;
        float HalfAngle;
    };
    TArray<FSkillRange> SkillRanges;
    void DrawSkillRanges(float Scale);
    UPROPERTY(Transient) TObjectPtr<UTexture2D> Portrait;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> Minimap;
    UPROPERTY(Transient) TObjectPtr<UFont> HudFont;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> CombatIcons[45];
    UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> ArenaMinimap;
    UPROPERTY(Transient) TObjectPtr<ASceneCapture2D> MinimapCapture;
    void CaptureArenaMinimap();
    static constexpr float MinimapWorldWidth = 10600.f;
    bool bLoadedMinimap = false;
    bool bTacticalMinimap = false;
    float LastHudHealthFraction = 1.f;
    float DamageTrailFraction = 1.f;
    double HealthDamageAt = -10.;
    int32 PortraitIndex = INDEX_NONE;
    bool bSettingsOpen = false;
};
