@echo off

if [%~1] == [] echo Please specifiy a solutiondir && exit
if [%~2] == [] echo Please specifiy a configuration && exit

set SOLUTIONDIR=%~1
set BUILDDIR=%SOLUTIONDIR%\..\build\
set TOOLSDIR=%SOLUTIONDIR%\tools\

set CONFIGURATION=%~2

echo Removing old pk3 if exist
del "%BUILDDIR%\%CONFIGURATION%\bin\goldy\goldy_bin.pk3"

if not exist "%BUILDDIR%\%CONFIGURATION%\bin\goldy\cgame_mp_x86.dll" echo cgame_mp_x86.dll not found && exit
if not exist "%BUILDDIR%\%CONFIGURATION%\bin\goldy\ui_mp_x86.dll" echo ui_mp_x86.dll not found && exit

echo Creating pk3 file...
cd "%BUILDDIR%\%CONFIGURATION%\bin\goldy\"
"%TOOLSDIR%\zip.exe" -9 "goldy_bin.pk3" "cgame_mp_x86.dll" "ui_mp_x86.dll"