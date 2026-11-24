#include "ClientPreferences.h"
#include "AnimaPreview.h"
#include "TimerManager.h"
#include "Containers/Ticker.h"
#include "MoviePlayer.h"
#include "RenderingThread.h"
#include "HAL/FileManager.h"
#include "Engine/World.h"
#include "UnrealClient.h"
#include "CombatAudio.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ConfigCacheIni.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
namespace ClientPreferences
{
static TSharedPtr<SWidget> Overlay;
static bool Combat=false;
static TWeakObjectPtr<APlayerController> OptionsOwner;
static const TCHAR* Section=TEXT("TroubleSquad.Display");
static FString DisplayPath()
{
#if WITH_EDITOR
 if(FParse::Param(FCommandLine::Get(),TEXT("SwarmSettingsAcceptanceTest"))) { return FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Tests/SettingsSmoke/Display.ini")); }
#endif
 return FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Config/Display.ini"));
}
static FConfigFile& DisplaySettings()
{
 static FConfigFile File;static bool Loaded=false;
 if(!Loaded){File.Read(DisplayPath());File.bCanSaveAllSections=true;Loaded=true;}
 return File;
}

static FIntPoint Resolutions[]={FIntPoint(1280,720),FIntPoint(1600,900),FIntPoint(1920,1080),FIntPoint(2560,1440)};
static void Close() { if(GEngine&&GEngine->GameViewport&&Overlay) { GEngine->GameViewport->RemoveViewportWidgetContent(Overlay.ToSharedRef()); } Overlay.Reset(); FSlateApplication::Get().SetAllUserFocusToGameViewport(); }
bool IsOpen(){return Overlay.IsValid();}
bool DamageNumbers(){bool Enabled=true;DisplaySettings().GetBool(Section,TEXT("DamageNumbers"),Enabled);return Enabled;}
static uint32 DisplayGeneration=0;
static void ApplyWorldResolution(bool bCombat)
{
 if(auto* S=GEngine?GEngine->GetGameUserSettings():nullptr) {
  FIntPoint Size=bCombat?FIntPoint(1920,1080):FIntPoint(1280,720); int32 X=Size.X,Y=Size.Y,Mode=2;
  DisplaySettings().GetInt(Section,bCombat?TEXT("CombatWidth"):TEXT("LobbyWidth"),X);
  DisplaySettings().GetInt(Section,bCombat?TEXT("CombatHeight"):TEXT("LobbyHeight"),Y);
  if(bCombat) { DisplaySettings().GetInt(Section,TEXT("CombatWindowMode"),Mode); }
  const auto WindowMode=static_cast<EWindowMode::Type>(FMath::Clamp(Mode,0,2));
  if(S->GetScreenResolution()==FIntPoint(X,Y)&&S->GetFullscreenMode()==WindowMode) { return; }
  FlushRenderingCommands(); S->SetFullscreenMode(WindowMode); S->SetScreenResolution(FIntPoint(X,Y)); S->ApplyResolutionSettings(false);
  UE_LOG(LogTemp,Display,TEXT("Safe display applied: combat=%d width=%d height=%d loading=0"),bCombat,X,Y);
 }
}
void EnterWorld(bool bCombat,bool bForce)
{
 Combat=bCombat;
 int32 Diagnostic=0;
 if(!bForce && FParse::Value(FCommandLine::Get(),TEXT("SwarmSkillVisual="),Diagnostic)) { return; }
 if(!GEngine||!GEngine->GameViewport||IsRunningCommandlet()||FParse::Param(FCommandLine::Get(),TEXT("nullrhi"))||FParse::Param(FCommandLine::Get(),TEXT("SwarmSkillVisual"))) { return; }
 const uint32 Generation=++DisplayGeneration;
 if(bForce) { ApplyWorldResolution(bCombat);return; }
 FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Generation,bCombat,Stable=false](float) mutable {
  if(Generation!=DisplayGeneration) { return false; }
  if(IsMoviePlayerEnabled()&&GetMoviePlayer()->IsMovieCurrentlyPlaying()) { Stable=false;return true; }
  if(!Stable) { Stable=true;return true; }
  ApplyWorldResolution(bCombat);return false;
 }),.15f);
}
class SOptions final:public SCompoundWidget
{
 int Tab=0,Resolution=0,Mode=0,Fps=1,Textures=2,Effects=2,Shadows=2,View=2,AA=2;
 bool ConfirmLeave=false;
 bool Vsync=false,Numbers=true;float Music=.5f,Sound=.5f;
 TSharedPtr<SVerticalBox> Body;
 FSlateColor Gold=FLinearColor(.76f,.62f,.34f),White=FLinearColor(.86f,.9f,.89f);
 TSharedRef<STextBlock> Text(const FString& S,int Size=18){return SNew(STextBlock).Text(FText::FromString(S)).ColorAndOpacity(White).Font(FCoreStyle::GetDefaultFontStyle("Regular",Size));}
 TSharedRef<SButton> Button(const FString& Caption,TFunction<void()> Action){return SNew(SButton).ContentPadding(FMargin(16,9)).ButtonColorAndOpacity(FLinearColor(.09f,.19f,.20f)).OnClicked_Lambda([Action](){Action();return FReply::Handled();})[Text(Caption)];}
 void Read(){auto* S=GEngine->GetGameUserSettings();for(int I=0;I<4;++I){if(Resolutions[I]==S->GetScreenResolution()){Resolution=I;}}
 Mode=(int)S->GetFullscreenMode();Textures=S->GetTextureQuality();Effects=S->GetVisualEffectQuality();Shadows=S->GetShadowQuality();View=S->GetViewDistanceQuality();AA=S->GetAntiAliasingQuality();Vsync=S->IsVSyncEnabled();Numbers=DamageNumbers();Music=CombatAudio::GetMusicVolume();Sound=CombatAudio::GetEffectsVolume();float Limit=S->GetFrameRateLimit();Fps=Limit==0?4:Limit<=60?0:Limit<=120?1:Limit<=144?2:3;}
 void Apply(){auto* S=GEngine->GetGameUserSettings();S->SetScreenResolution(Resolutions[Resolution]);S->SetFullscreenMode((EWindowMode::Type)Mode);S->SetTextureQuality(Textures);S->SetVisualEffectQuality(Effects);S->SetShadowQuality(Shadows);S->SetViewDistanceQuality(View);S->SetAntiAliasingQuality(AA);S->SetVSyncEnabled(Vsync);const float Limits[]={60,120,144,240,0};S->SetFrameRateLimit(Limits[Fps]);S->ApplySettings(false);S->SaveSettings();DisplaySettings().SetString(Section,Combat?TEXT("CombatWidth"):TEXT("LobbyWidth"),*FString::FromInt(Resolutions[Resolution].X));DisplaySettings().SetString(Section,Combat?TEXT("CombatHeight"):TEXT("LobbyHeight"),*FString::FromInt(Resolutions[Resolution].Y));if(Combat){DisplaySettings().SetString(Section,TEXT("CombatWindowMode"),*FString::FromInt(Mode));}
DisplaySettings().SetBool(Section,TEXT("DamageNumbers"),Numbers);DisplaySettings().Write(DisplayPath(),false);CombatAudio::SetMusicVolume(Music);CombatAudio::SetEffectsVolume(Sound);}
 void Row(const FString& Name,const FString& Value,TFunction<void()> Action){Body->AddSlot().AutoHeight().Padding(0,6)[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(Name)]+SHorizontalBox::Slot().FillWidth(1)[Button(Value,[this,Action](){Action();Rebuild();})]];}
 void Rebuild(){Body->ClearChildren();const TCHAR* Quality[]={TEXT("낮음"),TEXT("보통"),TEXT("높음"),TEXT("매우 높음"),TEXT("최상")};
 if(Tab==0){Row(TEXT("해상도"),FString::Printf(TEXT("%d × %d"),Resolutions[Resolution].X,Resolutions[Resolution].Y),[this](){Resolution=(Resolution+1)%4;});const TCHAR* Modes[]={TEXT("전체 화면"),TEXT("테두리 없음"),TEXT("창 모드")};Row(TEXT("화면 모드"),Modes[FMath::Clamp(Mode,0,2)],[this](){Mode=(Mode+1)%3;});const TCHAR* FpsNames[]={TEXT("60 FPS"),TEXT("120 FPS"),TEXT("144 FPS"),TEXT("240 FPS"),TEXT("제한 없음")};Row(TEXT("최대 프레임"),FpsNames[Fps],[this](){Fps=(Fps+1)%5;});Row(TEXT("수직 동기화"),Vsync?TEXT("켜짐"):TEXT("꺼짐"),[this](){Vsync=!Vsync;});Row(TEXT("텍스처 품질"),Quality[FMath::Clamp(Textures,0,4)],[this](){Textures=(Textures+1)%5;});Row(TEXT("효과 품질"),Quality[FMath::Clamp(Effects,0,4)],[this](){Effects=(Effects+1)%5;});Row(TEXT("그림자 품질"),Quality[FMath::Clamp(Shadows,0,4)],[this](){Shadows=(Shadows+1)%5;});Row(TEXT("표시 거리"),Quality[FMath::Clamp(View,0,4)],[this](){View=(View+1)%5;});Row(TEXT("안티 앨리어싱"),Quality[FMath::Clamp(AA,0,4)],[this](){AA=(AA+1)%5;});}
 else if(Tab==1){Row(TEXT("배경 음악"),FString::Printf(TEXT("%d%%  (+10 / 순환)"),FMath::RoundToInt(Music*100)),[this](){Music=Music>=.99f?0:FMath::Min(1.f,Music+.1f);});Row(TEXT("효과음"),FString::Printf(TEXT("%d%%  (+10 / 순환)"),FMath::RoundToInt(Sound*100)),[this](){Sound=Sound>=.99f?0:FMath::Min(1.f,Sound+.1f);});}
 else if(Tab==2){Row(TEXT("몬스터 피해 숫자"),Numbers?TEXT("표시"):TEXT("숨김"),[this](){Numbers=!Numbers;});Body->AddSlot().AutoHeight().Padding(0,20)[Text(TEXT("명중 숫자는 서버에서 확정된 실제 피해를 표시합니다."),16)];}
 else {const TCHAR* Lines[]={TEXT("W A S D   이동"),TEXT("마우스   조준 / 기본 무기 자동 공격"),TEXT("E   일반 스킬 / R   궁극기 (6레벨 해금)"),TEXT("1 · 2 · 3   성장 카드 선택 / Tab   능력치"),TEXT("Esc   환경설정 열기 · 닫기"),TEXT("마우스 휠   카메라 거리")};for(auto L:Lines){Body->AddSlot().AutoHeight().Padding(0,12)[Text(L)];}
 if(Combat){Body->AddSlot().AutoHeight().Padding(0,18)[Button(ConfirmLeave?TEXT("전투를 종료하고 로비로 돌아가기 · 확인"):TEXT("로비로 돌아가기"),[this](){if(!ConfirmLeave){ConfirmLeave=true;Rebuild();return;}auto* Pawn=OptionsOwner.IsValid()?Cast<AAnimaPreviewCharacter>(OptionsOwner->GetPawn()):nullptr;Close();if(Pawn){Pawn->ReturnToSoloMenu();}})];}}
 }
public:
#if WITH_EDITOR
 void AcceptanceApply(){Resolution=1;Mode=2;Fps=0;Textures=1;Effects=1;Shadows=1;View=1;AA=1;Vsync=false;Numbers=false;Music=.3f;Sound=.4f;Apply();}
 void AcceptanceCancel(){Resolution=3;Numbers=true;Read();}
 bool AcceptancePendingRestored()const{return Resolution==1&&!Numbers;}
#endif
 SLATE_BEGIN_ARGS(SOptions){} SLATE_END_ARGS()
 void Construct(const FArguments& Args){Read();auto Tabs=SNew(SVerticalBox);const TCHAR* Names[]={TEXT("화면"),TEXT("음향"),TEXT("인터페이스"),TEXT("조작 안내")};for(int I=0;I<4;++I){Tabs->AddSlot().AutoHeight().Padding(0,5)[Button(Names[I],[this,I](){Tab=I;Rebuild();})];}
 ChildSlot[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.01f,.045f,.05f,.98f)).Padding(24)[SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight().Padding(0,0,0,20)[SNew(SHorizontalBox)+SHorizontalBox::Slot()[Text(TEXT("환경설정"),30)]+SHorizontalBox::Slot().AutoWidth()[Button(TEXT("닫기 ×"),[](){Close();})]]+SVerticalBox::Slot().FillHeight(1)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().Padding(0,0,28,0)[SNew(SBox).WidthOverride(170)[Tabs]]+SHorizontalBox::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(Body,SVerticalBox)]]]+SVerticalBox::Slot().AutoHeight().Padding(0,18,0,0)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth()[Button(TEXT("기본값 복원"),[this](){Resolution=Combat?2:0;Mode=2;Fps=1;Textures=Effects=Shadows=View=AA=2;Vsync=false;Numbers=true;Music=Sound=.5f;Rebuild();})]+SHorizontalBox::Slot().AutoWidth().Padding(8,0)[Button(Combat?TEXT("로비로 나가기"):TEXT("조작 안내"),[this](){Tab=3;ConfirmLeave=Combat;Rebuild();})]+SHorizontalBox::Slot().FillWidth(1)[SNullWidget::NullWidget]+SHorizontalBox::Slot().AutoWidth().Padding(8,0)[Button(TEXT("저장 · 적용"),[this](){Apply();Close();})]+SHorizontalBox::Slot().AutoWidth()[Button(TEXT("취소"),[](){Close();})]]]];Rebuild();}
 bool SupportsKeyboardFocus() const override{return true;}
 FReply OnKeyDown(const FGeometry& G,const FKeyEvent& E) override {if(E.GetKey()==EKeys::Escape){Close();return FReply::Handled();}return SCompoundWidget::OnKeyDown(G,E);}
};
void Toggle(APlayerController* Owner){OptionsOwner=Owner;if(IsOpen()){Close();return;}if(!GEngine||!GEngine->GameViewport){return;}TSharedRef<SOptions> Options=SNew(SOptions);Overlay=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0,0,0,.65f)).Padding(30)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(960).HeightOverride(700)[Options]]];GEngine->GameViewport->AddViewportWidgetContent(Overlay.ToSharedRef(),1000);FSlateApplication::Get().SetKeyboardFocus(Options);}
#if WITH_EDITOR
void RunAcceptanceTest(UWorld* World)
{
 const FString Original=GGameUserSettingsIni;
 GGameUserSettingsIni=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Tests/SettingsSmoke/GameUserSettings.ini"));
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(GGameUserSettingsIni),true);
 // Deterministic profile; only the test-owned file is reset.
 DisplaySettings().Empty();
 FConfigFile Seed;Seed.bCanSaveAllSections=true;Seed.SetBool(TEXT("SectionsToSave"),TEXT("bCanSaveAllSections"),true);Seed.Write(GGameUserSettingsIni,false);GConfig->LoadFile(GGameUserSettingsIni);
 if(auto* File=GConfig->FindConfigFile(GGameUserSettingsIni)){File->bCanSaveAllSections=true;}
 EnterWorld(false,true);
 FTimerHandle LobbyTimer;
 World->GetTimerManager().SetTimer(LobbyTimer,[World,Original]()
 {
  const bool Lobby=GEngine->GameViewport->Viewport->GetSizeXY()==FIntPoint(1280,720);
  EnterWorld(true,true);
  FTimerHandle CombatTimer;
  World->GetTimerManager().SetTimer(CombatTimer,[World,Original,Lobby]()
  {
   const bool Fight=GEngine->GameViewport->Viewport->GetSizeXY()==FIntPoint(1920,1080);
   auto Options=SNew(SOptions);Options->AcceptanceApply();Options->AcceptanceCancel();
   const bool Cancel=Options->AcceptancePendingRestored();
   FTimerHandle AppliedTimer;
   World->GetTimerManager().SetTimer(AppliedTimer,[World,Original,Lobby,Fight,Cancel]()
   {
    auto* S=GEngine->GetGameUserSettings();S->LoadSettings(true);
    FConfigFile Disk;Disk.Read(DisplayPath());int32 X=0,Y=0;bool Numbers=true;
    const bool Stored=Disk.GetInt(Section,TEXT("CombatWidth"),X)&&Disk.GetInt(Section,TEXT("CombatHeight"),Y)&&Disk.GetBool(Section,TEXT("DamageNumbers"),Numbers)&&X==1600&&Y==900&&!Numbers;
    const bool Quality=S->GetTextureQuality()==1&&S->GetVisualEffectQuality()==1&&S->GetShadowQuality()==1&&FMath::IsNearlyEqual(S->GetFrameRateLimit(),60.f)&&!S->IsVSyncEnabled();
    const bool Applied=GEngine->GameViewport->Viewport->GetSizeXY()==FIntPoint(1600,900);
    FConfigFile Audio;Audio.Read(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Tests/SettingsSmoke/Audio.ini")));float Music=0,Effects=0;Audio.GetFloat(TEXT("TroubleSquad.Audio"),TEXT("Music"),Music);Audio.GetFloat(TEXT("TroubleSquad.Audio"),TEXT("Effects"),Effects);
    const bool Sound=FMath::IsNearlyEqual(Music,.3f)&&FMath::IsNearlyEqual(Effects,.4f);
    UE_LOG(LogTemp,Display,TEXT("Settings acceptance: lobby=%d combat=%d resolution=%d saved=%d quality=%d audio=%d cancel=%d"),Lobby,Fight,Applied,Stored,Quality,Sound,Cancel);
    GGameUserSettingsIni=Original;S->LoadSettings(true);
   },.5f,false);
  },.5f,false);
 },.5f,false);
}
#endif

}
