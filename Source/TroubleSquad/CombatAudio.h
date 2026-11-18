#pragma once

#include "CoreMinimal.h"

class USoundWave;
class UWorld;

namespace CombatAudio
{
    float GetMusicVolume();
    float GetEffectsVolume();
    void SetMusicVolume(float Volume);
    void SetEffectsVolume(float Volume);
    USoundWave* Load(const TCHAR* Cue);
    void PlayLocal(UWorld* World, const TCHAR* Cue, float Volume);
    void PlayWorld(UWorld* World, const TCHAR* Cue, FVector Location, float Volume);
}
