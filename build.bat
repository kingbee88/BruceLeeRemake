@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist build mkdir build
cl /nologo /EHsc /O2 /W4 /DUNICODE /D_UNICODE /Fo:build\ /Fe:build\BruceLee.exe ^
  src\game.cpp src\load.cpp src\scene.cpp src\collision.cpp src\bruce.cpp src\draw.cpp src\main.cpp ^
  /link /SUBSYSTEM:WINDOWS d2d1.lib user32.lib gdi32.lib
