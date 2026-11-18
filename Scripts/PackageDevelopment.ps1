param(
    [string]$EngineRoot = 'C:/Program Files/Epic Games/UE_5.8',
    [string]$OutputDirectory,
    [switch]$Verify
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $projectRoot 'TroubleSquad.uproject'
if (!$OutputDirectory) { $OutputDirectory = Join-Path $projectRoot 'Saved/Builds/Development' }
$buildScript = Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat'
$uatScript = Join-Path $EngineRoot 'Engine/Build/BatchFiles/RunUAT.bat'
& $buildScript TroubleSquad Win64 Development "-Project=$project" -WaitMutex
if ($LASTEXITCODE -ne 0) { throw 'Game compilation failed; packaging was not started.' }
& $uatScript BuildCookRun "-project=$project" -noP4 -platform=Win64 -clientconfig=Development -skipbuild -nocompileeditor -cook -iterate -stage -pak -prereqs -archive "-archivedirectory=$OutputDirectory" -unattended -utf8output
if ($LASTEXITCODE -ne 0) { throw 'Packaging failed; the previous output must not be treated as this build.' }
if ($Verify) { & (Join-Path $PSScriptRoot 'VerifyPackagedMaps.ps1') -BuildDirectory $OutputDirectory }
