#include "CombatHUD.h"
#include "CombatAudio.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "SwarmArenas.h"
#include "Kismet/GameplayStatics.h"
#include "GeneralWeapons.h"
#include "SwarmEncounter.h"
#include "EngineUtils.h"
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

void ACombatHUD::CaptureArenaMinimap()
{
    ArenaMinimap = NewObject<UTextureRenderTarget2D>(this);
    ArenaMinimap->RenderTargetFormat = RTF_RGBA8;
    ArenaMinimap->ClearColor = FLinearColor(.015f, .025f, .035f);
    ArenaMinimap->InitAutoFormat(512, 512);
    ArenaMinimap->UpdateResourceImmediate();

    MinimapCapture = GetWorld()->SpawnActor<ASceneCapture2D>(FVector(0.f, 0.f, 9000.f),
        FRotator(-90.f, 0.f, 0.f));
    if (!MinimapCapture) { ArenaMinimap = nullptr; return; }
    USceneCaptureComponent2D* Capture = MinimapCapture->GetCaptureComponent2D();
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    Capture->ProjectionType = ECameraProjectionMode::Orthographic;
    Capture->OrthoWidth = MinimapWorldWidth;
    Capture->TextureTarget = ArenaMinimap;
    Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    Capture->ShowFlags.SetLighting(false);
    Capture->ShowFlags.SetDynamicShadows(false);
    Capture->ShowFlags.SetBloom(false);
    Capture->ShowFlags.SetFog(false);
    Capture->ShowFlags.SetVolumetricFog(false);
    Capture->ShowFlags.SetMotionBlur(false);
    Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
    Capture->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
    Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
    Capture->PostProcessSettings.AutoExposureBias = 0.f;
    Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
    for (TActorIterator<APawn> Pawn(GetWorld()); Pawn; ++Pawn) { Capture->HideActorComponents(*Pawn); }
    Capture->CaptureScene();
}

void ACombatHUD::EndPlay(const EEndPlayReason::Type Reason)
{
    if (MinimapCapture) { MinimapCapture->Destroy(); }
    Super::EndPlay(Reason);
}

void ACombatHUD::ShowSkillRange(FVector Center, float Radius, FLinearColor Color, float Duration, float Heading, float HalfAngle)
{
    if (Radius <= 0.f || Duration <= 0.f) { return; }
    if (SkillRanges.Num() >= 24) { SkillRanges.RemoveAt(0); }
    SkillRanges.Add({Center, Radius, Color, GetWorld()->GetTimeSeconds() + Duration, Duration, Heading, HalfAngle});
}

void ACombatHUD::DrawSkillRanges(float Scale)
{
    const double Now = GetWorld()->GetTimeSeconds();
    SkillRanges.RemoveAll([Now](const FSkillRange& Ring) { return Ring.ExpiresAt <= Now; });
    if (!PlayerOwner) { return; }

    for (const FSkillRange& Ring : SkillRanges)
    {
        const float Fade = static_cast<float>((Ring.ExpiresAt - Now) / Ring.Duration);
        const FLinearColor Color = Ring.Color.CopyWithNewOpacity(.7f * Fade);
        const auto ArcPoint = [&](int32 Step)
        {
            const float Angle = FMath::DegreesToRadians(Ring.Heading - Ring.HalfAngle + 2.f * Ring.HalfAngle * Step / 40.f);
            return Ring.Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Ring.Radius;
        };
        FVector2D Origin = FVector2D::ZeroVector;
        FVector2D Previous = FVector2D::ZeroVector;
        const bool bCone = Ring.HalfAngle < 180.f;
        const bool bOriginVisible = bCone && PlayerOwner->ProjectWorldLocationToScreen(Ring.Center, Origin, true);
        bool bPreviousVisible = PlayerOwner->ProjectWorldLocationToScreen(ArcPoint(0), Previous, true);
        const float Thickness = FMath::Max(1.f, Scale * 2.f);
        if (bOriginVisible && bPreviousVisible)
        {
            DrawLine(Origin.X, Origin.Y, Previous.X, Previous.Y, Color, Thickness);
        }
        for (int32 Step = 1; Step <= 40; ++Step)
        {
            const FVector Point = ArcPoint(Step);
            FVector2D Next;
            const bool bNextVisible = PlayerOwner->ProjectWorldLocationToScreen(Point, Next, true);
            if (bPreviousVisible && bNextVisible)
            {
                DrawLine(Previous.X, Previous.Y, Next.X, Next.Y, Color, Thickness);
            }
            Previous = Next;
            bPreviousVisible = bNextVisible;
        }
        if (bOriginVisible && bPreviousVisible)
        {
            DrawLine(Previous.X, Previous.Y, Origin.X, Origin.Y, Color, Thickness);
        }
    }
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

    if (Pawn->IsCarryingArenaBattery() && !Pawn->HasFinishedDistrict())
    {
        Frame(W / 2 - 132, 61, 264, 34, Gold);
        const FVector Cannon = SwarmArenas::OutskirtsCannon;
        const int32 Distance = FMath::RoundToInt(FVector::Dist2D(Pawn->GetActorLocation(), Cannon) / 100.f);
        Label(FString::Printf(TEXT("DELIVER BATTERY  /  %d m"), Distance), W / 2 - 132, 61, 264, 34, .86f, Gold);
        FVector2D PawnScreen;
        FVector2D CannonScreen;
        const FVector BearingPoint = Pawn->GetActorLocation()
            + (Cannon - Pawn->GetActorLocation()).GetSafeNormal2D() * 200.f;
        if (PlayerOwner->ProjectWorldLocationToScreen(Pawn->GetActorLocation(), PawnScreen, true)
            && PlayerOwner->ProjectWorldLocationToScreen(BearingPoint, CannonScreen, true))
        {
            const FVector2D Direction = (CannonScreen - PawnScreen).GetSafeNormal();
            const FVector2D Side(-Direction.Y, Direction.X);
            const FVector2D Tip = PawnScreen + Direction * (78.f * Scale);
            const FVector2D Base = Tip - Direction * (15.f * Scale);
            const FVector2D Left = Base + Side * (8.f * Scale);
            const FVector2D Right = Base - Side * (8.f * Scale);
            DrawLine(Left.X, Left.Y, Tip.X, Tip.Y, Gold, 3.f * Scale);
            DrawLine(Right.X, Right.Y, Tip.X, Tip.Y, Gold, 3.f * Scale);
        }
    }

    for (TActorIterator<ASwarmEnemy> Enemy(GetWorld()); Enemy; ++Enemy)
    {
        if (Enemy->Kind != 3) { continue; }
        const float BossX = W * .5f - 225.f;
        Frame(BossX, 63, 450, 46, Gold);
        const int32 Arena = Enemy->GetArenaIndex();
        FString BossLabel = SwarmArenas::Bosses[Arena];
        if (Arena == 0)
        {
            BossLabel = Enemy->IsBurrowed() ? TEXT("REK'SAI  |  DESTROY THE TUNNEL") : TEXT("REK'SAI  |  EXPOSED");
        }
        else if (Arena == 1 && Enemy->IsSecondPhase()) { BossLabel = TEXT("BRIAR  |  FRENZY"); }
        else if (Arena == 2)
        {
            if (Enemy->IsLabChamberActive() && Pawn->GetActorLocation().X < 1750.f)
            {
                BossLabel = TEXT("BEL'VETH  |  MOVE EAST");
            }
            else if (Enemy->GetLabCoralsRemaining() > 0)
            {
                BossLabel = FString::Printf(TEXT("BEL'VETH  |  IMMUNE  /  CORALS %d"), Enemy->GetLabCoralsRemaining());
            }
            else if (Enemy->IsSecondPhase()) { BossLabel = TEXT("BEL'VETH  |  EVOLVED"); }
        }
        else if (Arena == 3)
        {
            if (Enemy->IsBeachheadTetherActive()) { BossLabel = TEXT("AATROX  |  DESTROY THE PILLAR"); }
            else if (Enemy->IsSecondPhase()) { BossLabel = TEXT("AATROX  |  PHASE 2"); }
        }
        Label(BossLabel, BossX, 65, 450, 25, .96f);
        const float Fraction = FMath::Clamp(Enemy->GetHealth() / Enemy->GetMaxHealth(), 0.f, 1.f);
        Rect(BossX + 9, 96, 432, 7, Ink);
        FLinearColor HealthColor(.57f, .08f, .22f);
        if (Enemy->GetLabCoralsRemaining() > 0) { HealthColor = FLinearColor(.62f, .24f, .87f); }
        else if (Arena == 1 && Enemy->IsSecondPhase()) { HealthColor = FLinearColor(1.f, .12f, .28f); }
        Rect(BossX + 9, 96, 432 * Fraction, 7, HealthColor);
        FVector FieldCenter;
        float FieldRadius = 0.f;
        if (Enemy->GetOutskirtsField(FieldCenter, FieldRadius))
        {
            const bool bOutside = FVector::DistSquared2D(Pawn->GetActorLocation(), FieldCenter) > FMath::Square(FieldRadius);
            const FLinearColor FieldColor = bOutside ? FLinearColor(1.f, .16f, .10f) : FLinearColor(.29f, .67f, 1.f);
            Frame(BossX, 114.f, 450.f, 34.f, Gold);
            Label(bOutside ? TEXT("OUTSIDE THE FIELD") : TEXT("STAY WITHIN THE FIELD"),
                BossX, 114.f, 450.f, 34.f, .86f, FieldColor);
            FVector Previous = Project(FieldCenter + FVector(FieldRadius, 0, 0));
            for (int32 Step = 1; Step <= 96; ++Step)
            {
                const float Angle = 2.f * UE_PI * Step / 96.f;
                const FVector Next = Project(FieldCenter + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * FieldRadius);
                if (Previous.Z > 0.f && Next.Z > 0.f)
                {
                    DrawLine(Previous.X, Previous.Y, Next.X, Next.Y, FieldColor, 2.5f * Scale);
                }
                Previous = Next;
            }
        }
        float Boundary = 0.f;
        bool bDangerEast = false;
        const double ServerTime = State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
        if (Enemy->GetArenaHazardBoundary(ServerTime, Boundary, bDangerEast))
        {
            const auto NormalizeX = [](double X) { return FMath::Clamp((X + 4800.0) / 9600.0, 0.0, 1.0); };
            const float TrackX = BossX + 10.f;
            const float BorderX = TrackX + 430.f * NormalizeX(Boundary);
            const float PlayerX = TrackX + 430.f * NormalizeX(Pawn->GetActorLocation().X);
            const FLinearColor Hazard = bDangerEast ? FLinearColor(.06f, .52f, .92f) : FLinearColor(.90f, .17f, .04f);
            Frame(BossX, 114.f, 450.f, 42.f, Gold);
            Label(bDangerEast ? TEXT("FLOOD  |  MOVE WEST") : TEXT("FIRE  |  MOVE EAST"), BossX, 117.f, 450.f, 21.f, .86f);
            Rect(TrackX, 140.f, 430.f, 7.f, Ink);
            Rect(bDangerEast ? BorderX : TrackX, 140.f,
                bDangerEast ? TrackX + 430.f - BorderX : BorderX - TrackX, 7.f, Hazard);
            Rect(PlayerX - 2.f, 138.f, 4.f, 11.f, FLinearColor::White);
        }
        break;
    }

    if (PlayerOwner)
    {
        const double ServerTime = State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
        const auto Segment = [&](const FVector& A, const FVector& B, FLinearColor Color)
        {
            FVector2D From, To;
            if (PlayerOwner->ProjectWorldLocationToScreen(A, From, true) &&
                PlayerOwner->ProjectWorldLocationToScreen(B, To, true))
            {
                DrawLine(From.X, From.Y, To.X, To.Y, Color, 2.5f * Scale);
            }
        };
        for (TActorIterator<AAnimaPreviewCharacter> Player(GetWorld()); Player; ++Player)
        {
            const auto DrawGroundCircle = [&](const FVector& Center, float Radius, FLinearColor Color)
            {
                FVector Previous = Center + FVector(Radius, 0.f, 0.f);
                for (int32 Step = 1; Step <= 40; ++Step)
                {
                    const FVector Next = Center + FRotator(0.f, Step * 9.f, 0.f).Vector() * Radius;
                    Segment(Previous, Next, Color);
                    Previous = Next;
                }
            };
            if (Player->HasTimedInvulnerability(ServerTime))
            {
                const FVector HaloCenter = Player->GetActorLocation() + FVector::UpVector * 72.f;
                DrawGroundCircle(HaloCenter, 47.f, FLinearColor(1.f, .89f, .52f, .98f));
                DrawGroundCircle(HaloCenter, 56.f, FLinearColor(1.f, .70f, .23f, .55f));
            }
            if (Player->HasSeraphineShield(ServerTime))
            {
                const FVector ShieldCenter = Player->GetActorLocation() - FVector::UpVector * 50.f;
                DrawGroundCircle(ShieldCenter, 85.f, FLinearColor(.28f, .80f, 1.f, .92f));
                DrawGroundCircle(ShieldCenter + FVector::UpVector * 12.f, 71.f, FLinearColor(.67f, .43f, 1.f, .68f));
            }
            FVector LeonaCenter;
            float LeonaRadius = 0.f;
            if (Player->GetLeonaSolarZone(ServerTime, LeonaCenter, LeonaRadius))
            {
                DrawGroundCircle(LeonaCenter, LeonaRadius, FLinearColor(1.f, .61f, .14f, .9f));
            }
            if (Player->GetLeonaEclipseZone(ServerTime, LeonaCenter, LeonaRadius))
            {
                DrawGroundCircle(LeonaCenter, LeonaRadius, FLinearColor(1.f, .82f, .38f, .85f));
            }
            FVector WallCenter, WallNormal;
            if (Player->GetWindWallBoundary(ServerTime, WallCenter, WallNormal))
            {
                WallCenter.Z -= 55.f;
                const FVector Side = FVector::CrossProduct(FVector::UpVector, WallNormal)
                    * AAnimaPreviewCharacter::WindWallHalfWidth;
                const FVector Depth = WallNormal * AAnimaPreviewCharacter::WindWallHalfDepth;
                const FLinearColor WindColor(.42f, .86f, 1.f, .22f);
                Segment(WallCenter + Depth - Side, WallCenter + Depth + Side, WindColor);
                Segment(WallCenter - Depth - Side, WallCenter - Depth + Side, WindColor);
            }
            FVector Afterimage;
            if (Player->GetRivenAfterimage(ServerTime, Afterimage))
            {
                constexpr float Radius = AAnimaPreviewCharacter::RivenAfterimageRadius;
                FVector Previous = Afterimage + FVector(Radius, 0.f, 0.f);
                for (int32 Step = 1; Step <= 32; ++Step)
                {
                    const FVector Next = Afterimage + FRotator(0.f, Step * 11.25f, 0.f).Vector() * Radius;
                    Segment(Previous, Next, FLinearColor(.37f, 1.f, .55f, .9f));
                    Previous = Next;
                }
            }
            for (const FIllaoTentacleVisual& Tentacle : Player->GetTentacleVisuals())
            {
                if (Player->HasFinishedDistrict()) { break; }
                const FVector Base = Tentacle.Position - FVector::UpVector * 55.f;

                const FLinearColor Color = Tentacle.bGiant ? FLinearColor(.66f, 1.f, .22f, .96f)
                    : FLinearColor(.12f, .95f, .70f, .86f);



                const double UntilAttack = Tentacle.NextAttackAt - ServerTime;
                if (UntilAttack > 0.0 && UntilAttack <= .55)
                {
                    DrawGroundCircle(Base, Tentacle.AttackRadius,
                        Color.CopyWithNewOpacity(static_cast<float>(.70 - .55 * UntilAttack)));
                }



            }
            FVector RealmCenter;
            if (Player->GetAuroraRealmBoundary(ServerTime, RealmCenter))
            {
                constexpr float RealmRadius = AAnimaPreviewCharacter::AuroraRealmRadius;
                FVector Previous = RealmCenter + FVector(RealmRadius, 0.f, 0.f);
                for (int32 Step = 1; Step <= 48; ++Step)
                {
                    const FVector Next = RealmCenter + FRotator(0.f, Step * 7.5f, 0.f).Vector() * RealmRadius;
                    Segment(Previous, Next, FLinearColor(.72f, .32f, 1.f, .88f));
                    Previous = Next;
                }
            }
            FVector Center;
            float Opacity;
            if (!Player->GetLabFreezePulse(ServerTime, Center, Opacity)) { continue; }
            constexpr float Radius = SwarmArenas::LabFreezeRadius;
            FVector Previous = Center + FVector(Radius, 0.f, 0.f);
            for (int32 Step = 1; Step <= 48; ++Step)
            {
                const FVector Next = Center + FRotator(0.f, Step * 7.5f, 0.f).Vector() * Radius;
                Segment(Previous, Next, FLinearColor(.18f, .85f, 1.f, Opacity));
                Previous = Next;
            }
        }
        for (TActorIterator<ASwarmEnemy> Enemy(GetWorld()); Enemy; ++Enemy)
        {
            FVector Start, End;
            float Radius = 0.f;
            bool bDash = false;
            if (!Enemy->GetAttackWarning(ServerTime, Start, End, Radius, bDash)) { continue; }
            const FLinearColor Color(1.f, .22f, .12f, .92f);
            const float Flight = Enemy->GetArtilleryFlightProgress(ServerTime);
            if (Flight >= 0.f)
            {
                const auto PointOnArc = [&](float T)
                {
                    return FMath::Lerp(Start, End, T) + FVector::UpVector * (340.f * 4.f * T * (1.f - T));
                };
                const FVector Tip = PointOnArc(Flight);
                const FVector Tail = PointOnArc(FMath::Max(0.f, Flight - .10f));
                Segment(Tail, Tip, FLinearColor(1.f, .45f, .15f, 1.f));
                Segment(Tip - FVector(0.f, 0.f, 8.f), Tip + FVector(0.f, 0.f, 8.f),
                    FLinearColor(1.f, .85f, .55f, 1.f));
            }
            if (bDash)
            {
                const FVector Side = FVector::CrossProduct(FVector::UpVector, (End - Start).GetSafeNormal2D()) * Radius;
                Segment(Start + Side, End + Side, Color);
                Segment(Start - Side, End - Side, Color);
                Segment(Start - Side, Start + Side, Color);
                Segment(End - Side, End + Side, Color);
            }
            else
            {
                FVector Previous = End + FVector(Radius, 0, 0);
                for (int32 Step = 1; Step <= 40; ++Step)
                {
                    const float Angle = 2.f * UE_PI * Step / 40.f;
                    const FVector Next = End + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
                    Segment(Previous, Next, Color);
                    Previous = Next;
                }
            }
        }
    }

    DrawSkillRanges(Scale);

    const float PanelX = W / 2 - 210;
    const float PanelY = H - 124;
    const float FrenzyRemaining = Pawn->GetBriarFrenzyRemaining();
    if (FrenzyRemaining > 0.f)
    {
        const FLinearColor FrenzyColor(.96f, .30f, .39f);
        Frame(PanelX, PanelY - 78.f, 420, 30, FrenzyColor);
        Label(FString::Printf(TEXT("FRENZY  %.1fs  |  E TO END"), FrenzyRemaining),
            PanelX, PanelY - 78.f, 420, 30, .87f, FrenzyColor);
    }
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
    Counter(W - 245, MetallicGold, Pawn->GetRunGold());
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
    GeneralIcons.SetNum(GeneralWeaponCount * 2);
    auto LoadGeneralIcon = [&](uint8 Weapon, bool bEvolved)
    {
        auto& Icon = GeneralIcons[Weapon * 2 + (bEvolved ? 1 : 0)];
        if (!Icon)
        {
            const FString Name = FString::Printf(TEXT("T_GeneralWeapon_%s%s"),
                GetGeneralWeaponInfo(Weapon).AssetSuffix, bEvolved ? TEXT("Evolved") : TEXT(""));
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
        if (Slot >= 4 && Pawn->GetGeneralWeapons().IsValidIndex(Slot - 4))
        {
            const uint8 Weapon = Pawn->GetGeneralWeapons()[Slot - 4];
            if (UTexture2D* Icon = LoadGeneralIcon(Weapon, Pawn->IsGeneralWeaponEvolved(Weapon)))
            {
                DrawTexture(Icon, (X + 2) * Scale, (PanelY + 17) * Scale, 40 * Scale, 40 * Scale, 0, 0, 1, 1);
            }
            Rect(X + 3, PanelY + 42, 16, 15, Ink);
            Label(FString::FromInt(Pawn->GetGeneralWeaponRank(Weapon)), X + 3, PanelY + 42, 16, 15, .75f);
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
    TArray<uint8> OwnedStats;
    for (uint8 Choice : {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, static_cast<int>(PickupRangeChoice), static_cast<int>(ExperienceChoice)})
    {
        if (Pawn->GetRunStatRank(Choice) > 0) { OwnedStats.Add(Choice); }
    }
    const auto DrawStatIcon = [&](uint8 Choice, float X, float Y, float Size)
    {
        const float Unit = Size / 32.f;
        const FLinearColor Color = Choice == 1 || Choice == 4 ? FLinearColor(.20f, .85f, .55f)
            : Choice == 10 ? FLinearColor(1.f, .35f, .39f)
            : Choice == ExperienceChoice ? FLinearColor(.72f, .48f, 1.f) : Silver;
        const auto Point = [&](float A, float B) { return FVector2D(X + A * Unit, Y + B * Unit); };
        const auto Stroke = [&](float A, float B, float C, float D, float Width = 3.f)
        {
            DrawLine((X + A * Unit) * Scale, (Y + B * Unit) * Scale,
                (X + C * Unit) * Scale, (Y + D * Unit) * Scale, Color, Width * Unit * Scale);
        };
        const auto Tri = [&](float A, float B, float C, float D, float E, float F)
        { Triangle(Point(A, B), Point(C, D), Point(E, F), Color); };
        if (Choice == 1 || Choice == 4)
        {
            Disk(Point(10, 11), 6 * Unit, Color, nullptr, {}, {});
            Disk(Point(22, 11), 6 * Unit, Color, nullptr, {}, {});
            Tri(4, 13, 28, 13, 16, 29);
            if (Choice == 4) { Stroke(25, 1, 25, 10); Stroke(21, 5, 29, 5); }
        }
        else if (Choice == 2)
        {
            Stroke(6, 5, 26, 5); Stroke(6, 5, 7, 20); Stroke(26, 5, 25, 20);
            Stroke(7, 20, 16, 28); Stroke(25, 20, 16, 28); Stroke(12, 11, 20, 11, 2);
        }
        else if (Choice == 3)
        {
            Tri(15, 4, 25, 10, 14, 22); Tri(14, 22, 27, 25, 25, 29);
            Stroke(7, 8, 2, 8, 2); Stroke(9, 14, 2, 14, 2); Stroke(10, 20, 4, 20, 2);
        }
        else if (Choice == 5)
        {
            Tri(13, 17, 26, 3, 25, 12); Tri(13, 17, 25, 12, 18, 22);
            Stroke(5, 28, 16, 17, 4); Stroke(8, 16, 21, 29, 3);
        }
        else if (Choice == 6)
        {
            Stroke(7, 4, 25, 4); Stroke(7, 28, 25, 28);
            Stroke(8, 6, 24, 26, 2); Stroke(24, 6, 8, 26, 2);
            Tri(12, 25, 20, 25, 16, 19);
        }
        else if (Choice == 7)
        {
            for (int32 N = 0; N < 3; ++N) { Stroke(4, 9 + 8 * N, 22, 9 + 5 * N, 2); Tri(21, 6 + 5*N, 29, 9 + 5*N, 21, 12 + 5*N); }
        }
        else if (Choice == 8)
        {
            for (int32 N = 0; N < 16; ++N)
            {
                const float A = N * 2.f * UE_PI / 16.f;
                const float B = (N+1) * 2.f * UE_PI / 16.f;
                Stroke(16 + 12 * FMath::Cos(A), 17 + 12 * FMath::Sin(A),
                    16 + 12 * FMath::Cos(B), 17 + 12 * FMath::Sin(B), 2);
            }
            Stroke(16, 17, 16, 10, 2); Stroke(16, 17, 22, 17, 2); Stroke(12, 1, 20, 1, 2);
        }
        else if (Choice == 9)
        {
            Stroke(7, 25, 25, 7, 2); Stroke(7, 7, 25, 25, 2);
            Tri(3, 3, 13, 4, 4, 13); Tri(29, 3, 19, 4, 28, 13);
            Tri(3, 29, 13, 28, 4, 19); Tri(29, 29, 19, 28, 28, 19);
        }
        else if (Choice == 10)
        {
            Tri(18, 2, 14, 13, 4, 9); Tri(14, 13, 3, 26, 27, 22); Tri(18, 2, 27, 22, 23, 12);
        }
        else if (Choice == PickupRangeChoice)
        {
            Stroke(6, 6, 6, 20, 5); Stroke(26, 6, 26, 20, 5);
            Stroke(6, 20, 12, 27, 5); Stroke(26, 20, 20, 27, 5); Stroke(12, 27, 20, 27, 5);
        }
        else if (Choice == ExperienceChoice)
        {
            Stroke(4, 18, 16, 6, 5); Stroke(16, 6, 28, 18, 5);
            Stroke(4, 28, 16, 16, 5); Stroke(16, 16, 28, 28, 5);
        }
    };
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
        if (OwnedStats.IsValidIndex(Slot))
        {
            const uint8 Choice = OwnedStats[Slot];
            DrawStatIcon(Choice, X + 5, PanelY - 32, 23);
            Rect(X + 21, PanelY - 17, 11, 12, Ink);
            Label(FString::FromInt(Pawn->GetRunStatRank(Choice)), X + 21, PanelY - 18, 11, 13, .68f, MetallicGold);
        }
    }
    Rect(PanelX + 15, PanelY + 68, 390, 18, Ink);
    Rect(PanelX + 17, PanelY + 70, 386, 14, FLinearColor(.008f, .14f, .015f));
    const float HealthWidth = 386.f * FMath::Clamp(Pawn->GetCombatHealth() / FMath::Max(1.f, Pawn->GetMaxCombatHealth()), 0.f, 1.f);
    Rect(PanelX + 17, PanelY + 72, HealthWidth, 10, Green);
    Rect(PanelX + 17, PanelY + 72, HealthWidth, 2, FLinearColor(.22f, .65f, .16f));
    Label(FString::Printf(TEXT("%.0f / %.0f"), Pawn->GetCombatHealth(), Pawn->GetMaxCombatHealth()), PanelX + 15, PanelY + 68, 390, 18, .8f);
    Rect(PanelX + 15, PanelY + 95, 390, 12, Ink);
    const float ExperienceRatio = FMath::Clamp(static_cast<float>(Pawn->GetExperience()) / Pawn->GetExperienceRequired(), 0.f, 1.f);
    Rect(PanelX + 15, PanelY + 95, 390 * ExperienceRatio, 12, Purple);

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
        const FString Values[] = {
            FString::Printf(TEXT("%.0f%%"), Pawn->GetCombatDamageMultiplier() * 100.f), MoveSpeed,
            FString::Printf(TEXT("%.0f"), 100.f * (100.f / FMath::Max(.01f, Pawn->GetCombatCooldown(100.f)) - 1.f)),
            FString::Printf(TEXT("%.0f%%"), (Pawn->GetBaseCriticalChance() + .08f * Pawn->GetRunStatRank(10)) * 100.f),
            FString::FromInt(4 * Pawn->GetRunStatRank(4)),
            FString::Printf(TEXT("%.0f"), Pawn->GetCombatArmor()), FString::Printf(TEXT("%.0f"), Pawn->GetMaxCombatHealth()),
            FString::FromInt(Pawn->GetCombatBonusProjectiles()),
            FString::Printf(TEXT("%.0f%%"), Pawn->GetCombatAreaMultiplier() * 100.f),
            FString::Printf(TEXT("%.0f%%"), Pawn->GetCombatDurationMultiplier() * 100.f),
            FString::Printf(TEXT("%.0f%%"), Pawn->GetLootPickupRadiusMultiplier() * 100.f),
            FString::Printf(TEXT("+%d%%"), 10 * Pawn->GetRunStatRank(ExperienceChoice)), TEXT("100%"), TEXT("100%")};
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
        const int32 Arena = SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this));
        static constexpr const TCHAR* MapTextures[] = {
            TEXT("T_CoastalMinimapReference"), TEXT("T_Arena_Outskirts"),
            TEXT("T_Arena_SubterraneanLab"), TEXT("T_Arena_Beachhead")
        };
        const TCHAR* TextureName = MapTextures[FMath::Clamp(Arena, 0, 3)];
        Minimap = LoadObject<UTexture2D>(nullptr,
            *FString::Printf(TEXT("/Game/TroubleSquad/UI/%s.%s"), TextureName, TextureName));
    }
    if (Minimap)
    {
        DrawTexture(Minimap, MapX * Scale, MapY * Scale, 282 * Scale, 282 * Scale, 0, 0, 1, 1);
    }

    if (!ArenaMinimap && GetWorld()->GetTimeSeconds() >= 6.f) { CaptureArenaMinimap(); }
    if (ArenaMinimap)
    {
        DrawTexture(ArenaMinimap, MapX * Scale, MapY * Scale, 282.f * Scale, 282.f * Scale,
            0.f, 0.f, 1.f, 1.f, FLinearColor::White, BLEND_Opaque);
        const auto MapMarker = [&](FVector Position, FLinearColor Color, float Radius)
        {
            const FVector2D UV(.5f + Position.Y / MinimapWorldWidth, .5f - Position.X / MinimapWorldWidth);
            if (UV.X < 0.f || UV.X > 1.f || UV.Y < 0.f || UV.Y > 1.f) { return; }
            const FVector2D Center(MapX + UV.X * 282.f, MapY + UV.Y * 282.f);
            Disk(Center, Radius + 1.5f, Ink, nullptr, {}, {});
            Disk(Center, Radius, Color, nullptr, {}, {});
        };
        const int32 Arena = SwarmArenas::Find(UGameplayStatics::GetCurrentLevelName(this));
        if (Arena == 1)
        {
            MapMarker(SwarmArenas::OutskirtsCannon, FLinearColor(.95f, .62f, .12f), 5.f);
            MapMarker(FVector(1800.f, 1850.f, 0.f), FLinearColor(.28f, .95f, .46f), 4.f);
        }
        else if (Arena == 2)
        {
            for (const FVector& Station : SwarmArenas::LabFreezeStations)
            {
                MapMarker(Station, FLinearColor(.18f, .85f, 1.f), 4.f);
            }
        }
        for (TActorIterator<ASwarmEnemy> Enemy(GetWorld()); Enemy; ++Enemy)
        {
            if (Enemy->Kind == 3 && Enemy->GetHealth() > 0.f)
            {
                MapMarker(Enemy->GetActorLocation(), FLinearColor(1.f, .16f, .24f), 5.f);
            }
        }
        for (TActorIterator<AAnimaPreviewCharacter> Player(GetWorld()); Player; ++Player)
        {
            if (!Player->GetPlayerState() || Player->GetCombatHealth() <= 0.f) { continue; }
            const FVector Position = Player->GetActorLocation();
            const FVector2D UV(.5f + Position.Y / MinimapWorldWidth, .5f - Position.X / MinimapWorldWidth);
            if (UV.X < 0.f || UV.X > 1.f || UV.Y < 0.f || UV.Y > 1.f) { continue; }
            const FVector2D Marker(MapX + UV.X * 282.f, MapY + UV.Y * 282.f);
            Disk(Marker, 5.f, Ink, nullptr, {}, {});
            Disk(Marker, 3.5f, *Player == Pawn ? FLinearColor(.95f, .85f, .36f)
                : FLinearColor(.20f, .83f, 1.f), nullptr, {}, {});
        }
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
        const float SettingsX = MapX - 260.f;
        const float SettingsY = H - 220.f;
        Frame(SettingsX, SettingsY, 253.f, 168.f, Gold);
        Label(TEXT("Settings"), SettingsX, SettingsY, 253.f, 32.f, 1.f);
        Label(TEXT("Toggle fullscreen"), SettingsX, SettingsY + 33.f, 253.f, 32.f, .85f);
        AddHitBox(FVector2D(SettingsX, SettingsY + 33.f) * Scale, FVector2D(253.f, 32.f) * Scale,
            TEXT("Fullscreen"), true, 1);
        const float Volumes[] = {CombatAudio::GetMusicVolume(), CombatAudio::GetEffectsVolume()};
        const TCHAR* Names[] = {TEXT("Music"), TEXT("Effects")};
        for (int32 Row = 0; Row < 2; ++Row)
        {
            const float Y = SettingsY + 71.f + Row * 42.f;
            Label(FString::Printf(TEXT("%s  %d%%"), Names[Row], FMath::RoundToInt(Volumes[Row] * 100.f)),
                SettingsX + 5.f, Y, 157.f, 32.f, .8f);
            for (int32 Direction = 0; Direction < 2; ++Direction)
            {
                const float X = SettingsX + 165.f + Direction * 40.f;
                Frame(X, Y, 34.f, 32.f, Gold);
                Label(Direction == 0 ? TEXT("-") : TEXT("+"), X, Y, 34.f, 32.f, 1.f);
                AddHitBox(FVector2D(X, Y) * Scale, FVector2D(34.f, 32.f) * Scale,
                    FName(*FString::Printf(TEXT("Audio%d%d"), Row, Direction)), true, 2);
            }
        }
    }

    if (Pawn->HasFinishedDistrict())
    {
        Rect(0, 0, W, H, FLinearColor(.006f, .009f, .022f, .70f));
        Frame(W * .5f - 250.f, H * .5f - 105.f, 500.f, 210.f, MetallicGold);
        Label(Pawn->HasClearedDistrict() ? TEXT("DISTRICT CLEARED") : TEXT("DEFEAT"), W * .5f - 220.f, H * .5f - 80.f, 440.f, 52.f, 1.7f, MetallicGold);
        const FVector2D Button(W * .5f - 116.f, H * .5f + 22.f);
        Frame(Button.X, Button.Y, 232.f, 48.f, MetallicGold);
        Label(TEXT("Return to lobby"), Button.X, Button.Y, 232.f, 48.f, 1.f);
        AddHitBox(Button * Scale, FVector2D(232.f, 48.f) * Scale, TEXT("DistrictResultReturn"), true, 20);
        return;
    }

    const TArray<uint8>& Choices = Pawn->GetLevelChoices();
    if (Choices.IsEmpty() && GetNetMode() != NM_Standalone && UGameplayStatics::GetGlobalTimeDilation(this) < .001f)
    {
        Frame(W * .5f - 154.f, H * .20f, 308.f, 44.f, MetallicGold);
        Label(TEXT("Waiting for teammates"), W * .5f - 154.f, H * .20f, 308.f, 44.f, 1.f);
    }
    if (!Choices.IsEmpty())
    {
        static const TCHAR* Titles[] = {TEXT("Signature weapon"), TEXT("Max health"), TEXT("Armor"),
            TEXT("Move speed"), TEXT("Health regen"), TEXT("Damage"), TEXT("Ability haste"),
            TEXT("Projectiles"), TEXT("Duration"), TEXT("Area"), TEXT("Critical chance")};
        static const TCHAR* Rewards[] = {TEXT("+1 weapon rank"), TEXT("+150 max health"), TEXT("+8 armor"),
            TEXT("+8% movement"), TEXT("+4 health / sec"), TEXT("+10% damage"), TEXT("+10 haste"),
            TEXT("Projectile upgrade"), TEXT("+12% duration"), TEXT("+11% area"), TEXT("+8% crit chance")};
        const float CardY = H * .22f;
        constexpr float CardHeight = 365.f;
        Rect(0, 0, W, H, FLinearColor(.008f, .005f, .022f, .52f));
        const int32 Seconds = FMath::CeilToInt(Pawn->GetLevelChoiceTimeRemaining());
        Frame(W * .5f - 44.f, CardY - 66.f, 88.f, 46.f, MetallicGold);
        Label(FString::FromInt(Seconds), W * .5f - 44.f, CardY - 66.f, 88.f, 46.f, 1.45f,
            Seconds <= 5 ? FLinearColor(1.f, .37f, .22f) : MetallicGold);
        float MouseX = -1.f;
        float MouseY = -1.f;
        if (PlayerOwner) { PlayerOwner->GetMousePosition(MouseX, MouseY); }
        for (int32 Slot = 0; Slot < Choices.Num(); ++Slot)
        {
            const float CardX = W * .5f - 114.f - 121.f * (Choices.Num() - 1) + 242.f * Slot;
            const uint8 Choice = Choices[Slot];
            const bool bHovered = MouseX >= CardX * Scale && MouseX <= (CardX + 228) * Scale &&
                MouseY >= CardY * Scale && MouseY <= (CardY + CardHeight) * Scale;
            const bool bEvolution = Choice == SignatureEvolutionChoice || Choice >= FirstEvolutionChoice;
            const bool bWeapon = bEvolution || Choice == 0 || (Choice >= FirstGeneralWeaponChoice && Choice < PickupRangeChoice);
            const FLinearColor Edge = bWeapon ? FLinearColor(.27f, .83f, .99f) : FLinearColor(.62f, .34f, .98f);
            if (bHovered)
            {
                Rect(CardX - 4, CardY - 4, 236, CardHeight + 8, Edge * .55f);
            }
            Frame(CardX, CardY, 228, CardHeight, bHovered ? Edge * 1.35f : Edge);
            Rect(CardX + 6, CardY + 6, 216, CardHeight - 12, FLinearColor(.032f, .014f, .095f, .98f));
            AddHitBox(FVector2D(CardX, CardY) * Scale, FVector2D(228, CardHeight) * Scale,
                FName(*FString::Printf(TEXT("LevelCard%d"), Slot)), true, 10);
            const bool bStatChoice = (Choice >= 1 && Choice <= 10) || Choice == PickupRangeChoice || Choice == ExperienceChoice;
            if (bStatChoice)
            {
                const int32 Rank = Pawn->GetRunStatRank(Choice);
                Label(Rank == 0 ? TEXT("NEW PASSIVE") : *FString::Printf(TEXT("LEVEL %d > %d"), Rank, Rank + 1),
                    CardX, CardY + 58, 228, 26, .85f, MetallicGold);
                DrawStatIcon(Choice, CardX + 78, CardY + 98, 72);
                for (int32 Step = 0; Step < 5; ++Step)
                {
                    Rect(CardX + 69 + Step * 19, CardY + 293, 14, 5,
                        Step < Rank ? MetallicGold : Step == Rank ? Edge : FLinearColor(.12f, .09f, .20f));
                }
            }
            if (Pawn->IsAccessRewardOpen())
            {
                Label(TEXT("ACCESS CARD"), CardX, CardY + 28, 228, 26, .85f, MetallicGold);
            }
            if (Choice == SignatureEvolutionChoice)
            {
                if (UTexture2D* Icon = LoadCombatIcon(4))
                {
                    DrawTexture(Icon, (CardX + 74) * Scale, (CardY + 83) * Scale, 80 * Scale, 80 * Scale,
                        0, 0, 1, 1, FLinearColor::White, BLEND_Translucent);
                }
                Label(TEXT("Signature evolution"), CardX, CardY + 191, 228, 32, 1.f);
                Label(TEXT("Evolve weapon"), CardX, CardY + 244, 228, 28, .9f, Text);
                continue;
            }
            if (Choice == RunGoldRewardChoice)
            {
                Label(TEXT("Gold reserve"), CardX, CardY + 191, 228, 32, 1.10f);
                Label(TEXT("+25 gold"), CardX, CardY + 244, 228, 28, .9f, Text);
                continue;
            }
            if (Choice == PickupRangeChoice || Choice == ExperienceChoice)
            {
                Label(Choice == PickupRangeChoice ? TEXT("Pickup radius") : TEXT("Experience"), CardX, CardY + 191, 228, 32, 1.10f);
                Label(Choice == PickupRangeChoice ? TEXT("+35% pickup radius") : TEXT("+10% experience"), CardX, CardY + 244, 228, 28, .9f, Text);
                continue;
            }
            if (Choice >= FirstGeneralWeaponChoice)
            {
                const uint8 Weapon = Choice - (bEvolution ? FirstEvolutionChoice : FirstGeneralWeaponChoice);
                if (UTexture2D* Icon = LoadGeneralIcon(Weapon, bEvolution))
                {
                    DrawTexture(Icon, (CardX + 74) * Scale, (CardY + 83) * Scale, 80 * Scale, 80 * Scale, 0, 0, 1, 1);
                }
                Label(bEvolution ? GetGeneralWeaponInfo(Weapon).EvolvedName : GetGeneralWeaponInfo(Weapon).Name, CardX, CardY + 191, 228, 26, .9f);
                const int32 Rank = Pawn->GetGeneralWeaponRank(Weapon);
                Label(bEvolution ? FString(TEXT("Evolve weapon")) : Rank == 0 ? FString(TEXT("New weapon")) : FString::Printf(TEXT("Rank %d"), Rank + 1),
                    CardX, CardY + 244, 228, 26, .85f, Text);
                const auto Before=GetGeneralWeaponStats(Weapon,FMath::Max(Rank,1),false);
                const auto After=GetGeneralWeaponStats(Weapon,bEvolution?5:FMath::Min(Rank+1,5),bEvolution);
                Label(Rank>0?FString::Printf(TEXT("Base CD %.2f > %.2fs"),Before.Cooldown,After.Cooldown)
                    :FString::Printf(TEXT("Base CD %.2fs"),After.Cooldown),CardX,CardY+280,228,22,.65f,Text);
                if(After.bUsesProjectileStat)
                {
                    auto ShotCount=[&](const FGeneralWeaponStats& S,bool Evolved)
                    {
                        int32 Count=S.Projectiles+Pawn->GetCombatBonusProjectiles();
                        const auto Type=static_cast<EGeneralWeapon>(Weapon);
                        if(Type==EGeneralWeapon::Crescent || (Type==EGeneralWeapon::Glove && Evolved)) { Count*=2; }
                        if(Type==EGeneralWeapon::Crossbow && Evolved) { Count+=30; }
                        return Count;
                    };
                    Label(Rank>0?FString::Printf(TEXT("Projectiles %d > %d"),ShotCount(Before,false),ShotCount(After,bEvolution))
                        :FString::Printf(TEXT("Projectiles %d"),ShotCount(After,bEvolution)),CardX,CardY+308,228,22,.65f,Text);
                }
                else
                { Label(FString::Printf(TEXT("%s %.0f"),After.Range>0.f?TEXT("Range"):TEXT("Radius"),After.Range>0.f?After.Range:After.Radius),CardX,CardY+308,228,22,.65f,Text); }
                continue;
            }
            Label(Titles[Choice], CardX, CardY + 191, 228, 32, 1.10f);
            if (bStatChoice)
            {
                const int32 Rank = Pawn->GetRunStatRank(Choice);
                const float Steps[] = {0.f, 150.f, 8.f, 8.f, 4.f, 10.f, 10.f, 0.f, 12.f, 11.f, 8.f};
                const int32 Before = Choice == 7 ? (Rank + 1) / 2 : FMath::RoundToInt(Steps[Choice] * Rank);
                const int32 After = Choice == 7 ? (Rank + 2) / 2 : FMath::RoundToInt(Steps[Choice] * (Rank + 1));
                const bool bPercent = Choice == 3 || Choice == 5 || Choice >= 8;
                Label((bPercent ? FString::Printf(TEXT("+%d%% > +%d%%"), Before, After) : FString::Printf(TEXT("+%d > +%d"), Before, After)),
                    CardX, CardY + 244, 228, 28, .9f, Text);
            }
            else { Label(Rewards[Choice], CardX, CardY + 244, 228, 28, .9f, Text); }
        }
    }

    // Selection cards cover world-space health labels.
    if (!Choices.IsEmpty()) { return; }

    for (TActorIterator<ASwarmEnemy> Enemy(GetWorld()); Enemy; ++Enemy)
    {
        if (Enemy->Kind != 4) { continue; }
        FVector2D Position;
        if (PlayerOwner && PlayerOwner->ProjectWorldLocationToScreen(Enemy->GetActorLocation() + FVector(0, 0, 45), Position, true))
        {
            const float X = Position.X / Scale - 45;
            const float Y = Position.Y / Scale - 14;
            Frame(X, Y, 90, 14, Gold);
            const float Fraction = FMath::Clamp(Enemy->GetHealth() / Enemy->GetMaxHealth(), 0.f, 1.f);
            Rect(X + 3, Y + 4, 84 * Fraction, 6, FLinearColor(.73f, .14f, .24f));
        }
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
    if (auto* Pawn = Cast<AAnimaPreviewCharacter>(GetOwningPawn()); Pawn && Pawn->HasFinishedDistrict())
    {
        if (BoxName == TEXT("DistrictResultReturn")) { Pawn->ReturnToSoloMenu(); }
        return;
    }
    const FString Name = BoxName.ToString();
    if (Name.StartsWith(TEXT("LevelCard")))
    {
        if (auto* Character = Cast<AAnimaPreviewCharacter>(GetOwningPawn()))
        {
            Character->SelectLevelCard(FCString::Atoi(*Name.RightChop(9)));
        }
        return;
    }
    if (BoxName.ToString().StartsWith(TEXT("Audio")))
    {
        const FString AudioBox = BoxName.ToString();
        if (AudioBox.Len() == 7 && (AudioBox[5] == '0' || AudioBox[5] == '1') && (AudioBox[6] == '0' || AudioBox[6] == '1'))
        {
            const float Change = AudioBox[6] == '0' ? -.1f : .1f;
            if (AudioBox[5] == '0') { CombatAudio::SetMusicVolume(CombatAudio::GetMusicVolume() + Change); }
            else { CombatAudio::SetEffectsVolume(CombatAudio::GetEffectsVolume() + Change); }
        }
        return;
    }
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
