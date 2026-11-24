#include "CombatAudio.h"
#include "AnimaPreview.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundConcurrency.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
FString AudioPreferencesPath()
{
#if WITH_EDITOR
 if(FParse::Param(FCommandLine::Get(),TEXT("SwarmSettingsAcceptanceTest"))) { return FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Tests/SettingsSmoke/Audio.ini")); }
#endif
 return FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Config/Audio.ini"));
}
FConfigFile& AudioSettings()
{
    static FConfigFile Settings;
    static bool bLoaded = false;
    if (!bLoaded)
    {
        Settings.Read(AudioPreferencesPath());
        Settings.bCanSaveAllSections = true;
        bLoaded = true;
    }
    return Settings;
}

float ReadVolume(const TCHAR* Key)
{
    float Volume = 1.f;
    AudioSettings().GetFloat(TEXT("TroubleSquad.Audio"), Key, Volume);
    return FMath::IsFinite(Volume) ? FMath::Clamp(Volume, 0.f, 1.f) : 1.f;
}

void WriteVolume(const TCHAR* Key, float Volume)
{
    if (!FMath::IsFinite(Volume)) { return; }
    FConfigFile& Settings = AudioSettings();
    Settings.SetFloat(TEXT("TroubleSquad.Audio"), Key, FMath::Clamp(Volume, 0.f, 1.f));
    Settings.Write(AudioPreferencesPath(), false);
}
}

int32 CombatAudio::GetVariationCount(const TCHAR* Cue)
{
    static const TMap<FString,int32> Counts={
#include "OriginalAudioVariants.inl"
    };
    const int32* Count=Counts.Find(Cue);return Count?*Count:1;
}
bool CombatAudio::IsIntentionallySilent(const TCHAR* Cue)
{
    static const TSet<FString> Silent={
#include "OriginalAudioSilence.inl"
    };
    return Silent.Contains(Cue);
}
FString CombatAudio::SelectMusicCue(int32 Arena,float Elapsed,bool bBoss,bool bSecondPhase)
{
    Arena=FMath::Clamp(Arena,0,3);
    if(bBoss) {return FString::Printf(TEXT("Music/Boss%d%s"),Arena,Arena==3&&bSecondPhase?TEXT("Phase2"):TEXT(""));}
    if(Elapsed>=360.f) {return TEXT("Music/Late");}
    if(Elapsed>=180.f) {return TEXT("Music/Mid");}
    return FString::Printf(TEXT("Music/Map%d"),Arena);
}
namespace
{
FString VariedCue(const TCHAR* Cue)
{
    static FRandomStream AudioRandom(0x534658);
    const int32 Variant=AudioRandom.RandRange(0,CombatAudio::GetVariationCount(Cue)-1);
    return Variant?FString::Printf(TEXT("%s_V%d"),Cue,Variant):FString(Cue);
}
}

float CombatAudio::GetMusicVolume() { return ReadVolume(TEXT("Music")); }
float CombatAudio::GetEffectsVolume() { return ReadVolume(TEXT("Effects")); }
void CombatAudio::SetMusicVolume(float Volume) { WriteVolume(TEXT("Music"), Volume); }
void CombatAudio::SetEffectsVolume(float Volume) { WriteVolume(TEXT("Effects"), Volume); }

USoundWave* CombatAudio::Load(const TCHAR* Cue)
{
    static TMap<FString, TWeakObjectPtr<USoundWave>> Cache;
    auto& Cached = Cache.FindOrAdd(Cue);
    if (!Cached.IsValid())
    {
        const FString Relative(Cue);
        const FString Path = FString::Printf(TEXT("/Game/TroubleSquad/Audio/%s.%s"),
            Cue, *FPaths::GetCleanFilename(Relative));
        Cached = LoadObject<USoundWave>(nullptr, *Path);
        if (USoundWave* Sound = Cached.Get())
        {
            const bool bMusic = Relative.StartsWith(TEXT("Music/"));
            const bool bPickup = Relative.StartsWith(TEXT("Pickups/"));
            const bool bWeapon = Relative.EndsWith(TEXT("_Weapon"));
            Sound->bOverrideConcurrency = true;

            Sound->ConcurrencyOverrides.MaxCount = bMusic ? 1 : bPickup ? 3 : bWeapon ? 3 : 2;
            Sound->ConcurrencyOverrides.RetriggerTime = bMusic ? 0.f : bPickup ? .045f : bWeapon ? .065f : .09f;
            Sound->ConcurrencyOverrides.ResolutionRule = EMaxConcurrentResolutionRule::StopOldest;
        }
    }
    return Cached.Get();
}

void CombatAudio::PlayLocal(UWorld* World, const TCHAR* Cue, float Volume)
{
    if (World->GetNetMode() != NM_DedicatedServer)
    {
        if(IsIntentionallySilent(Cue)) {return;}
        const FString Selected=VariedCue(Cue);
        UGameplayStatics::PlaySound2D(World, Load(*Selected), Volume * (FString(Cue).StartsWith(TEXT("Music/"))?GetMusicVolume():GetEffectsVolume()));
    }
}

void CombatAudio::PlayWorld(UWorld* World, const TCHAR* Cue, FVector Location, float Volume)
{
    if (World->GetNetMode() == NM_DedicatedServer) { return; }
    const APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0);
    const AActor* Listener = Controller ? Controller->GetViewTarget() : nullptr;
    if(!Listener&&Controller) {Listener=Controller->GetPawn();}
    if (!Listener) { return; }
    if (const auto* Character = Cast<AAnimaPreviewCharacter>(Listener); Character && Character->HasFinishedDistrict()) { return; }
    if(IsIntentionallySilent(Cue)) {return;}
    const FString Key(Cue);
    if(Key.StartsWith(TEXT("General/")))
    {
        static TWeakObjectPtr<UWorld> LastWorld;static FGeneralVoiceBudget Budget;
        if(LastWorld.Get()!=World){LastWorld=World;Budget=FGeneralVoiceBudget{};}
        if(!Budget.TryReserve(World->GetTimeSeconds(),Key.Contains(TEXT("_Impact")))){return;}
    }
    if(Key.StartsWith(TEXT("Enemies/")))
    {
        static TWeakObjectPtr<UWorld> BudgetWorld;static float NextMinor=0.f,NextDeath=0.f;
        if(BudgetWorld.Get()!=World) {BudgetWorld=World;NextMinor=NextDeath=0.f;}
        const bool Boss=Key.Contains(TEXT("Reksai_"))||Key.Contains(TEXT("Briar_"))||Key.Contains(TEXT("Belveth_"))||Key.Contains(TEXT("Aatrox_"))||Key.Contains(TEXT("Boss_"));
        if(!Boss&&!Key.Contains(TEXT("Windup")))
        {const float Now=World->GetTimeSeconds();const bool Death=Key.Contains(TEXT("Death"));float& Next=Death?NextDeath:NextMinor;if(Now<Next) {return;}Next=Now+(Death?.14f:.08f);}
    }
    const float Distance = FVector::Dist2D(Location, Listener->GetActorLocation());
    if (Distance >= 2200.f) { return; }
    const float Gain = FMath::GetMappedRangeValueClamped(FVector2D(250.f, 2200.f), FVector2D(1.f, 0.f), Distance);
    const FString Selected=VariedCue(Cue);
    UGameplayStatics::PlaySoundAtLocation(World, Load(*Selected), Location, Volume * Gain * GetEffectsVolume());
}
