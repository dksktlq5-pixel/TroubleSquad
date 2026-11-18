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
FConfigFile& AudioSettings()
{
    static FConfigFile Settings;
    static bool bLoaded = false;
    if (!bLoaded)
    {
        Settings.Read(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config/Audio.ini")));
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
    Settings.Write(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config/Audio.ini")), false);
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
        UGameplayStatics::PlaySound2D(World, Load(Cue), Volume * GetEffectsVolume());
    }
}

void CombatAudio::PlayWorld(UWorld* World, const TCHAR* Cue, FVector Location, float Volume)
{
    if (World->GetNetMode() == NM_DedicatedServer) { return; }
    const APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0);
    const APawn* Listener = Controller ? Controller->GetPawn() : nullptr;
    if (!Listener) { return; }
    if (const auto* Character = Cast<AAnimaPreviewCharacter>(Listener); Character && Character->HasFinishedDistrict()) { return; }
    const float Distance = FVector::Dist2D(Location, Listener->GetActorLocation());
    if (Distance >= 2200.f) { return; }
    const float Gain = FMath::GetMappedRangeValueClamped(FVector2D(250.f, 2200.f), FVector2D(1.f, 0.f), Distance);
    UGameplayStatics::PlaySoundAtLocation(World, Load(Cue), Location, Volume * Gain * GetEffectsVolume());
}
