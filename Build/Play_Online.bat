@echo off
setlocal
cd /d "%~dp0"
set "SteamAppId=480"
set "SteamGameId=480"
tasklist /FI "IMAGENAME eq steam.exe" 2>NUL | find /I "steam.exe" >NUL
if errorlevel 1 (
  echo Please start Steam and sign in, then run this file again.
  pause
  exit /b 1
)
start "" "TroubleSquad.exe" -OnlineParty -windowed -ResX=1280 -ResY=720
