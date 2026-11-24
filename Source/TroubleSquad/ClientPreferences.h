#pragma once
class APlayerController;
class UWorld;
namespace ClientPreferences
{
 void Toggle(APlayerController* Owner);
 bool IsOpen();
 void EnterWorld(bool bCombat,bool bForce=false);
#if WITH_EDITOR
 void RunAcceptanceTest(UWorld* World);
#endif
 bool DamageNumbers();
}
