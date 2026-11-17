#include "CombatHUD.h"
#include "AnimaPreview.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "GlobalRenderResources.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Texture2D.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputCoreTypes.h"

namespace
{
const FLinearColor Panel(.008f, .024f, .025f, .98f);
const FLinearColor Gold(.36f, .27f, .135f);
const FLinearColor Ink(.005f, .012f, .018f, .87f);
const FLinearColor Text(.91f, .89f, .80f);
const FLinearColor Green(.035f, .46f, .055f);
const FLinearColor Purple(.22f, .085f, .40f);
}

void ACombatHUD::DrawHUD()
{
    Super::DrawHUD();
    const auto* Pawn = Cast<AAnimaPreviewCharacter>(GetOwningPawn());
    if (!Canvas || !Pawn)
    {
        return;
    }

    const float Scale = FMath::Min(Canvas->ClipX / 1920.f, Canvas->ClipY / 1080.f);
    const float W = Canvas->ClipX / Scale;
    const float H = Canvas->ClipY / Scale;
    const auto Rect = [&](float X, float Y, float Width, float Height, FLinearColor Color)
    {
        DrawRect(Color, X * Scale, Y * Scale, Width * Scale, Height * Scale);
    };
    const auto Frame = [&](float X, float Y, float Width, float Height, FLinearColor Edge)
    {
        Rect(X - 1, Y + 2, Width + 2, Height, Ink);
        Rect(X, Y, Width, Height, Edge);
        Rect(X + 2, Y + 2, Width - 4, Height - 4, Panel);
        Rect(X + 5, Y + 4, Width - 10, 1, Edge.CopyWithNewOpacity(.46f));
    };
    if (!HudFont)
    {
        HudFont = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
    }
    const auto Label = [&](const FString& Value, float X, float Y, float Width, float Height,
        float Size, FLinearColor Color = Text, bool bCentered = true)
    {
        UFont* Font = HudFont ? HudFont.Get() : GEngine->GetMediumFont();
        const float FontScale = Size * 1.40f * Scale;
        float TextWidth = 0.f;
        float TextHeight = 0.f;
        GetTextSize(Value, TextWidth, TextHeight, Font, FontScale);
        const float Left = X * Scale + (bCentered ? (Width * Scale - TextWidth) * .5f : 0.f);
        const float Top = Y * Scale + (Height * Scale - TextHeight) * .5f;
        DrawText(Value, Ink, Left + Scale, Top + Scale, Font, FontScale, false);
        DrawText(Value, Color, Left, Top, Font, FontScale, false);
    };

    const int32 Index = Pawn->GetSelectedCharacter();
    if (PortraitIndex != Index)
    {
        static const TCHAR* Names[] = {TEXT("Jinx"), TEXT("Seraphine"), TEXT("Leona"), TEXT("Illaoi"),
            TEXT("Briar"), TEXT("Yasuo"), TEXT("Riven"), TEXT("Aurora"), TEXT("Xayah")};
        PortraitIndex = Index;
        const TCHAR* Name = Index == 0 ? TEXT("BattleCatJinx") : Names[FMath::Clamp(Index, 0, 8)];
        Portrait = LoadObject<UTexture2D>(nullptr,
            *FString::Printf(TEXT("/Game/TroubleSquad/UI/T_ChampionHD_%s.T_ChampionHD_%s"), Name, Name));
    }

    const AGameStateBase* State = GetWorld()->GetGameState();
    const int32 Elapsed = FMath::Max(0, FMath::FloorToInt(State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds()));
    Frame(W / 2 - 67, 0, 134, 54, Gold);
    Label(FString::Printf(TEXT("%02d:%02d"), Elapsed / 60, Elapsed % 60), W / 2 - 67, 0, 134, 54, 1.68f);

    const float PanelX = W / 2 - 210;
    const float PanelY = H - 124;
    Frame(PanelX, PanelY, 420, 118, Gold);
    const auto Disk = [&](FVector2D Center, double Radius, FLinearColor Color,
        const FTexture* Texture, FVector2D UVCenter, FVector2D UVRadius)
    {
        TArray<FCanvasUVTri> Triangles;
        constexpr int32 Segments = 40;
        Triangles.Reserve(Segments);
        for (int32 Segment = 0; Segment < Segments; ++Segment)
        {
            const double Angle = 2.0 * UE_PI * Segment / Segments;
            const double Next = 2.0 * UE_PI * (Segment + 1) / Segments;
            const FVector2D A(FMath::Cos(Angle), FMath::Sin(Angle));
            const FVector2D B(FMath::Cos(Next), FMath::Sin(Next));
            auto& Tri = Triangles.AddDefaulted_GetRef();
            Tri.V0_Pos = Center * Scale;
            Tri.V1_Pos = (Center + A * Radius) * Scale;
            Tri.V2_Pos = (Center + B * Radius) * Scale;
            Tri.V0_UV = UVCenter;
            Tri.V1_UV = UVCenter + A * UVRadius;
            Tri.V2_UV = UVCenter + B * UVRadius;
            Tri.V0_Color = Tri.V1_Color = Tri.V2_Color = Color;
        }
        FCanvasTriangleItem Item(Triangles, Texture ? Texture : GWhiteTexture);
        Canvas->DrawItem(Item);
    };
    const auto Triangle = [&](FVector2D A, FVector2D B, FVector2D C, FLinearColor Color)
    {
        FCanvasTriangleItem Item(A * Scale, B * Scale, C * Scale, GWhiteTexture);
        Item.SetColor(Color);
        Canvas->DrawItem(Item);
    };
    const FLinearColor CounterBackground(.013f, .027f, .041f, .9f);
    const FLinearColor MetallicGold(.95f, .71f, .24f);
    const FLinearColor Silver(.73f, .84f, .85f);
    const auto Counter = [&](float X, FLinearColor Accent, int32 Value)
    {
        constexpr float Y = 8;
        constexpr float Width = 116;
        constexpr float Height = 32;
        constexpr float Bevel = 12;
        Triangle({X, Y + Height}, {X + Bevel, Y}, {X + Width, Y}, CounterBackground);
        Triangle({X, Y + Height}, {X + Width, Y}, {X + Width - Bevel, Y + Height}, CounterBackground);
        Rect(X + Bevel, Y, Width - Bevel, 1, Accent.CopyWithNewOpacity(.55f));
        Label(FString::FromInt(Value), X + 42, Y, 53, Height, 1.08f);
    };
    Counter(W - 245, MetallicGold, 0);
    Counter(W - 125, Silver, Pawn->GetEnemyKills());

    const FVector2D Coin(W - 215, 24);
    Disk(Coin + FVector2D(2, 2), 10, Gold, nullptr, {}, {});
    Disk(Coin, 10, MetallicGold, nullptr, {}, {});
    Disk(Coin, 7, FLinearColor(.55f, .30f, .055f), nullptr, {}, {});
    Rect(Coin.X - 3, Coin.Y - 4, 6, 8, MetallicGold);

    const FVector2D Monster(W - 95, 24);
    const FLinearColor MonsterShade(.25f, .36f, .42f);
    Disk(Monster, 8, MonsterShade, nullptr, {}, {});
    Triangle(Monster + FVector2D(-8, 2), Monster + FVector2D(-11, -11), Monster + FVector2D(-3, -5), Silver);
    Triangle(Monster + FVector2D(8, 2), Monster + FVector2D(11, -11), Monster + FVector2D(3, -5), Silver);
    Triangle(Monster + FVector2D(-8, -2), Monster + FVector2D(8, -2), Monster + FVector2D(0, 10), Silver);
    Triangle(Monster + FVector2D(-6, 0), Monster + FVector2D(-1, 1), Monster + FVector2D(-2, 4), Ink);
    Triangle(Monster + FVector2D(6, 0), Monster + FVector2D(1, 1), Monster + FVector2D(2, 4), Ink);

    const FVector2D PortraitCenter(PanelX - 33, PanelY + 59);
    Disk(PortraitCenter, 48, Gold, nullptr, {}, {});
    Disk(PortraitCenter, 46, FLinearColor(.57f, .43f, .23f), nullptr, {}, {});
    Disk(PortraitCenter, 43, Ink, nullptr, {}, {});
    if (Portrait && Portrait->GetResource())
    {
        static const FVector2D Faces[] = {{.606,.138},{.655,.443},{.501,.197},
            {.508,.182},{.612,.350},{.679,.322},{.539,.238},{.531,.343},{.812,.239}};
        constexpr double HalfHeight = .127;
        const double HalfWidth = HalfHeight * Portrait->GetSizeY() / Portrait->GetSizeX();
        Disk(PortraitCenter, 40, FLinearColor::White, Portrait->GetResource(),
            Faces[FMath::Clamp(Index, 0, 8)], FVector2D(HalfWidth, HalfHeight));
    }
    Disk(PortraitCenter + FVector2D(31, 42), 14, Gold, nullptr, {}, {});
    Disk(PortraitCenter + FVector2D(31, 42), 11, Ink, nullptr, {}, {});
    Label(FString::FromInt(Pawn->GetCombatLevel()), PanelX - 16, PanelY + 87, 28, 28, .85f);
    const auto LoadCombatIcon = [&](int32 IconIndex) -> UTexture2D*
    {
        if (Index < 0 || Index >= 9)
        {
            return nullptr;
        }
        static const TCHAR* Champions[] = {TEXT("Jinx"), TEXT("Seraphine"), TEXT("Leona"),
            TEXT("Illaoi"), TEXT("Briar"), TEXT("Yasuo"), TEXT("Riven"), TEXT("Aurora"), TEXT("Xayah")};
        static const TCHAR* Types[] = {TEXT("Passive"), TEXT("E"), TEXT("R"), TEXT("Weapon"), TEXT("Evolution")};
        static const TCHAR* JinxAssets[] = {TEXT("T_JinxAbility_Passive"), TEXT("T_JinxAbility_E"),
            TEXT("T_JinxAbility_R"), TEXT("T_JinxWeapon_Meow"), TEXT("T_JinxWeapon_Evolved")};
        auto& Icon = CombatIcons[Index * 5 + IconIndex];
        if (!Icon)
        {
            const FString Name = Index == 0 ? FString(JinxAssets[IconIndex])
                : FString::Printf(TEXT("T_%s_%s"), Champions[Index], Types[IconIndex]);
            Icon = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/TroubleSquad/UI/%s.%s"), *Name, *Name));
        }
        return Icon.Get();
    };
    for (int32 Slot = 0; Slot < 8; ++Slot)
    {
        const float X = PanelX + 16 + 49 * Slot;
        Rect(X, PanelY + 15, 44, 44, FLinearColor(.027f, .046f, .047f));
        Rect(X + 2, PanelY + 17, 40, 40, Ink);
        if (Slot == 3 && Index <= 8)
        {
            UTexture2D* Icon = LoadCombatIcon(Pawn->IsSignatureWeaponEvolved() ? 4 : 3);
            if (Icon)
            {
                DrawTexture(Icon, (X + 3) * Scale, (PanelY + 15) * Scale, 38 * Scale, 38 * Scale,
                    0, 0, 1, 1);
            }
            Rect(X + 3, PanelY + 42, 38, 15, Ink);
            Label(FString::FromInt(Pawn->GetSignatureWeaponLevel()), X + 3, PanelY + 42, 14, 15, .75f);
            Label(Index == 6 ? FString::FromInt(FMath::FloorToInt(Pawn->GetRivenCharge())) : Index == 5 ? FString::FromInt(FMath::FloorToInt(Pawn->GetYasuoFlow())) : FString::Printf(TEXT("%.1f"), Pawn->GetJinxWeaponRemaining()), X + 17, PanelY + 42, 24, 15, .75f);
        }
        if (Slot < 3 && Index <= 8)
        {
            UTexture2D* Icon = LoadCombatIcon(Slot);
            const bool bPassive = Slot == 0;
            const float Remaining = bPassive ? (Index == 8 ? Pawn->GetXayahBuffRemaining() : Pawn->GetJinxPassiveRemaining()) : Pawn->GetJinxSkillRemaining(Slot == 2);
            const bool bActive = bPassive && Remaining > 0.f;
            const bool bCoolingDown = !bPassive && Remaining > 0.f;
            const float Y = PanelY + 15.f;
            const FLinearColor Rim = bActive ? MetallicGold : bCoolingDown ? FLinearColor(.28f, .28f, .23f) : Gold;
            Rect(X, Y, 44, 44, Rim);
            Rect(X + 1, Y + 1, 42, 42, Ink);
            if (Icon && Icon->GetResource())
            {
                DrawTexture(Icon, (X + 1) * Scale, (Y + 1) * Scale, 42 * Scale, 42 * Scale, 0, 0, 1, 1);
            }
            if (bPassive && (Index == 0 || Index == 8))
            {
                Rect(X + 23, Y + 28, 20, 15, Ink);
                Label(FString::FromInt(bActive ? FMath::CeilToInt(Remaining) : Index == 8 ? Pawn->GetXayahLootRemaining() : Pawn->GetJinxKillsUntilPassive()),
                    X + 23, Y + 28, 20, 15, .72f, bActive ? MetallicGold : Text);
            }
            else if (!bPassive)
            {
                if (bCoolingDown)
                {
                    Rect(X + 1, Y + 1, 42, 42, FLinearColor(.006f, .009f, .015f, .63f));
                    Label(FString::FromInt(FMath::CeilToInt(Remaining)), X, Y, 44, 44, 1.f);
                }
                Label(Slot == 2 ? TEXT("R") : TEXT("E"), X + 1, Y + 31, 14, 14, .72f);
                if (Slot == 1 && (Index == 3 || Index == 5))
                {
                    Label(FString::FromInt(Pawn->GetExtraSkillCharges()), X + 27, Y + 29, 16, 14, .77f);
                }
            }
        }
        else if (Slot < 3)
        {
            static const TCHAR* Keys[] = {TEXT("P"), TEXT("E"), TEXT("R")};
            Label(Keys[Slot], X, PanelY + 15, 44, 44, .95f);
        }
    }
    for (int32 Slot = 0; Slot < 6; ++Slot)
    {
        const float X = PanelX + 184 + Slot * 36;
        Frame(X, PanelY - 36, 33, 32, Purple);
        const FVector2D Top(X + 16.5f, PanelY - 44);
        const FVector2D Left(X + 8.5f, PanelY - 36);
        const FVector2D Right(X + 24.5f, PanelY - 36);
        const FVector2D Bottom(X + 16.5f, PanelY - 28);
        Triangle(Top, Left, Right, Purple);
        Triangle(Left, Bottom, Right, FLinearColor(.07f, .04f, .13f));
    }
    Rect(PanelX + 15, PanelY + 68, 390, 18, Ink);
    Rect(PanelX + 17, PanelY + 70, 386, 14, FLinearColor(.008f, .14f, .015f));
    const float HealthWidth = 386.f * FMath::Clamp(Pawn->GetCombatHealth() / FMath::Max(1.f, Pawn->GetMaxCombatHealth()), 0.f, 1.f);
    Rect(PanelX + 17, PanelY + 72, HealthWidth, 10, Green);
    Rect(PanelX + 17, PanelY + 72, HealthWidth, 2, FLinearColor(.22f, .65f, .16f));
    Label(FString::Printf(TEXT("%.0f / %.0f"), Pawn->GetCombatHealth(), Pawn->GetMaxCombatHealth()), PanelX + 15, PanelY + 68, 390, 18, .8f);
    Rect(PanelX + 15, PanelY + 95, 390, 12, Ink);
    Rect(PanelX + 15, PanelY + 95, 390, 2, Purple);

    if (PlayerOwner && PlayerOwner->IsInputKeyDown(EKeys::Tab))
    {
        const float X = FMath::Max(8.f, PanelX - 376.f);
        const float Y = H - 348.f;
        constexpr float Width = 270.f;
        Frame(X, Y, Width, 340.f, Gold);
        Label(TEXT("PLAYER STATS"), X, Y, Width, 38, 1.f, Gold);
        Rect(X + 14, Y + 38, Width - 28, 1, Gold);

        const FString MoveSpeed = FString::FromInt(FMath::RoundToInt(Pawn->GetCharacterMovement()->GetMaxSpeed()));
        const TCHAR* Names[] = {TEXT("Damage"), TEXT("Move speed"), TEXT("Ability haste"),
            TEXT("Crit chance"), TEXT("Health regen"), TEXT("Armor"), TEXT("Max health"),
            TEXT("Projectiles"), TEXT("Area size"), TEXT("Duration"), TEXT("Pickup radius"),
            TEXT("Experience"), TEXT("Gold gain"), TEXT("Boon duration")};
        const FString Values[] = {TEXT("100%"), MoveSpeed, Pawn->GetJinxPassiveRemaining() > 0.f ? TEXT("150")
            : Pawn->GetXayahBuffRemaining() > 0.f ? TEXT("80") : TEXT("0"),
            FString::Printf(TEXT("%.0f%%"), Pawn->GetBaseCriticalChance() * 100.f), TEXT("0"),
            FString::Printf(TEXT("%.0f"), Pawn->GetCombatArmor()), FString::Printf(TEXT("%.0f"), Pawn->GetMaxCombatHealth()),
            TEXT("0"), TEXT("100%"), TEXT("100%"), TEXT("100%"), TEXT("100%"), TEXT("100%"), TEXT("100%")};
        static_assert(UE_ARRAY_COUNT(Names) == UE_ARRAY_COUNT(Values));
        for (int32 Row = 0; Row < UE_ARRAY_COUNT(Names); ++Row)
        {
            const float RowY = Y + 46.f + 20.f * Row;
            if (Row % 2 == 0)
            {
                Rect(X + 10, RowY, Width - 20, 20, Ink);
            }
            Label(Names[Row], X + 18, RowY, 0, 20, .78f, Text, false);
            Label(Values[Row], X + 200, RowY, 50, 20, .78f, Row == 1 ? Green : Text);
        }
    }

    const float MapX = W - 290;
    const float MapY = H - 291;
    Frame(MapX - 2, MapY - 2, 286, 286, Gold);
    if (!bLoadedMinimap)
    {
        bLoadedMinimap = true;
        Minimap = LoadObject<UTexture2D>(nullptr,
            TEXT("/Game/TroubleSquad/UI/T_CoastalMinimapReference.T_CoastalMinimapReference"));
    }
    if (Minimap)
    {
        DrawTexture(Minimap, MapX * Scale, MapY * Scale, 282 * Scale, 282 * Scale, 0, 0, 1, 1);
    }

    Frame(MapX - 43, H - 45, 38, 37, Gold);
    const FVector2D Gear(MapX - 24, H - 26.5);
    const FLinearColor GearColor = bSettingsOpen ? MetallicGold : FLinearColor(.68f, .64f, .44f);
    for (int32 Tooth = 0; Tooth < 8; ++Tooth)
    {
        const double Angle = UE_PI * Tooth / 4.0;
        const FVector2D Axis(FMath::Cos(Angle), FMath::Sin(Angle));
        const FVector2D Start = Gear + 7.0 * Axis;
        const FVector2D End = Gear + 12.0 * Axis;
        DrawLine(Start.X * Scale, Start.Y * Scale, End.X * Scale, End.Y * Scale, GearColor, 4.0f * Scale);
    }
    Disk(Gear, 9.5, GearColor, nullptr, {}, {});
    Disk(Gear, 4.5, Panel, nullptr, {}, {});
    AddHitBox(FVector2D(MapX - 43, H - 45) * Scale, FVector2D(38, 37) * Scale, TEXT("Settings"), true);
    if (bSettingsOpen)
    {
        Frame(MapX - 208, H - 145, 201, 92, Gold);
        Label(TEXT("Settings"), MapX - 208, H - 145, 201, 40, 1.f);
        Label(TEXT("Toggle fullscreen"), MapX - 202, H - 101, 186, 32, .9f);
        AddHitBox(FVector2D(MapX - 202, H - 101) * Scale, FVector2D(186, 32) * Scale,
            TEXT("Fullscreen"), true, 1);
    }

    FVector2D Head;
    if (PlayerOwner && PlayerOwner->ProjectWorldLocationToScreen(Pawn->GetActorLocation() + FVector(0, 0, 125), Head, true))
    {
        const float X = Head.X / Scale - 57;
        const float Y = Head.Y / Scale - 24;
        Frame(X, Y, 115, 19, Gold);
        Label(FString::FromInt(Pawn->GetCombatLevel()), X + 2, Y, 18, 19, .7f);
        for (int32 Segment = 0; Segment < 10; ++Segment)
        {
            Rect(X + 22 + Segment * 9, Y + 5, 8, 9, Green);
        }
    }
}

void ACombatHUD::NotifyHitBoxClick(FName BoxName)
{
    Super::NotifyHitBoxClick(BoxName);
    if (BoxName == TEXT("Settings"))
    {
        bSettingsOpen = !bSettingsOpen;
    }
    else if (BoxName == TEXT("Fullscreen"))
    {
        if (UGameUserSettings* Settings = GEngine->GetGameUserSettings())
        {
            Settings->SetFullscreenMode(Settings->GetFullscreenMode() == EWindowMode::Windowed
                ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
            Settings->ApplySettings(false);
        }
    }
}
