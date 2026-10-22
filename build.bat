@echo off
setlocal EnableExtensions
cd /d "%~dp0"

rem Usage: build.bat [debug|release|all]
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=release"

if /I "%CONFIG%"=="all" (
  call "%~f0" debug
  if errorlevel 1 exit /b 1
  call "%~f0" release
  if errorlevel 1 exit /b 1
  exit /b 0
)

if /I "%CONFIG%"=="debug" goto config_debug
if /I "%CONFIG%"=="release" goto config_release

echo Usage: build.bat [debug^|release^|all]
exit /b 1

:config_debug
set "OUTDIR=build\Debug"
set "CFLAGS=/nologo /std:c++17 /FC /EHsc /MP /FS /Od /Zi /RTC1 /MDd /W4 /D_DEBUG /DUNICODE /D_UNICODE"
set "PDB=/Fd:build\Debug\vc.pdb"
set "LDFLAGS=/SUBSYSTEM:WINDOWS /DEBUG:FULL"
goto build

:config_release
set "OUTDIR=build\Release"
set "CFLAGS=/nologo /std:c++17 /FC /EHsc /MP /FS /O2 /MD /W4 /DNDEBUG /DUNICODE /D_UNICODE"
set "PDB="
set "LDFLAGS=/SUBSYSTEM:WINDOWS"
goto build

:build
set "VSINSTALL="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
  for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"
)
if not defined VSINSTALL set "VSINSTALL=C:\Program Files\Microsoft Visual Studio\18\Community"
set "VCVARS=%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
  echo Could not find the Visual Studio C++ tools.
  echo Install the Desktop development with C++ workload in the Visual Studio Installer.
  exit /b 1
)

call "%VCVARS%" >nul
if errorlevel 1 (
  echo Failed to load the Visual Studio build environment.
  exit /b 1
)

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

echo Building %CONFIG% -^> %OUTDIR%\BruceLee.exe
cl %CFLAGS% %PDB% /Fo:%OUTDIR%\ /Fe:%OUTDIR%\BruceLee.exe ^
  src\game.cpp src\load.cpp src\config.cpp src\room.cpp src\roomfx.cpp src\scene.cpp src\collision.cpp src\bruce.cpp src\fighter.cpp src\pad.cpp src\draw.cpp src\front.cpp src\sound.cpp src\ui.cpp src\main.cpp ^
  /link %LDFLAGS% d2d1.lib dwmapi.lib xinput.lib user32.lib gdi32.lib xaudio2.lib ole32.lib comctl32.lib shell32.lib windowscodecs.lib uxtheme.lib advapi32.lib
if errorlevel 1 exit /b 1
echo Built %OUTDIR%\BruceLee.exe
exit /b 0
