param([string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8', [switch]$OnlySignatures, [switch]$FinalPolish,
    [switch]$GalleryOnly,
    [ValidateSet('L_CoastalResearch','L_Outskirts','L_SubterraneanLab','L_Beachhead')]
    [string]$MapName='L_CoastalResearch')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe'
$review=Join-Path $projectRoot 'Saved/VfxReview'
New-Item -ItemType Directory -Force -Path $review | Out-Null
$cases=@(@{Name='Lifecycle';Flags='-ArcadeVfxLifecycleTest -nullrhi';Index=0})
foreach($evolved in 0,1) { $cases+=@{Name="Weapons_$evolved";Flags='-ArcadeVfxGallery';Index=$evolved} }
foreach($character in 0..8) {
    foreach($skill in 'E','R') {
        $cases+=@{Name="Champion_${character}_$skill";Flags=('-ArcadeSkillReview '+$(if($skill -eq 'R'){'-UltimateOnly'}else{''}));Index=$character}
    }
}
if($OnlySignatures) {
    $cases=@(@{Name='Lifecycle_Final';Flags='-ArcadeVfxLifecycleTest -nullrhi';Index=0})
    foreach($character in 0..8) {
        foreach($evolved in 0,1) {
            $cases+=@{Name="Signature_${character}_$evolved";Flags=('-ArcadeSignatureReview '+$(if($evolved){'-EvolvedVfx'}else{''}));Index=$character}
        }
    }
}
if($FinalPolish) {
    $cases=@(
        @{Name='Lifecycle_PostPolish';Flags='-ArcadeVfxLifecycleTest -nullrhi';Index=0},
        @{Name='Weapons_PostPolish';Flags='-ArcadeVfxGallery';Index=1},
        @{Name='Illaoi_PostPolish';Flags='-ArcadeSkillReview';Index=3},
        @{Name='WindWall_PostPolish';Flags='-ArcadeSkillReview -UltimateOnly';Index=5},
        @{Name='Xayah_PostPolish';Flags='-ArcadeSkillReview -UltimateOnly';Index=8},
        @{Name='YasuoSignature_PostPolish';Flags='-ArcadeSignatureReview -EvolvedVfx';Index=5},
        @{Name='RivenSignature_PostPolish';Flags='-ArcadeSignatureReview -EvolvedVfx';Index=6}
    )
}
if($GalleryOnly) { $cases=@(@{Name=($MapName+'_Gallery');Flags='-ArcadeVfxGallery';Index=1}) }
foreach($case in $cases) {
    $log=Join-Path $review ($case.Name+'.log')
    $started=Get-Date
    $args="`"$projectRoot/TroubleSquad.uproject`" /Game/TroubleSquad/Maps/$MapName -game -nosteam -nosound -unattended -windowed -ResX=1280 -ResY=720 -ExecCmds=DisableAllScreenMessages -SwarmSkillVisual=$($case.Index) $($case.Flags) -seconds=8 -abslog=`"$log`""
    $process=Start-Process -FilePath $editor -ArgumentList $args -WindowStyle Hidden -PassThru
    if(!$process.WaitForExit(60000)) { throw "VFX review still running: $($case.Name), PID=$($process.Id)" }
    $text=Get-Content -LiteralPath $log -Raw
    if($process.ExitCode -ne 0 -or $text -match 'Fatal error:|Assertion failed:') { throw "Runtime failure: $($case.Name)" }
    if($case.Name -like 'Lifecycle*') {
        if($text -notmatch 'VFX lifecycle: budget=1 expired=1 active=0') { throw 'VFX lifetime/budget failed' }
    } else {
        if($text -notmatch 'VFX gallery: weapons=20|VFX skill review: character=|VFX signature review: character=') { throw "Missing VFX execution: $($case.Name)" }
        if($case.Flags -match 'ArcadeSkillReview' -and $text -notmatch 'Skill visual cast accepted=1') { throw "Skill was not actually cast: $($case.Name)" }
        if($case.Flags -match 'ArcadeVfxGallery') {
            foreach($kind in 0..5) {
                if($text -notmatch "Readable VFX loaded: kind=$kind vertices=[1-9]") { throw "Blender VFX mesh missing: kind=$kind" }
            }
        }
        $captures=[regex]::Matches($text,'Tracing Screenshot "([^"]+)"')
        if(!$captures.Count) { throw "Missing render capture: $($case.Name)" }
        $shot=Join-Path "$projectRoot/Saved/Screenshots/WindowsEditor" ($captures[$captures.Count-1].Groups[1].Value+'.png')
        if(!(Test-Path -LiteralPath $shot)) { throw "Capture file missing: $($case.Name)" }
        Copy-Item -LiteralPath $shot -Destination (Join-Path $review ($case.Name+'.png'))
    }
    Write-Output "PASS $($case.Name) (runtime and capture; artistic fidelity requires visual review)"
}
