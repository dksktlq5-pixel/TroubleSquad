#include "SoloMenu.h"

#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SOverlay.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SoloMenu"

namespace
{
const FName DistrictMap(TEXT("/Game/TroubleSquad/Maps/L_CoastalResearch"));
const FLinearColor Background(0.014f, 0.024f, 0.046f);
const FLinearColor Panel(0.025f, 0.040f, 0.063f, 0.95f);
const FLinearColor Gold(0.62f, 0.44f, 0.21f);
const FLinearColor Muted(0.61f, 0.72f, 0.78f);
const FLinearColor White(0.90f, 0.97f, 1.f);

TSharedRef<STextBlock> Text(const FText& Value, int32 Size, FLinearColor Color, bool bBold = false)
{
    return SNew(STextBlock)
        .Text(Value)
        .ColorAndOpacity(Color)
        .Font(FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size));
}

TSharedRef<SWidget> Frame(const TSharedRef<SWidget>& Body)
{
    const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("WhiteBrush");
    return SNew(SBorder)
        .BorderImage(Brush)
        .BorderBackgroundColor(Gold)
        .Padding(1.f)
        [
            SNew(SBorder)
            .BorderImage(Brush)
            .BorderBackgroundColor(Panel)
            .Padding(14.f)
            [Body]
        ];
}

}

ASoloMenuMode::ASoloMenuMode()
{
    PlayerControllerClass = ASoloMenuController::StaticClass();
    DefaultPawnClass = nullptr;
}

void ASoloMenuController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController())
    {
        return;
    }

    bShowMouseCursor = true;
    ShowPage(false);
}

void ASoloMenuController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (Menu.IsValid())
    {
        if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
        {
            Viewport->RemoveViewportWidgetContent(Menu.ToSharedRef());
        }
        Menu.Reset();
    }
    Super::EndPlay(EndPlayReason);
}

void ASoloMenuController::ShowPage(bool bLobby)
{
    UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    if (!Viewport)
    {
        return;
    }

    if (Menu.IsValid())
    {
        Viewport->RemoveViewportWidgetContent(Menu.ToSharedRef());
    }

    static const FText Names[] = {
        LOCTEXT("Jinx", "징크스"), LOCTEXT("Seraphine", "세라핀"), LOCTEXT("Leona", "레오나"),
        LOCTEXT("Illaoi", "일라오이"), LOCTEXT("Briar", "브라이어"), LOCTEXT("Yasuo", "야스오"),
        LOCTEXT("Riven", "리븐"), LOCTEXT("Aurora", "오로라"), LOCTEXT("Xayah", "자야")
    };
    static const TCHAR* const ArtNames[] = {
        TEXT("Jinx"), TEXT("Seraphine"), TEXT("Leona"), TEXT("Illaoi"), TEXT("Briar"),
        TEXT("Yasuo"), TEXT("Riven"), TEXT("Aurora"), TEXT("Xayah")
    };

    auto LoadArt = [](const TCHAR* Name)
    {
        const FString Asset = FString::Printf(TEXT("/Game/TroubleSquad/UI/T_Menu_%s.T_Menu_%s"), Name, Name);
        return LoadObject<UTexture2D>(nullptr, *Asset);
    };
    PortraitTexture = LoadArt(ArtNames[SelectedCharacter]);
    DistrictTexture = LoadArt(TEXT("District"));
    PortraitBrush.SetResourceObject(PortraitTexture.Get());
    DistrictBrush.SetResourceObject(DistrictTexture.Get());

    const auto ChangeCharacter = [this](int32 Step)
    {
        SelectedCharacter = (SelectedCharacter + Step + UE_ARRAY_COUNT(ArtNames)) % UE_ARRAY_COUNT(ArtNames);
        ShowPage(true);
        return FReply::Handled();
    };

    TSharedRef<SButton> Primary = SNew(SButton)
        .PressMethod(EButtonPressMethod::ButtonPress)
        .HAlign(HAlign_Center)
        .ContentPadding(FMargin(14.f, 13.f))
        .ButtonColorAndOpacity(FLinearColor(0.47f, 0.28f, 0.085f))
        .OnClicked_Lambda([this, bLobby]()
        {
            if (!bLobby)
            {
                ShowPage(true);
            }
            else if (!bTraveling)
            {
                bTraveling = true;
                UGameplayStatics::OpenLevel(this, DistrictMap, true,
                    FString::Printf(TEXT("Character=%d"), SelectedCharacter));
            }
            return FReply::Handled();
        })
        [Text(bLobby ? LOCTEXT("Start", "전투 시작") : LOCTEXT("Solo", "솔로 플레이"), 19, White, true)];

    TSharedRef<SVerticalBox> Left = SNew(SVerticalBox);
    if (bLobby)
    {
        Left->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
        [
            Frame(SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
                [Text(LOCTEXT("Character", "캐릭터"), 12, Muted)]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth()
                    [SNew(SButton).OnClicked_Lambda([ChangeCharacter]() { return ChangeCharacter(-1); })[Text(FText::FromString(TEXT("<")), 16, Gold)]]
                    + SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Center).VAlign(VAlign_Center)
                    [Text(Names[SelectedCharacter], 20, White, true)]
                    + SHorizontalBox::Slot().AutoWidth()
                    [SNew(SButton).OnClicked_Lambda([ChangeCharacter]() { return ChangeCharacter(1); })[Text(FText::FromString(TEXT(">")), 16, Gold)]]
                ])
        ];
        Left->AddSlot().AutoHeight()
        [
            Frame(SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SBox).HeightOverride(86.f)
                    [SNew(SImage).Image(&DistrictBrush)]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.f, 9.f, 0.f, 0.f)
                [Text(LOCTEXT("District", "물류 창고 지구"), 17, White, true)])
        ];
    }
    else
    {
        Left->AddSlot().AutoHeight()
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [Text(LOCTEXT("MainTitle", "집중포화"), 43, White, true)]
        ];
    }

    TSharedRef<SVerticalBox> Right = SNew(SVerticalBox);
    if (bLobby)
    {
        Right->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
        [
            Frame(SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [Text(LOCTEXT("Party", "파티"), 12, Muted)]
                + SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f, 0.f, 0.f)
                [Text(LOCTEXT("SoloSlot", "솔로 · 1인"), 17, White, true)])
        ];
    }
    Right->AddSlot().AutoHeight()[Primary];

    Menu = SNew(SOverlay)
        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(Background)
            [
                SNew(SScaleBox)
                .Stretch(EStretch::ScaleToFit)
                [
                    SNew(SBox).WidthOverride(1280.f).HeightOverride(720.f)
                    [SNew(SImage).Image(&PortraitBrush)]
                ]
            ]
        ]
        + SOverlay::Slot().Padding(22.f, 16.f).VAlign(VAlign_Top)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f, 0.f)
            [
                SNew(SButton)
                .Visibility(bLobby ? EVisibility::Visible : EVisibility::Collapsed)
                .OnClicked_Lambda([this]() { ShowPage(false); return FReply::Handled(); })
                [Text(LOCTEXT("Back", "뒤로"), 13, Gold, true)]
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [Text(bLobby ? LOCTEXT("Lobby", "집중포화 · 솔로 로비") : LOCTEXT("Brand", "TROUBLE SQUAD"), 14, Gold, true)]
        ]
        + SOverlay::Slot().Padding(22.f, 20.f).VAlign(VAlign_Center)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [SNew(SBox).WidthOverride(220.f)[Left]]
            + SHorizontalBox::Slot().FillWidth(1.f)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [SNew(SBox).WidthOverride(208.f)[Right]]
        ];


    Viewport->AddViewportWidgetContent(Menu.ToSharedRef(), 10);
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(Primary);
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
}

#undef LOCTEXT_NAMESPACE
