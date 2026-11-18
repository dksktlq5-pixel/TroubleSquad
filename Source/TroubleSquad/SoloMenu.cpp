#include "SoloMenu.h"
#include "SteamRoomState.h"
#include "Engine/GameInstance.h"
#include "SwarmArenas.h"
#include "DistrictProgress.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Texture2D.h"
#include "MoviePlayer.h"
#include "OnlineSubsystem.h"
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "Slate/DeferredCleanupSlateBrush.h"
#include "TimerManager.h"
#include "Internationalization/Regex.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SOverlay.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateColorBrush.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SoloMenu"

namespace
{
const FText ArenaNames[] = { LOCTEXT("ArenaWarehouse", "물류 창고 지구"), LOCTEXT("ArenaOutskirts", "외곽 지대"),
    LOCTEXT("ArenaLab", "지하 세계 실험실"), LOCTEXT("ArenaBeach", "상륙지") };
const FLinearColor Panel(0.025f, 0.040f, 0.063f, 0.95f);
const FLinearColor Gold(0.62f, 0.44f, 0.21f);
const FLinearColor Muted(0.61f, 0.72f, 0.78f);
const FLinearColor White(0.90f, 0.97f, 1.f);
const FLinearColor Cyan(0.35f, 0.79f, 0.85f);

const FText CharacterNames[] = {
        LOCTEXT("Jinx", "징크스"), LOCTEXT("Seraphine", "세라핀"), LOCTEXT("Leona", "레오나"),
        LOCTEXT("Illaoi", "일라오이"), LOCTEXT("Briar", "브라이어"), LOCTEXT("Yasuo", "야스오"),
        LOCTEXT("Riven", "리븐"), LOCTEXT("Aurora", "오로라"), LOCTEXT("Xayah", "자야")
    };
const FButtonStyle& ClientButtonStyle()
{
    static const FButtonStyle Style = FButtonStyle()
        .SetNormal(FSlateColorBrush(FLinearColor(.014f,.028f,.042f)))
        .SetHovered(FSlateColorBrush(FLinearColor(.055f,.13f,.16f)))
        .SetPressed(FSlateColorBrush(FLinearColor(.08f,.22f,.25f)))
        .SetNormalPadding(FMargin(0.f)).SetPressedPadding(FMargin(0.f));
    return Style;
}

TSharedRef<STextBlock> Text(const FText& Value, int32 Size, FLinearColor Color, bool bBold = false)
{
    return SNew(STextBlock)
        .Text(Value)
        .ColorAndOpacity(Color)
        .Font(FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size));
}




const FVector2f FaceCenters[] = {{.606f,.138f},{.655f,.443f},{.501f,.197f},
    {.508f,.182f},{.612f,.350f},{.679f,.322f},{.539f,.238f},{.531f,.343f},{.812f,.239f}};

class SLobbyShade : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SLobbyShade) {}
    SLATE_END_ARGS()
    void Construct(const FArguments&) {}

    FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

    int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle&, bool) const override
    {
        const FVector2D Size = Geometry.GetLocalSize();
        const FLinearColor Clear(.005f, .01f, .025f, 0.f);
        const FLinearColor Dark(.005f, .01f, .025f, .79f);
        const FLinearColor Top(.005f, .01f, .025f, .55f);
        FSlateDrawElement::MakeGradient(Elements, Layer, Geometry.ToPaintGeometry(),
            {{FVector2D(0, 0), Dark}, {FVector2D(Size.X * .32, 0), Clear}, {FVector2D(Size.X * .66, 0), Clear}, {FVector2D(Size.X, 0), Dark}},
            Orient_Vertical);
        FSlateDrawElement::MakeGradient(Elements, Layer + 1, Geometry.ToPaintGeometry(),
            {{FVector2D(0, 0), Top}, {FVector2D(0, Size.Y * .22), Clear}, {FVector2D(0, Size.Y * .53), Clear}, {FVector2D(0, Size.Y), Dark}},
            Orient_Horizontal);
        return Layer + 1;
    }
};

TSharedRef<SWidget> CoverArt(const TSharedRef<FDeferredCleanupSlateBrush>& Art)
{
    return SNew(SBox)
        .Clipping(EWidgetClipping::ClipToBounds)
        [
            SNew(SScaleBox)
            .Stretch(EStretch::ScaleToFill)
            [
                SNew(SBox).WidthOverride(1280.f).HeightOverride(720.f)
                [SNew(SImage).Image_Lambda([Art]() { return Art->GetSlateBrush(); })]
            ]
        ];
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
            .Padding(12.f)
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

    if (GEngine)
    {
        NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ASoloMenuController::HandleConnectionFailure);
    }
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("SteamRoomSmokeTest")))
    {
        RoomCapacity = 2;
        FTimerHandle RoomSmokeTimer;
        GetWorldTimerManager().SetTimer(RoomSmokeTimer, FTimerDelegate::CreateWeakLambda(this,[this]()
        {
            RequestSteamRoom(true);
            UE_LOG(LogTemp,Display,TEXT("Steam room smoke: creation requested=%d"),SteamRoomRequest != 0);
        }),2.5f,false);
    }
#endif
    if (FParse::Param(FCommandLine::Get(), TEXT("OnlineParty"))) { RoomCapacity = 4; }
    bShowMouseCursor = true;
    GetWorldTimerManager().SetTimer(SteamRoomWatchTimer, this, &ASoloMenuController::WatchSteamRoom, .25f, true);
    ShowStartup();
#if WITH_EDITOR
    if (FParse::Param(FCommandLine::Get(), TEXT("ClientUISmokeTest")))
    {
        FTimerHandle LobbyCheck;
        GetWorldTimerManager().SetTimer(LobbyCheck, FTimerDelegate::CreateWeakLambda(this,[this](){ ShowPage(true); }),2.2f,false);
        FTimerHandle ScreenshotCheck;
        GetWorldTimerManager().SetTimer(ScreenshotCheck, FTimerDelegate::CreateWeakLambda(this,[this](){ ConsoleCommand(TEXT("Shot ShowUI")); }),3.5f,false);
    }
#endif
}

void ASoloMenuController::ShowStartup()
{
    auto* Viewport = GetWorld()->GetGameViewport();
    if (!Viewport) { ShowPage(false); return; }
    LogoTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/TroubleSquad/UI/T_TroubleSquadLogo.T_TroubleSquadLogo"));
    const auto Logo = FDeferredCleanupSlateBrush::CreateBrush(LogoTexture.Get());
    Menu = SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(.003f,.008f,.018f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [SNew(SBox).WidthOverride(660.f).HeightOverride(220.f)
                [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image_Lambda([Logo](){ return Logo->GetSlateBrush(); })]]]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f,24.f)
            [Text(LOCTEXT("StartupPreparing", "클라이언트 준비 중"),14,Muted)]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [SNew(SThrobber).NumPieces(3)]];
    Viewport->AddViewportWidgetContent(Menu.ToSharedRef(),10);
    SetInputMode(FInputModeUIOnly());
    GetWorldTimerManager().SetTimer(StartupTimer, FTimerDelegate::CreateWeakLambda(this,[this](){ ShowPage(RoomCapacity > 1); }),1.8f,false);
}

void ASoloMenuController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(StartupTimer);
    if (Menu.IsValid())
    {
        if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
        {
            Viewport->RemoveViewportWidgetContent(Menu.ToSharedRef());
        }
        Menu.Reset();
    }
    if (GEngine && NetworkFailureHandle.IsValid())
    {
        GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
    }
    GetWorldTimerManager().ClearTimer(SteamRoomWatchTimer);
    GetWorldTimerManager().ClearTimer(SteamRoomTimer);
    if (!bTraveling) { LeaveSteamRoom(); }
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

    static const TCHAR* const ArtNames[] = {
        TEXT("Jinx"), TEXT("Seraphine"), TEXT("Leona"), TEXT("Illaoi"), TEXT("Briar"),
        TEXT("Yasuo"), TEXT("Riven"), TEXT("Aurora"), TEXT("Xayah")
    };

    auto LoadArt = [](const TCHAR* Name)
    {
        const FString Asset = FString::Printf(TEXT("/Game/TroubleSquad/UI/T_Menu_%s.T_Menu_%s"), Name, Name);
        return LoadObject<UTexture2D>(nullptr, *Asset);
    };
    const TCHAR* Champion = SelectedCharacter == 0 ? TEXT("BattleCatJinx") : ArtNames[SelectedCharacter];
    PortraitTexture = LoadObject<UTexture2D>(nullptr,
        *FString::Printf(TEXT("/Game/TroubleSquad/UI/T_ChampionHD_%s.T_ChampionHD_%s"), Champion, Champion));
    DistrictTexture = SelectedArena == 0 ? LoadArt(TEXT("District"))
        : LoadArt(*(FString(TEXT("Arena_")) + FString(SwarmArenas::Names[SelectedArena]).RightChop(2)));
    if (!SplashTexture)
    {
        SplashTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/TroubleSquad/UI/T_ClientSplash.T_ClientSplash"));
    }
    static_assert(UE_ARRAY_COUNT(FaceCenters) == UE_ARRAY_COUNT(ArtNames));
    FSlateBrush LobbyArt;
    LobbyArt.DrawAs = ESlateBrushDrawType::Image;
    LobbyArt.SetResourceObject(PortraitTexture.Get());
    constexpr float CropSize = .56f;
    const FVector2f FaceCenter = FaceCenters[SelectedCharacter];
    const FVector2f CropOrigin(FMath::Clamp(FaceCenter.X - CropSize * .5f, 0.f, 1.f - CropSize),
        FMath::Clamp(FaceCenter.Y - CropSize * .32f, 0.f, 1.f - CropSize));
    LobbyArt.SetUVRegion(FBox2f(CropOrigin, CropOrigin + FVector2f(CropSize, CropSize)));
    const TSharedRef<FDeferredCleanupSlateBrush> PageArt = bLobby
        ? FDeferredCleanupSlateBrush::CreateBrush(LobbyArt)
        : FDeferredCleanupSlateBrush::CreateBrush(SplashTexture ? SplashTexture.Get() : PortraitTexture.Get());
    PortraitBrush.SetResourceObject(PortraitTexture.Get());
    DistrictBrush.SetResourceObject(DistrictTexture.Get());


    TSharedRef<SButton> Primary = SNew(SButton).ButtonStyle(&ClientButtonStyle())
        .PressMethod(EButtonPressMethod::ButtonPress)
        .HAlign(HAlign_Center)
        .ContentPadding(FMargin(20.f, 18.f))
        .ButtonColorAndOpacity(FLinearColor(0.06f, 0.38f, 0.68f))
        .IsEnabled_Lambda([this,bLobby]() { return !bLobby || (RoomCapacity == 1 && SteamRoom == 0) || CanStartSteamRoom(); })
        .OnClicked_Lambda([this, bLobby]()
        {
            if (!bLobby)
            {
                ShowPage(true);
            }
            else if (!bTraveling)
            {
                if (SteamRoom) { StartSteamMatch(); }
                else { PendingJoinAddress.Reset(); ShowReadyCheck(); }
            }
            return FReply::Handled();
        })
        [Text(bLobby ? (LOCTEXT("Start", "게임 시작")) : LOCTEXT("Solo", "플레이"), 25, White, true)];

    TSharedRef<SVerticalBox> Left = SNew(SVerticalBox);
    if (bLobby)
    {
        TSharedRef<SVerticalBox> Picker = SNew(SVerticalBox);
        Picker->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 7.f)
            [Text(CharacterNames[SelectedCharacter], 27, White, true)];
        for (int32 Row = 0; Row < 3; ++Row)
        {
            TSharedRef<SHorizontalBox> Tiles = SNew(SHorizontalBox);
            for (int32 Column = 0; Column < 3; ++Column)
            {
                const int32 Index = Row * 3 + Column;
                const TCHAR* Name = Index == 0 ? TEXT("BattleCatJinx") : ArtNames[Index];
                UTexture2D* Art = LoadObject<UTexture2D>(nullptr,
                    *FString::Printf(TEXT("/Game/TroubleSquad/UI/T_ChampionHD_%s.T_ChampionHD_%s"), Name, Name));
                const FVector2f Center = FaceCenters[Index];
                const float HalfHeight = .135f;
                const float HalfWidth = Art && Art->GetSizeX() > 0
                    ? HalfHeight * Art->GetSizeY() / Art->GetSizeX() : .08f;
                const FVector2f Extent(HalfWidth, HalfHeight);
                FSlateBrush Brush;
                Brush.DrawAs = ESlateBrushDrawType::Image;
                Brush.SetResourceObject(Art);
                Brush.SetUVRegion(FBox2f(Center - Extent, Center + Extent));
                const auto Face = FDeferredCleanupSlateBrush::CreateBrush(Brush);
                Tiles->AddSlot().AutoWidth().Padding(2.f)
                [
                    SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .BorderBackgroundColor(Index == SelectedCharacter ? Gold : Panel)
                    .Padding(2.f)
                    [
                        SNew(SButton).ButtonStyle(&ClientButtonStyle()).ContentPadding(0.f).ToolTipText(CharacterNames[Index])
                        .OnClicked_Lambda([this, Index]()
                        {
                            SelectedCharacter = Index;
                            ShowPage(true);
                            return FReply::Handled();
                        })
                        [
                            SNew(SBox).WidthOverride(68.f).HeightOverride(68.f)
                            [SNew(SImage).Image_Lambda([Face]() { return Face->GetSlateBrush(); })]
                        ]
                    ]
                ];
            }
            Picker->AddSlot().AutoHeight()[Tiles];
        }
        Left->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)[Frame(Picker)];

        Left->AddSlot().AutoHeight()
        [
            Frame(SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SBox).HeightOverride(142.f)
                    [SNew(SImage).Image(&DistrictBrush)]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.f, 9.f, 0.f, 0.f)
                [
                    SNew(SButton).ButtonStyle(&ClientButtonStyle())
                    .IsEnabled_Lambda([this]() { return SteamRoom == 0 || (SteamMatchmaking() && SteamUser() &&
                        SteamMatchmaking()->GetLobbyOwner(CSteamID(SteamRoom)) == SteamUser()->GetSteamID()); })
                    .OnClicked_Lambda([this]() { SelectedArena = (SelectedArena + 1) % UE_ARRAY_COUNT(ArenaNames); ShowPage(true); return FReply::Handled(); })
                    [Text(ArenaNames[SelectedArena], 19, White, true)]
                ])
        ];
    }
    else
    {
        Left->AddSlot().AutoHeight()
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [Text(LOCTEXT("MainTitle", "집중포화"), 43, White, true)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
            [Text(LOCTEXT("MainMode", "솔로 생존"), 18, Muted)]
        ];
    }

    TSharedRef<SVerticalBox> Right = SNew(SVerticalBox);
    if (bLobby)
    {

        Right->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
        [
            SNew(SButton).ButtonStyle(&ClientButtonStyle()).OnClicked_Lambda([this]()
            {
                ShowUpgrades();
                return FReply::Handled();
            })
            [Text(LOCTEXT("Upgrades", "업그레이드"), 21, Gold, true)]
        ];
        for (int32 Slot = 1; Slot < 4; ++Slot)
        {
            const bool bLocal = Slot == 0;
            Right->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
            [
                Frame(SNew(SBox).HeightOverride(46.f)
                [
                    SNew(SOverlay)
                    + SOverlay::Slot()
                    [
                        bLocal ? CoverArt(PageArt) : SNullWidget::NullWidget
                    ]
                    + SOverlay::Slot()
                    [
                        SNew(SBorder)
                        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                        .BorderBackgroundColor(FLinearColor(0.01f, 0.02f, 0.04f, bLocal ? 0.60f : 0.25f))
                        .Padding(FMargin(8.f, 2.f))
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()
                            [Text(bLocal ? CharacterNames[SelectedCharacter] : LOCTEXT("WaitingSlot", "빈 팀원 슬롯"), 14, White, bLocal)]
                            + SVerticalBox::Slot().AutoHeight()
                            [Text(bLocal ? LOCTEXT("LocalPartySlot", "나") : LOCTEXT("WaitingMember", "참가 대기"), 10, Muted)]
                        ]
                    ]
                ])
            ];
        }

        TSharedRef<SHorizontalBox> CapacityButtons = SNew(SHorizontalBox);
        for (int32 Count = 1; Count <= 4; ++Count)
        {
            CapacityButtons->AddSlot().FillWidth(1.f).Padding(0.f, 0.f, 3.f, 0.f)
            [
                SNew(SButton).ButtonStyle(&ClientButtonStyle())
                .HAlign(HAlign_Center)
                .ButtonColorAndOpacity(Count == RoomCapacity ? FLinearColor(0.10f, 0.44f, 0.48f) : Panel)
                .OnClicked_Lambda([this, Count]()
                {
                    LeaveSteamRoom();
                    RoomCapacity = Count;
                    ShowPage(true);
                    return FReply::Handled();
                })
                [Text(FText::AsNumber(Count), 20, White, true)]
            ];
        }


        Right->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
        [
            Frame(SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [Text(LOCTEXT("PartyCapacity", "인원 설정"), 17, Muted)]
                + SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
                [CapacityButtons])
        ];
    }


    if (bLobby)
    {
        if (RoomCapacity > 1 || SteamRoom != 0)
        {
            Right->AddSlot().AutoHeight()
            [
                Frame(SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [Text(LOCTEXT("RoomCodeTitle", "참가 코드"), 14, Gold, true)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
                    [SNew(STextBlock).Text_Lambda([this]()
                        { return RoomCode.IsEmpty() ? JoinStatus : FText::FromString(RoomCode); }).ColorAndOpacity(Cyan)]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SEditableTextBox).HintText(LOCTEXT("RoomCodeHint", "6자리 코드"))
                        .OnTextChanged_Lambda([this](const FText& Value) { EnteredCode = Value.ToString(); })
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1)
                        [SNew(SButton).ButtonStyle(&ClientButtonStyle()).OnClicked_Lambda([this]() { RequestSteamRoom(true); return FReply::Handled(); })
                            [Text(LOCTEXT("CreateSteamRoom", "방 만들기"), 13, White)]]
                        + SHorizontalBox::Slot().FillWidth(1)
                        [SNew(SButton).ButtonStyle(&ClientButtonStyle()).OnClicked_Lambda([this]() { RequestSteamRoom(false); return FReply::Handled(); })
                            [Text(LOCTEXT("JoinSteamRoom", "참가"), 13, White)]]
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(STextBlock).Text_Lambda([this]() { return GetSteamRoomRoster(); }).ColorAndOpacity(White)
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1)
                        [SNew(SButton).ButtonStyle(&ClientButtonStyle()).IsEnabled_Lambda([this]() { return SteamRoom != 0; })
                            .OnClicked_Lambda([this]()
                            {
                                bSteamReady = !bSteamReady;
                                SteamMatchmaking()->SetLobbyMemberData(CSteamID(SteamRoom), "ready", bSteamReady ? "1" : "0");
                                return FReply::Handled();
                            })
                            [SNew(STextBlock).Text_Lambda([this]() { return bSteamReady ? LOCTEXT("CancelReady", "준비 취소") : LOCTEXT("ReadyRoom", "준비"); }).ColorAndOpacity(White)]]
                        + SHorizontalBox::Slot().FillWidth(1)
                        [SNew(SButton).ButtonStyle(&ClientButtonStyle()).IsEnabled_Lambda([this]() { return SteamRoom != 0 && SteamFriends() != nullptr; })
                            .OnClicked_Lambda([this]()
                            {
                                SteamFriends()->ActivateGameOverlayInviteDialog(CSteamID(SteamRoom));
                                return FReply::Handled();
                            })
                            [Text(LOCTEXT("SteamInvite", "친구 초대"), 13, White)]]
                    ])
            ];
        }
    }

    const IOnlineSubsystem* Steam = IOnlineSubsystem::Get(FName(TEXT("STEAM")));
    const IOnlineIdentityPtr Identity = Steam ? Steam->GetIdentityInterface() : nullptr;
    const bool bSteamSignedIn = Identity.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn;
    const FText SteamProfile = bSteamSignedIn
        ? FText::FromString(Identity->GetPlayerNickname(0))
        : LOCTEXT("SteamOffline", "Steam 오프라인");

    const IOnlineFriendsPtr Friends = Steam ? Steam->GetFriendsInterface() : nullptr;
    const auto FriendDrawer = SNew(SVerticalBox);
    if (!bLobby)
    {
        const TSharedRef<SVerticalBox> FriendRows = SNew(SVerticalBox);
        const TSharedRef<SScrollBox> FriendScroll = SNew(SScrollBox);
        FriendScroll->AddSlot()[FriendRows];
        const auto ShowStatus = [FriendRows](const FText& Status)
        {
            FriendRows->ClearChildren();
            FriendRows->AddSlot().AutoHeight().Padding(0.f, 6.f)[Text(Status, 15, Muted)];
        };
        ShowStatus(LOCTEXT("FriendsOffline", "Steam 연결 후 친구 목록을 볼 수 있어요."));
        if (bSteamSignedIn && Friends.IsValid())
        {
            const FString ListName = EFriendsLists::ToString(EFriendsLists::Default);
            const TWeakPtr<SVerticalBox> WeakRows(FriendRows);
            ShowStatus(LOCTEXT("FriendsLoading", "친구 목록 불러오는 중"));
            const bool bRequested = Friends->ReadFriendsList(0, ListName,
                FOnReadFriendsListComplete::CreateLambda(
                    [WeakRows](int32 UserIndex, bool bSuccess, const FString& CompletedList, const FString&)
                    {
                        const auto Rows = WeakRows.Pin();
                        if (!Rows.IsValid())
                        {
                            return;
                        }
                        TArray<TSharedRef<FOnlineFriend>> Entries;
                        IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(FName(TEXT("STEAM")));
                        const IOnlineFriendsPtr FriendInterface = Subsystem ? Subsystem->GetFriendsInterface() : nullptr;
                        const bool bLoaded = bSuccess && FriendInterface.IsValid()
                            && FriendInterface->GetFriendsList(UserIndex, CompletedList, Entries);
                        Rows->ClearChildren();
                        if (!bLoaded || Entries.IsEmpty())
                        {
                            Rows->AddSlot().AutoHeight()[Text(bLoaded ? LOCTEXT("NoFriends", "친구가 없어요.")
                                : LOCTEXT("FriendsUnavailable", "친구 목록을 불러오지 못했어요."), 15, Muted)];
                            return;
                        }

                        Entries.StableSort([](const TSharedRef<FOnlineFriend>& A, const TSharedRef<FOnlineFriend>& B)
                        {
                            return A->GetPresence().bIsOnline && !B->GetPresence().bIsOnline;
                        });
                        for (const auto& Friend : Entries)
                        {
                            const bool bOnline = Friend->GetPresence().bIsOnline;
                            const FString Name = Friend->GetDisplayName();
                            const FLinearColor Status = bOnline ? FLinearColor(.10f, .68f, .42f) : Muted;
                            Rows->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
                            [
                                SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                                .BorderBackgroundColor(FLinearColor(.035f, .056f, .074f)).Padding(FMargin(10.f, 6.f))
                                [
                                    SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
                                    [
                                        SNew(SBox).WidthOverride(34.f).HeightOverride(34.f)
                                        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                                            .BorderBackgroundColor(FLinearColor(.08f, .15f, .19f))
                                            .HAlign(HAlign_Center).VAlign(VAlign_Center)
                                            [Text(FText::FromString(Name.Left(1)), 17, Gold, true)]]
                                    ]
                                    + SHorizontalBox::Slot().FillWidth(1.f)
                                    [
                                        SNew(SVerticalBox)
                                        + SVerticalBox::Slot().AutoHeight()[Text(FText::FromString(Name), 15, White, true)]
                                        + SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
                                        [Text(bOnline ? LOCTEXT("OnlineFriend", "온라인") : LOCTEXT("OfflineFriend", "오프라인"), 12, Status)]
                                    ]
                                ]
                            ];
                        }
                    }));
            if (!bRequested)
            {
                ShowStatus(LOCTEXT("FriendsReadFailed", "친구 목록을 불러오지 못했어요."));
            }
        }
        FriendDrawer->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 14.f)
        [Text(LOCTEXT("FriendsTitle", "친구"), 23, Gold, true)];
        FriendDrawer->AddSlot().FillHeight(1.f)[FriendScroll];
    }

    auto Tab = [this](const FText& Label, uint8 Drawer)
    {
        return SNew(SButton).ButtonStyle(&ClientButtonStyle())
            .ButtonColorAndOpacity_Lambda([this, Drawer]()
            {
                return ActiveDrawer == Drawer && !bSettingsOpen ? FLinearColor(.43f, .30f, .13f) : Panel;
            })
            .ContentPadding(FMargin(17.f, 9.f))
            .OnClicked_Lambda([this, Drawer]()
            {
                ActiveDrawer = ActiveDrawer == Drawer ? 0 : Drawer;
                return FReply::Handled();
            })
            [Text(Label, 14, White, true)];
    };
    const auto Settings = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 14.f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
            [Text(LOCTEXT("SettingsTitle", "설정"), 25, White, true)]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).ButtonStyle(&ClientButtonStyle()).ContentPadding(FMargin(12.f, 6.f))
                .OnClicked_Lambda([this]() { bSettingsOpen = false; return FReply::Handled(); })
                [Text(LOCTEXT("SettingsClose", "닫기"), 13, Gold, true)]]
        ]
        + SVerticalBox::Slot().FillHeight(1.f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 20.f, 0.f)
            [
                SNew(SBox).WidthOverride(135.f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
                    [SNew(SButton).ButtonStyle(&ClientButtonStyle()).ContentPadding(12.f)
                        .ButtonColorAndOpacity_Lambda([this]() { return !bDisplaySettings ? Gold : Panel; })
                        .OnClicked_Lambda([this]() { bDisplaySettings = false; return FReply::Handled(); })
                        [Text(LOCTEXT("GeneralSettings", "일반"), 15, White, true)]]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
                    [SNew(SButton).ButtonStyle(&ClientButtonStyle()).ContentPadding(12.f)
                        .ButtonColorAndOpacity_Lambda([this]() { return bDisplaySettings ? Gold : Panel; })
                        .OnClicked_Lambda([this]() { bDisplaySettings = true; return FReply::Handled(); })
                        [Text(LOCTEXT("DisplaySettings", "그래픽"), 15, White, true)]]
                ]
            ]
            + SHorizontalBox::Slot().FillWidth(1.f)
            [
                SNew(SOverlay)
                + SOverlay::Slot().VAlign(VAlign_Top)
                [
                    SNew(SVerticalBox)
                    .Visibility_Lambda([this]() { return bDisplaySettings ? EVisibility::Collapsed : EVisibility::Visible; })
                    + SVerticalBox::Slot().AutoHeight()[Text(LOCTEXT("ConnectionTitle", "현재 프로필"), 14, Gold, true)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f)[Text(SteamProfile, 19, White, true)]
                ]
                + SOverlay::Slot().VAlign(VAlign_Top)
                [
                    SNew(SVerticalBox)
                    .Visibility_Lambda([this]() { return bDisplaySettings ? EVisibility::Visible : EVisibility::Collapsed; })
                    + SVerticalBox::Slot().AutoHeight()[Text(LOCTEXT("WindowMode", "화면 모드"), 16, Gold, true)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f).HAlign(HAlign_Left)
                    [SNew(SButton).ButtonStyle(&ClientButtonStyle()).ContentPadding(FMargin(16.f, 8.f))
                        .OnClicked_Lambda([]()
                        {
                            if (auto* Preferences = GEngine ? GEngine->GetGameUserSettings() : nullptr)
                            {
                                Preferences->SetFullscreenMode(Preferences->GetFullscreenMode() == EWindowMode::Windowed
                                    ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
                                Preferences->ApplySettings(false);
                                Preferences->SaveSettings();
                            }
                            return FReply::Handled();
                        })
                        [SNew(STextBlock).Text_Lambda([]()
                        {
                            const auto* Preferences = GEngine ? GEngine->GetGameUserSettings() : nullptr;
                            return Preferences && Preferences->GetFullscreenMode() == EWindowMode::Windowed
                                ? LOCTEXT("Windowed", "현재 창 모드 · 전체 화면으로 전환")
                                : LOCTEXT("Fullscreen", "현재 전체 화면 · 창 모드로 전환");
                        })]]
                ]
            ]
        ];

    const UDistrictProgress* Progress = UDistrictProgress::Load();
    const auto GoalRow = [](const FText& Goal, int32 Current, int32 Target)
    {
        const bool bDone = Current >= Target;
        return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(FLinearColor(.034f, .058f, .082f)).Padding(FMargin(12.f, 10.f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[Text(Goal, 15, White, true)]
                + SVerticalBox::Slot().AutoHeight().Padding(0.f, 7.f, 0.f, 4.f)
                [Text(bDone ? LOCTEXT("GoalComplete", "완료") : FText::Format(LOCTEXT("GoalCount", "{0} / {1}"), Current, Target),
                    12, bDone ? Cyan : Muted)]
                + SVerticalBox::Slot().AutoHeight()
                [SNew(SBox).HeightOverride(7.f)
                    [SNew(SProgressBar).Percent(FMath::Min(1.f, static_cast<float>(Current) / Target)).FillColorAndOpacity(Cyan)]]
            ];
    };
    TSharedRef<SWidget> Page = SNew(SOverlay)
        + SOverlay::Slot()
        [CoverArt(PageArt)]
        + SOverlay::Slot()
        [SNew(SLobbyShade).Visibility(bLobby ? EVisibility::HitTestInvisible : EVisibility::Collapsed)]
        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(bLobby ? FLinearColor(0.004f, 0.012f, 0.023f, 0.08f)
                : FLinearColor(0.006f, 0.011f, 0.028f, 0.55f))
            .Visibility(EVisibility::HitTestInvisible)
        ]
        + SOverlay::Slot().Padding(22.f, 16.f).VAlign(VAlign_Top)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f, 0.f)
            [
                SNew(SButton).ButtonStyle(&ClientButtonStyle())
                .Visibility(bLobby ? EVisibility::Visible : EVisibility::Collapsed)
                .IsEnabled_Lambda([this]() { return SteamRoomRequest == 0; })
                .OnClicked_Lambda([this]() { LeaveSteamRoom(); ShowPage(false); return FReply::Handled(); })
                [Text(LOCTEXT("Back", "뒤로"), 13, Gold, true)]
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [Text(bLobby ? LOCTEXT("Lobby", "집중포화 · 로비") : LOCTEXT("Brand", "TROUBLE SQUAD    /    플레이"), 14, Gold, true)]
        ]
        + SOverlay::Slot().Padding(22.f, 20.f).VAlign(VAlign_Center)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [SNew(SBox).WidthOverride(280.f)[Left]]
            + SHorizontalBox::Slot().FillWidth(1.f)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [SNew(SBox).WidthOverride(276.f)[Right]]
        ]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 24.f)
        [SNew(SBox).WidthOverride(290.f)[Primary]]
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Fill).Padding(0.f, 66.f, 24.f, 24.f)
        [
            SNew(SBox).WidthOverride(310.f)
            .Visibility_Lambda([this, bLobby]() { return !bLobby && ActiveDrawer == 1 ? EVisibility::Visible : EVisibility::Collapsed; })
            [Frame(FriendDrawer)]
        ]
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.f, 66.f, 24.f, 0.f)
        [
            SNew(SBox).WidthOverride(380.f)
            .Visibility_Lambda([this]() { return ActiveDrawer == 2 ? EVisibility::Visible : EVisibility::Collapsed; })
            [Frame(SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[Text(LOCTEXT("GoalsTitle", "목표"), 22, Gold, true)]
                + SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 4.f)
                [Text(LOCTEXT("DistrictGoal", "해안 연구소"), 16, White, true)]
                + SVerticalBox::Slot().AutoHeight()
                [Text(LOCTEXT("DistrictGoalDescription", "전투에서 달성한 기록"), 13, Muted)]
                + SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 6.f)
                [GoalRow(LOCTEXT("LevelGoal", "한 판에서 레벨 10 달성"), Progress->HighestLevel, 10)]
                + SVerticalBox::Slot().AutoHeight()
                [SNew(SBox).MaxDesiredHeight(290.f)
                    [SNew(SScrollBox)
                        + SScrollBox::Slot().Padding(0.f, 0.f, 0.f, 5.f)
                        [GoalRow(LOCTEXT("GoalReksai", "물류 창고 지구 · 렉사이"), Progress->HasClearedArena(0) ? 1 : 0, 1)]
                        + SScrollBox::Slot().Padding(0.f, 0.f, 0.f, 5.f)
                        [GoalRow(LOCTEXT("GoalBriar", "외곽 지대 · 브라이어"), Progress->HasClearedArena(1) ? 1 : 0, 1)]
                        + SScrollBox::Slot().Padding(0.f, 0.f, 0.f, 5.f)
                        [GoalRow(LOCTEXT("GoalBelveth", "지하 세계 실험실 · 벨베스"), Progress->HasClearedArena(2) ? 1 : 0, 1)]
                        + SScrollBox::Slot()
                        [GoalRow(LOCTEXT("GoalAatrox", "상륙지 · 아트록스"), Progress->HasClearedArena(3) ? 1 : 0, 1)]]])]
        ]
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.f, 14.f, 24.f, 0.f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
            [Text(SteamProfile, 15, bSteamSignedIn ? White : Muted, true)]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
            [Text(FText::Format(LOCTEXT("GoldBalance", "골드 {0}"), FText::AsNumber(Progress->GoldBalance)), 15, Gold, true)]
            + SHorizontalBox::Slot().AutoWidth().Padding(3.f, 0.f)[Tab(LOCTEXT("GoalsTab", "목표"), 2)]
            + SHorizontalBox::Slot().AutoWidth().Padding(3.f, 0.f)
            [SNew(SBox).Visibility(bLobby ? EVisibility::Collapsed : EVisibility::Visible)
                [Tab(LOCTEXT("FriendsTab", "친구"), 1)]]
            + SHorizontalBox::Slot().AutoWidth().Padding(3.f, 0.f)
            [SNew(SButton).ButtonStyle(&ClientButtonStyle()).ButtonColorAndOpacity(Panel).ContentPadding(FMargin(17.f, 9.f))
                .OnClicked_Lambda([this]() { bSettingsOpen = true; return FReply::Handled(); })
                [Text(LOCTEXT("SettingsTab", "설정"), 14, White, true)]]
        ]
        + SOverlay::Slot()
        [
            SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(FLinearColor(.002f, .005f, .012f, .72f))
            .HAlign(HAlign_Center).VAlign(VAlign_Center)
            .Visibility_Lambda([this]() { return bSettingsOpen ? EVisibility::Visible : EVisibility::Collapsed; })
            [
                SNew(SBox).WidthOverride(690.f).HeightOverride(430.f)
                [Frame(Settings)]
            ]
        ];



    Menu = SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
        [SNew(SBox).WidthOverride(1280.f).HeightOverride(720.f)[Page]];
    Viewport->AddViewportWidgetContent(Menu.ToSharedRef(), 10);
    FInputModeUIOnly InputMode;
    if (Primary->IsEnabled())
    {
        InputMode.SetWidgetToFocus(Primary);
    }
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
}


void ASoloMenuController::RequestSteamRoom(bool bCreate)
{
    if (SteamRoomRequest || SteamRoom || bTraveling) { return; }
    ISteamMatchmaking* Matchmaking = SteamMatchmaking();
    if (!Matchmaking || !SteamUtils() || !SteamUser() || !SteamUser()->BLoggedOn())
    {
        JoinStatus = LOCTEXT("SteamUnavailable", "Steam 연결을 확인하세요.");
        return;
    }

    if (bCreate && (RoomCapacity < 2 || RoomCapacity > 4)) { JoinStatus = LOCTEXT("NeedPartySize", "인원을 2~4명으로 설정하세요."); return; }
    SteamRoomRequestAt = FPlatformTime::Seconds();
    if (bCreate)
    {
        SteamRoomRequest = Matchmaking->CreateLobby(k_ELobbyTypePublic, RoomCapacity);
        SteamRoomRequestType = 1;
    }
    else
    {
        const FString Code = EnteredCode.TrimStartAndEnd();
        if (Code.Len() != 6 || !FRegexMatcher(FRegexPattern(TEXT("^[0-9]{6}$")), Code).FindNext())
        {
            JoinStatus = LOCTEXT("WrongRoomCode", "숫자 6자리를 입력하세요.");
            return;
        }
        Matchmaking->AddRequestLobbyListStringFilter("app_namespace", "TroubleSquad-roomcode-v2", k_ELobbyComparisonEqual);
        Matchmaking->AddRequestLobbyListStringFilter("room_code", TCHAR_TO_UTF8(*Code), k_ELobbyComparisonEqual);
        Matchmaking->AddRequestLobbyListDistanceFilter(k_ELobbyDistanceFilterWorldwide);
        SteamRoomRequest = Matchmaking->RequestLobbyList();
        SteamRoomRequestType = 2;
    }
    JoinStatus = SteamRoomRequest ? LOCTEXT("RoomWorking", "Steam 로비 연결 중")
        : LOCTEXT("RoomFailed", "Steam 로비 연결 실패");
    if (SteamRoomRequest)
    {
        GetWorldTimerManager().SetTimer(SteamRoomTimer, this, &ASoloMenuController::PollSteamRoom, .2f, true);
    }
}

void ASoloMenuController::PollSteamRoom()
{
    ISteamUtils* Utils = SteamUtils();
    ISteamMatchmaking* Matchmaking = SteamMatchmaking();
    if (!SteamRoomRequest || !Utils || !Matchmaking)
    {
        GetWorldTimerManager().ClearTimer(SteamRoomTimer);
        SteamRoomRequest = 0;
        JoinStatus = LOCTEXT("RoomInterrupted", "Steam 연결이 끊겼어요.");
        return;
    }
    bool bFailed = false;
    if (!Utils->IsAPICallCompleted(SteamRoomRequest, &bFailed))
    {
        if (FPlatformTime::Seconds() - SteamRoomRequestAt > 20.0)
        {
            SteamRoomRequest = 0;
            GetWorldTimerManager().ClearTimer(SteamRoomTimer);
            JoinStatus = LOCTEXT("RoomTimeout", "요청 시간이 초과됐습니다. 다시 시도하세요.");
        }
        return;
    }
    const uint64 Call = SteamRoomRequest;
    SteamRoomRequest = 0;
    JoinStatus = LOCTEXT("RoomNoResult", "로비에 연결할 수 없어요.");
    if (!bFailed && SteamRoomRequestType == 1)
    {
        LobbyCreated_t Result{};
        if (Utils->GetAPICallResult(Call, &Result, sizeof(Result), LobbyCreated_t::k_iCallback, &bFailed)
            && !bFailed && Result.m_eResult == k_EResultOK)
        {
            SteamRoom = Result.m_ulSteamIDLobby;
            RoomCode = FString::Printf(TEXT("%06d"), FMath::RandRange(0, 999999));
            const CSteamID Lobby(SteamRoom);
            if (!Matchmaking->SetLobbyData(Lobby, "app_namespace", "TroubleSquad-roomcode-v2")
                || !Matchmaking->SetLobbyData(Lobby, "room_code", TCHAR_TO_UTF8(*RoomCode))
                || !Matchmaking->SetLobbyData(Lobby, "state", "waiting")
                || !Matchmaking->SetLobbyData(Lobby, "map", TCHAR_TO_UTF8(*FString::FromInt(SelectedArena))))
            {
                LeaveSteamRoom();
            }
        }
    }
    else if (!bFailed && SteamRoomRequestType == 2)
    {
        LobbyMatchList_t Result{};
        if (Utils->GetAPICallResult(Call, &Result, sizeof(Result), LobbyMatchList_t::k_iCallback, &bFailed)
            && !bFailed && Result.m_nLobbiesMatching == 1)
        {
            SteamRoomRequest = Matchmaking->JoinLobby(Matchmaking->GetLobbyByIndex(0));
            SteamRoomRequestType = 3;
            SteamRoomRequestAt = FPlatformTime::Seconds();
            if (SteamRoomRequest)
            {
                return;
            }
        }
    }
    else if (!bFailed && SteamRoomRequestType == 3)
    {
        LobbyEnter_t Result{};
        if (Utils->GetAPICallResult(Call, &Result, sizeof(Result), LobbyEnter_t::k_iCallback, &bFailed)
            && !bFailed && Result.m_EChatRoomEnterResponse == k_EChatRoomEnterResponseSuccess)
        {
            SteamRoom = Result.m_ulSteamIDLobby;
            RoomCode = UTF8_TO_TCHAR(Matchmaking->GetLobbyData(CSteamID(SteamRoom), "room_code"));
            RoomCapacity = Matchmaking->GetLobbyMemberLimit(CSteamID(SteamRoom));
        }
    }
#if !UE_BUILD_SHIPPING
    if (SteamRoomRequestType == 4)
    {
        LobbyMatchList_t Found{};
        const bool bFound = !bFailed && Utils->GetAPICallResult(Call,&Found,sizeof(Found),LobbyMatchList_t::k_iCallback,&bFailed)
            && !bFailed && Found.m_nLobbiesMatching == 1 && Matchmaking->GetLobbyByIndex(0).ConvertToUint64() == SteamRoom;
        GetWorldTimerManager().ClearTimer(SteamRoomTimer);
        UE_LOG(LogTemp,Display,TEXT("Steam room smoke: codeLookup=%d"),bFound);
        if (bFound)
        {
            // Host-only diagnostic: separately verifies the actual SteamSockets listen path.
            Matchmaking->SetLobbyData(CSteamID(SteamRoom),"state","starting");
            GetGameInstance()->GetSubsystem<USteamRoomState>()->StartHost();
            BeginSoloTravel();
        }
        return;
    }
#endif
    GetWorldTimerManager().ClearTimer(SteamRoomTimer);
    if (SteamRoom)
    {
        SteamRoomOwner = Matchmaking->GetLobbyOwner(CSteamID(SteamRoom)).ConvertToUint64();
        GetGameInstance()->GetSubsystem<USteamRoomState>()->Adopt(SteamRoom, SteamRoomOwner);
        Matchmaking->SetLobbyMemberData(CSteamID(SteamRoom), "ready", "0");
        PublishedCharacter = INDEX_NONE;
        JoinStatus = LOCTEXT("RoomJoined", "참가 완료 · 준비 후 방장이 시작합니다.");
        ShowPage(true);
#if !UE_BUILD_SHIPPING
        if (FParse::Param(FCommandLine::Get(), TEXT("SteamRoomSmokeTest")))
        {
            UE_LOG(LogTemp,Display,TEXT("Steam room smoke: created=%d codeLength=%d owner=%d gatedBeforeSecondPlayer=%d"),
                SteamRoom != 0,RoomCode.Len(),SteamRoomOwner == SteamUser()->GetSteamID().ConvertToUint64(),!CanStartSteamRoom());
            FTimerHandle LookupDelay;
            GetWorldTimerManager().SetTimer(LookupDelay, FTimerDelegate::CreateWeakLambda(this, [this]() {
            ISteamMatchmaking* Matchmaking = SteamMatchmaking();
            if (!Matchmaking || !SteamRoom) { return; }
            Matchmaking->AddRequestLobbyListStringFilter("app_namespace","TroubleSquad-roomcode-v2",k_ELobbyComparisonEqual);
            Matchmaking->AddRequestLobbyListStringFilter("room_code",TCHAR_TO_UTF8(*RoomCode),k_ELobbyComparisonEqual);
            Matchmaking->AddRequestLobbyListDistanceFilter(k_ELobbyDistanceFilterWorldwide);
            SteamRoomRequest = Matchmaking->RequestLobbyList();
            SteamRoomRequestType = 4;
            SteamRoomRequestAt = FPlatformTime::Seconds();
            if (SteamRoomRequest) { GetWorldTimerManager().SetTimer(SteamRoomTimer,this,&ASoloMenuController::PollSteamRoom,.2f,true); }
            }), 3.f, false);
        }
#endif
    }
}

void ASoloMenuController::LeaveSteamRoom()
{
    GetWorldTimerManager().ClearTimer(SteamRoomTimer);
    SteamRoomRequest = 0;
    if (GetGameInstance()) { GetGameInstance()->GetSubsystem<USteamRoomState>()->Leave(); }
    SteamRoom = SteamRoomOwner = 0;
    SteamHostWaitAt = 0.0;
    PublishedCharacter = INDEX_NONE;
    RoomCode.Reset();
    bSteamReady = false;
}

bool ASoloMenuController::CanStartSteamRoom() const
{
    auto* MM = SteamMatchmaking();
    if (!SteamRoom || !MM || !SteamUser() || !SteamUser()->BLoggedOn() || bTraveling || SteamRoomRequest) { return false; }
    const CSteamID Lobby(SteamRoom);
    if (MM->GetLobbyOwner(Lobby) != SteamUser()->GetSteamID()) { return false; }
    const int32 Count = MM->GetNumLobbyMembers(Lobby);
    if (Count < 2 || Count > RoomCapacity) { return false; }
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const CSteamID Member = MM->GetLobbyMemberByIndex(Lobby,Index);
        if (Member != SteamUser()->GetSteamID() && FCStringAnsi::Strcmp(MM->GetLobbyMemberData(Lobby,Member,"ready"),"1") != 0) { return false; }
    }
    return FCStringAnsi::Strcmp(MM->GetLobbyData(Lobby,"state"),"waiting") == 0;
}

void ASoloMenuController::StartSteamMatch()
{
    if (!CanStartSteamRoom()) { return; }
    auto* MM = SteamMatchmaking();
    const CSteamID Lobby(SteamRoom);
    if (!MM->SetLobbyData(Lobby,"map",TCHAR_TO_UTF8(*FString::FromInt(SelectedArena)))
        || !MM->SetLobbyData(Lobby,"state","starting"))
    {
        JoinStatus = LOCTEXT("RoomStartFailed", "게임 시작 정보를 전송하지 못했습니다.");
        return;
    }
    MM->SetLobbyJoinable(Lobby,false);
    GetGameInstance()->GetSubsystem<USteamRoomState>()->StartHost();
    PendingJoinAddress.Reset();
    BeginSoloTravel();
}

void ASoloMenuController::WatchSteamRoom()
{
    auto* MM = SteamMatchmaking();
    if (!SteamRoom || !MM || !SteamUser() || bTraveling) { return; }
    const CSteamID Lobby(SteamRoom);
    const uint64 LobbyOwnerId = MM->GetLobbyOwner(Lobby).ConvertToUint64();
    if (!LobbyOwnerId || LobbyOwnerId != SteamRoomOwner || MM->GetNumLobbyMembers(Lobby) == 0)
    {
        LeaveSteamRoom();
        JoinStatus = LOCTEXT("RoomOwnerLeft", "방장이 나갔습니다. 새 방에 참가하세요.");
        ShowPage(true);
        return;
    }
    const bool bOwner = LobbyOwnerId == SteamUser()->GetSteamID().ConvertToUint64();
    if (PublishedCharacter != SelectedCharacter)
    {
        MM->SetLobbyMemberData(Lobby,"character",TCHAR_TO_UTF8(*FString::FromInt(SelectedCharacter)));
        MM->SetLobbyMemberData(Lobby,"ready","0");
        PublishedCharacter = SelectedCharacter;
        bSteamReady = false;
    }
    const FString State = UTF8_TO_TCHAR(MM->GetLobbyData(Lobby,"state"));
    const FString MapText = UTF8_TO_TCHAR(MM->GetLobbyData(Lobby,"map"));
    if (bOwner && State == TEXT("waiting"))
    {
        if (MapText != FString::FromInt(SelectedArena)) { MM->SetLobbyData(Lobby,"map",TCHAR_TO_UTF8(*FString::FromInt(SelectedArena))); }
    }
    else if (!bOwner && State == TEXT("waiting"))
    {
        int32 Map = INDEX_NONE;
        if (LexTryParseString(Map,*MapText) && Map >= 0 && Map < UE_ARRAY_COUNT(SwarmArenas::Names) && Map != SelectedArena)
        {
            SelectedArena = Map; bSteamReady = false;
            MM->SetLobbyMemberData(Lobby,"ready","0");
            ShowPage(true);
        }
    }
    if (!bOwner && (State == TEXT("starting") || State == TEXT("ready")))
    {
        if (SteamHostWaitAt == 0.0) { SteamHostWaitAt = FPlatformTime::Seconds(); }
        if (State == TEXT("ready"))
        {
            const FString HostId = UTF8_TO_TCHAR(MM->GetLobbyData(Lobby,"host_id"));
            int32 Map = INDEX_NONE;
            if (HostId == FString::Printf(TEXT("%llu"), LobbyOwnerId) && LexTryParseString(Map,*MapText)
                && Map >= 0 && Map < UE_ARRAY_COUNT(SwarmArenas::Names))
            {
                SelectedArena = Map;
                PendingJoinAddress = FString::Printf(TEXT("steam.%llu:7777"),LobbyOwnerId);
                UE_LOG(LogTemp,Display,TEXT("Room-code participant connecting over SteamSockets"));
                BeginSoloTravel();
                return;
            }
        }
        if (FPlatformTime::Seconds() - SteamHostWaitAt > 50.0)
        {
            LeaveSteamRoom(); JoinStatus = LOCTEXT("HostTimeout", "방장의 게임 시작을 확인하지 못했습니다. 다시 참가하세요."); ShowPage(true);
        }
    }
    else if (State == TEXT("failed"))
    {
        LeaveSteamRoom(); JoinStatus = LOCTEXT("HostListenerFailed", "방장이 게임 서버를 열지 못했습니다."); ShowPage(true);
    }
}

FText ASoloMenuController::GetSteamRoomRoster() const
{
    ISteamMatchmaking* Matchmaking = SteamMatchmaking();
    if (!SteamRoom || !Matchmaking || !SteamFriends())
    {
        return FText::GetEmpty();
    }
    FString Roster;
    const CSteamID Lobby(SteamRoom);
    const int32 Count = Matchmaking->GetNumLobbyMembers(Lobby);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const CSteamID Member = Matchmaking->GetLobbyMemberByIndex(Lobby, Index);
        const bool bReady = FCStringAnsi::Strcmp(Matchmaking->GetLobbyMemberData(Lobby, Member, "ready"), "1") == 0;
        const FText Status = bReady ? LOCTEXT("MemberReady", "준비 완료") : LOCTEXT("MemberWaiting", "준비 대기");
        Roster += FString::Printf(TEXT("%s%s  %s"), Index == 0 ? TEXT("") : TEXT("\n"),
            UTF8_TO_TCHAR(SteamFriends()->GetFriendPersonaName(Member)), *Status.ToString());
    }
    return FText::FromString(Roster);
}

void ASoloMenuController::ShowUpgrades()
{
    UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    if (!Viewport || !Menu.IsValid())
    {
        return;
    }

    struct FUpgrade
    {
        const TCHAR* Name;
        const TCHAR* Increase;
        const TCHAR* Unlock;
    };
    static const FUpgrade Upgrades[] = {
        {TEXT("피해량"), TEXT("+10%  ·  8단계"), TEXT("")},
        {TEXT("방어력"), TEXT("+5  ·  5단계"), TEXT("")},
        {TEXT("최대 체력"), TEXT("+10%  ·  5단계"), TEXT("최대 체력 1,750 달성")},
        {TEXT("체력 재생"), TEXT("초당 +3  ·  5단계"), TEXT("체력 팩 3개 획득")},
        {TEXT("이동 속도"), TEXT("+5%  ·  4단계"), TEXT("")},
        {TEXT("획득 반경"), TEXT("+25%  ·  3단계"), TEXT("경험치 구슬 누적 10,000개 획득")},
        {TEXT("범위 크기"), TEXT("+5%  ·  4단계"), TEXT("폭탄 누적 10회 획득")},
        {TEXT("지속시간"), TEXT("+5%  ·  4단계"), TEXT("동물 슈퍼카 탑승")},
        {TEXT("치명타 확률"), TEXT("+5%p  ·  4단계"), TEXT("치명타 누적 40,000회 적중")},
        {TEXT("스킬 가속"), TEXT("+5  ·  4단계"), TEXT("챔피언 스킬 누적 200회 사용")},
        {TEXT("경험치"), TEXT("+5%  ·  5단계"), TEXT("30레벨 도달")},
        {TEXT("투사체"), TEXT("+1  ·  2단계"), TEXT("세라핀으로 25레벨 도달")},
        {TEXT("골드 획득"), TEXT("+15%  ·  3단계"), TEXT("누적 5,000골드 획득")},
        {TEXT("혜택 지속시간"), TEXT("+25%  ·  2단계"), TEXT("한 게임에서 전투 토끼 혜택 4회 획득")},
        {TEXT("동물 능력"), TEXT("피해·속도·반경 +1%, 가속 +1  ·  100단계"), TEXT("어려움 상륙지에서 아트록스 처치")}
    };
    const auto* Stored = UDistrictProgress::Load();
    const auto Balance = MakeShared<int32>(Stored->GoldBalance);
    const auto Ranks = MakeShared<TArray<int32>>(Stored->UpgradeRanks);
    Ranks->SetNum(UE_ARRAY_COUNT(Upgrades));
    TSharedRef<SScrollBox> Scroll = SNew(SScrollBox);
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Upgrades); Index += 4)
    {
        TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
        for (int32 Column = 0; Column < 4; ++Column)
        {
            const int32 Entry = Index + Column;
            auto Slot = Row->AddSlot();
            Slot.FillWidth(1.f).Padding(3.f);
            if (Entry >= UE_ARRAY_COUNT(Upgrades))
            {
                Slot[SNullWidget::NullWidget];
                continue;
            }
            const FUpgrade& Upgrade = Upgrades[Entry];
            Slot[Frame(SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[Text(FText::FromString(Upgrade.Name), 16, White, true)]
                + SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[Text(FText::FromString(Upgrade.Increase), 11, Cyan)]
                + SVerticalBox::Slot().AutoHeight()[Text(Upgrade.Unlock[0] ? FText::FromString(Upgrade.Unlock)
                    : LOCTEXT("DefaultUnlocked", "기본 해금"), 11, Muted)]
                + SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f)
                [
                    SNew(SButton).ButtonStyle(&ClientButtonStyle())
                    .Visibility(UDistrictProgress::UpgradeCap(Entry) > 0 ? EVisibility::Visible : EVisibility::Collapsed)
                    .IsEnabled_Lambda([Entry, Balance, Ranks, bUnlocked = Stored->IsUpgradeUnlocked(Entry)]()
                    {
                        if (!bUnlocked) { return false; }
                        const int32 Rank = (*Ranks)[Entry];
                        return Rank < UDistrictProgress::UpgradeCap(Entry) && *Balance >= UDistrictProgress::UpgradePrice(Rank);
                    })
                    .OnClicked_Lambda([Entry, Balance, Ranks]()
                    {
                        if (UDistrictProgress::PurchaseUpgrade(Entry))
                        {
                            const auto* Updated = UDistrictProgress::Load();
                            *Balance = Updated->GoldBalance;
                            (*Ranks)[Entry] = Updated->GetUpgradeRank(Entry);
                        }
                        return FReply::Handled();
                    })
                    [SNew(STextBlock).Text_Lambda([Entry, Ranks]()
                    {
                        const int32 Rank = (*Ranks)[Entry];
                        return Rank >= UDistrictProgress::UpgradeCap(Entry) ? LOCTEXT("UpgradeMaxRank", "최대 단계")
                            : FText::Format(LOCTEXT("PurchaseRank", "{0}단계 구매 · {1} 골드"), Rank + 1, UDistrictProgress::UpgradePrice(Rank));
                    })]
                ])];
        }
        Scroll->AddSlot()[Row];
    }

    TSharedRef<SWidget> Lobby = Menu.ToSharedRef();
    Viewport->RemoveViewportWidgetContent(Lobby);
    Menu = SNew(SOverlay)
        + SOverlay::Slot()[Lobby]
        + SOverlay::Slot()
        [
            SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(FLinearColor(0.004f, 0.011f, 0.020f, 0.91f))
            .HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(20.f)
            [
                SNew(SBox).MaxDesiredWidth(900.f).MaxDesiredHeight(520.f)
                [
                    Frame(SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().FillWidth(1.f)[Text(LOCTEXT("UpgradeTitle", "기본 스탯 업그레이드"), 24, Gold, true)]
                            + SHorizontalBox::Slot().AutoWidth()
                            [SNew(SButton).ButtonStyle(&ClientButtonStyle()).OnClicked_Lambda([this]() { ShowPage(true); return FReply::Handled(); })
                                [Text(LOCTEXT("UpgradeClose", "닫기"), 14, White)]]
                        ]
                        + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).ColorAndOpacity(Gold).Text_Lambda([Balance]()
                        {
                            return FText::Format(LOCTEXT("UpgradeBalance", "보유 골드 {0}"), FText::AsNumber(*Balance));
                        })]
                        + SVerticalBox::Slot().FillHeight(1.f)[Scroll])
                ]
            ]
        ];
    Viewport->AddViewportWidgetContent(Menu.ToSharedRef(), 10);
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(Scroll);
    SetInputMode(InputMode);
}

void ASoloMenuController::ShowReadyCheck()
{
    UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    if (!Viewport || !Menu.IsValid() || bTraveling)
    {
        return;
    }

    TSharedRef<SWidget> Lobby = Menu.ToSharedRef();
    Viewport->RemoveViewportWidgetContent(Lobby);

    TSharedRef<SButton> Accept = SNew(SButton).ButtonStyle(&ClientButtonStyle())
        .HAlign(HAlign_Center)
        .ButtonColorAndOpacity(FLinearColor(0.08f, 0.46f, 0.51f))
        .ContentPadding(FMargin(20.f, 11.f))
        .OnClicked_Lambda([this]()
        {
            BeginSoloTravel();
            return FReply::Handled();
        })
        [Text(LOCTEXT("ReadyAccept", "입장 수락"), 17, White, true)];

    Menu = SNew(SOverlay)
        + SOverlay::Slot()[Lobby]
        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(FLinearColor(0.004f, 0.011f, 0.020f, 0.86f))
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            [
                SNew(SBox).WidthOverride(370.f)
                [
                    Frame(SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 21.f, 0.f, 12.f)
                        [Text(PendingJoinAddress.IsEmpty() ? FText::Format(LOCTEXT("ReadyMode", "집중포화 · 최대 {0}인"), FText::AsNumber(RoomCapacity)) : LOCTEXT("ReadyJoinMode", "집중포화 · 방 참가"), 14, Cyan, true)]
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                        [Text(LOCTEXT("ReadyTitle", "입장 준비 완료"), 30, White, true)]
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 11.f, 0.f, 26.f)
                        [Text(CharacterNames[SelectedCharacter], 17, Muted)]
                        + SVerticalBox::Slot().AutoHeight()[Accept]
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 10.f, 0.f, 0.f)
                        [
                            SNew(SButton).ButtonStyle(&ClientButtonStyle())
                            .OnClicked_Lambda([this]() { ShowPage(true); return FReply::Handled(); })
                            [Text(LOCTEXT("ReadyCancel", "취소"), 13, Muted)]
                        ])
                ]
            ]
        ];
    Viewport->AddViewportWidgetContent(Menu.ToSharedRef(), 10);
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(Accept);
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
}

void ASoloMenuController::BeginSoloTravel()
{
    UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    if (!Viewport || bTraveling)
    {
        return;
    }

    bTraveling = true;
    if (Menu.IsValid())
    {
        Viewport->RemoveViewportWidgetContent(Menu.ToSharedRef());
    }

    const TSharedRef<FDeferredCleanupSlateBrush> Art =
        FDeferredCleanupSlateBrush::CreateBrush(PortraitBrush);
    const TSharedRef<FDeferredCleanupSlateBrush> LoadingArt =
        FDeferredCleanupSlateBrush::CreateBrush(SplashTexture ? SplashTexture.Get() : PortraitTexture.Get());
    const TSharedRef<SWidget> Loading =
        SNew(SOverlay)
        + SOverlay::Slot()[CoverArt(LoadingArt)]
        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(FLinearColor(0.004f, 0.010f, 0.024f, 0.66f))
        ]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f,38.f,0.f,0.f)
        [SNew(SBox).WidthOverride(340.f).HeightOverride(110.f)
            [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
                [SNew(SImage).Image_Lambda([Logo = FDeferredCleanupSlateBrush::CreateBrush(LogoTexture.Get())](){ return Logo->GetSlateBrush(); })]]]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
        [
            SNew(SBox).WidthOverride(340.f)
            [
                Frame(SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 8.f, 0.f, 11.f)
                    [Text(LOCTEXT("LoadingMode", "집중포화"), 13, Gold, true)]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SBox).HeightOverride(210.f)
                        [
                            SNew(SImage)
                            .Image_Lambda([Art]() { return Art->GetSlateBrush(); })
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 12.f, 0.f, 4.f)
                    [Text(CharacterNames[SelectedCharacter], 25, White, true)]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                    [Text(ArenaNames[SelectedArena], 15, Muted)]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 14.f, 0.f, 5.f)
                    [SNew(SThrobber).NumPieces(3)]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 2.f, 0.f, 6.f)
                    [Text(LOCTEXT("LoadingMap", "전장 불러오는 중"), 13, Cyan, true)])
            ]
        ];

    Menu = Loading;
    Viewport->AddViewportWidgetContent(Loading, 10);
    bShowMouseCursor = false;

    GetWorldTimerManager().SetTimer(TravelTimer, FTimerDelegate::CreateWeakLambda(this, [this, Loading]()
    {
        const bool bMoviePlayer = IsMoviePlayerEnabled() && !GIsEditor;
        if (bMoviePlayer)
        {
            FLoadingScreenAttributes Attributes;
            Attributes.WidgetLoadingScreen = Loading;
            Attributes.MinimumLoadingScreenDisplayTime = 0.8f;
            Attributes.bMoviesAreSkippable = false;
            GetMoviePlayer()->SetupLoadingScreen(Attributes);
        }
        UE_LOG(LogTemp, Display, TEXT("Solo loading screen active (Character=%d, MoviePlayer=%s)"),
            SelectedCharacter, bMoviePlayer ? TEXT("yes") : TEXT("no"));
        if (!PendingJoinAddress.IsEmpty())
        {
            ClientTravel(PendingJoinAddress + FString::Printf(TEXT("?Character=%d"), SelectedCharacter), TRAVEL_Absolute);
        }
        else
        {
            const TCHAR* Listen = RoomCapacity > 1 ? TEXT("listen?") : TEXT("");
            UGameplayStatics::OpenLevel(this, FName(*FString::Printf(TEXT("/Game/TroubleSquad/Maps/%s"), SwarmArenas::Names[SelectedArena])), true,
                FString::Printf(TEXT("%sPlayers=%d?Character=%d"), Listen, RoomCapacity, SelectedCharacter));
        }
    }), 0.45f, false);
}

void ASoloMenuController::HandleConnectionFailure(UWorld* World, UNetDriver* Driver,
    ENetworkFailure::Type Failure, const FString& Error)
{
    if (!bTraveling || PendingJoinAddress.IsEmpty() || (World && World != GetWorld()))
    {
        return;
    }

    bTraveling = false;
    bShowMouseCursor = true;
    PendingJoinAddress.Reset();
    if (IsMoviePlayerEnabled() && !GIsEditor)
    {
        GetMoviePlayer()->StopMovie();
        GetMoviePlayer()->SetupLoadingScreen(FLoadingScreenAttributes());
    }
    JoinStatus = FText::Format(LOCTEXT("JoinFailed", "방에 연결하지 못했습니다. {0}"), FText::FromString(Error));
    ShowPage(true);
}

#undef LOCTEXT_NAMESPACE
