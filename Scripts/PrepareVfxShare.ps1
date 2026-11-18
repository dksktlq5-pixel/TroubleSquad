param(
    [string]$ArchiveDirectory = 'D:/UnReal/TroubleSquad/Saved/Builds/Vfx20261119',
    [string]$ShareDirectory = 'C:/Users/cksdn/Desktop/공유폴더'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $ArchiveDirectory 'Windows'
$native = 'TroubleSquad/Binaries/Win64/TroubleSquad.exe'
if (!(Test-Path -LiteralPath (Join-Path $source $native))) { throw 'Packaged executable missing.' }
Copy-Item -LiteralPath "$root/Build/Play_Online.bat","$root/Build/Play_Solo.bat" -Destination $source
Copy-Item -LiteralPath "$root/Build/ROOM_CODE_README.ko.txt" -Destination (Join-Path $source '온라인 참가 방법.txt')
Copy-Item -LiteralPath "$root/Build/PLAYTEST_README.ko.txt" -Destination (Join-Path $source '플레이 안내.txt')
Set-Content -LiteralPath (Join-Path $source 'steam_appid.txt') -Value '480' -Encoding ascii
Set-Content -LiteralPath (Join-Path $source 'TroubleSquad/Binaries/Win64/steam_appid.txt') -Value '480' -Encoding ascii
$destination = Join-Path $ShareDirectory ('TroubleSquad_VFX_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
New-Item -ItemType Directory -Path $destination | Out-Null
& robocopy $source $destination /E /R:1 /W:1 /NFL /NDL /NP /NJH /NJS
if ($LASTEXITCODE -ge 8) { throw "Copy failed: robocopy exit $LASTEXITCODE" }
$sourceFiles = @(Get-ChildItem -LiteralPath $source -Recurse -File)
$destinationFiles = @(Get-ChildItem -LiteralPath $destination -Recurse -File)
if ($sourceFiles.Count -ne $destinationFiles.Count) { throw 'Copied file count differs.' }
foreach ($file in $sourceFiles) {
    $relative = $file.FullName.Substring($source.Length).TrimStart('\','/')
    $copied = Join-Path $destination $relative
    if ((Get-Item -LiteralPath $copied).Length -ne $file.Length) { throw "Size mismatch: $relative" }
    if ($file.Extension -in '.exe','.dll','.pak','.utoc','.ucas','.bat','.txt') {
        if ((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath $copied).Hash) { throw "Hash mismatch: $relative" }
    }
}
$report = [ordered]@{ Directory=$destination; Files=$sourceFiles.Count; Bytes=($sourceFiles | Measure-Object Length -Sum).Sum; VerifiedAt=(Get-Date -Format o) }
$report | ConvertTo-Json | Set-Content -LiteralPath "$root/Saved/Logs/VfxShareVerification.json" -Encoding utf8
$report | ConvertTo-Json
exit 0
