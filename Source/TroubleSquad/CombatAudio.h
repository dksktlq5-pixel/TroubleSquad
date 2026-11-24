#pragma once

#include "CoreMinimal.h"

class USoundWave;
class UWorld;

namespace CombatAudio
{
    struct FGeneralVoiceBudget
    {
        float ReadyAt[2]={};
        bool TryReserve(float Now,bool bImpact)
        {
            float& Next=ReadyAt[bImpact?1:0];
            if(Now<Next){return false;}
            Next=Now+.025f;return true;
        }
    };

    float GetMusicVolume();
    float GetEffectsVolume();
    void SetMusicVolume(float Volume);
    void SetEffectsVolume(float Volume);
    int32 GetVariationCount(const TCHAR* Cue);
    bool IsIntentionallySilent(const TCHAR* Cue);
    FString SelectMusicCue(int32 Arena,float Elapsed,bool bBoss,bool bSecondPhase);
    USoundWave* Load(const TCHAR* Cue);
    void PlayLocal(UWorld* World, const TCHAR* Cue, float Volume);
    void PlayWorld(UWorld* World, const TCHAR* Cue, FVector Location, float Volume);
}
