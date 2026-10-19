# Bruce Lee

This project recreates the original *Bruce Lee* on the Commodore 64, written natively for Windows with no emulator. It is a homage to the original game and a way to preserve it for the future.

This is still a work in progress.

Latest release: (https://github.com/kingbee88/BruceLeeRemake/releases/download/v0.1-alpha/release0.1.zip)

![The first screen, with Bruce standing by the lamp](first-room.png)


## Overview

The game is a C++ rewrite of the original rules. Pictures, the room layout, and the collision test come from the original bytes in `data/`. A Windows window, drawn with Direct2D, shows a 320 by 200 screen scaled up with nearest-neighbor filtering. Nothing is running inside a Commodore 64 emulator.

The window opens on the title picture. Space, Enter, or A on a pad starts the first room, with Bruce on the floor to the right of the lamp. Each frame reads the keys and any Xbox pad, tries a move, keeps it only when the original overlap test allows it, picks the matching pose, and paints the room, Bruce, and the score line.

| Key | Action |
| --- | --- |
| Space or Enter | Leave the title screen |
| Left / Right | Walk |
| Up | Jump. With Left or Right held, jump sideways |
| Down | Duck. On a ladder, climb down |
| Space | Punch while standing. Flying kick while moving |
| Escape | Close the window |

An Xbox pad, or any other XInput controller, does the same things. The keyboard keeps working while a pad is connected.

| Xbox control | Action |
| --- | --- |
| A | Leave the title screen. Punch while standing. Flying kick while moving |
| D-pad or left stick | Move. Left and right walk, up jumps, down ducks or climbs |
| D-pad or left stick, up with left or right | Jump sideways |

`build.bat` compiles with the Visual Studio C++ tools. `build.bat debug` writes `build\Debug\BruceLee.exe` with optimizations off and a program database beside it. `build.bat release`, or `build.bat` with no argument, writes the optimized `build\Release\BruceLee.exe`. `build.bat all` builds both. In Cursor, with the CodeLLDB extension installed, Ctrl+Shift+B builds Debug and F5 launches it under the debugger. The Run and Debug menu can start Release as well.

The executable looks for `charset.bin`, `room0.bin`, `room0.col`, `bruce.bin`, and `title.bin` beside itself, in `data\` one or two directories up, or in `data\` beside the executable. `config.ini` is read beside the exe, then one folder up, then two folders up, so an exe in `build\Debug` or `build\Release` still finds the copies at the project root. `ShowTitleScreen=False` in that file, or on the command line, skips the title.

`DESIGN.md` describes how a frame runs, how the character cells and sprite poses are stored, and how collision decides wall and floor.

## Current state

* All 20 room added - most rooms are playable and has lamps, beams, doors, etc. working
* All Bruce movement and fighting animations added and he can move as in original. 
* Gamepad support

## TODO

* Fix errors in all levels
* Prove the game is completable
* Add Ninja and Yumi and health
* Game over screen
* Fullscreen support
* Trainers
* Audio
* Scoring
* Datasoft selection screen
* 2 player support
* CRT filtering

## Stretch goals

* Load/save game state
* A level editor
* Option to play with improved graphics