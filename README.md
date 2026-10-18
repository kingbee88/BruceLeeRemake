# Bruce Lee

This project recreates the original *Bruce Lee* on the Commodore 64, written natively for Windows with no emulator. It is a homage to the original game and a way to preserve it for the future.

This is still a work in progress. 

![The first screen, with Bruce standing by the lamp](docs/first-room.png)


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

`build.bat` compiles with the Visual Studio C++ tools and writes `build\BruceLee.exe`. The executable looks for `charset.bin`, `room0.bin`, `room0.col`, `bruce.bin`, and `title.bin` beside itself, in `data\` one directory up, or in `data\` beside the executable. `config.ini` is read beside the exe, then in the parent folder. `ShowTitleScreen=False` in that file, or on the command line, skips the title.

`DESIGN.md` describes how a frame runs, how the character cells and sprite poses are stored, and how collision decides wall and floor.

## Current state

* All 20 screens added - flow not complete yet, but now you can open hatch in room2.
* All Bruce movement animation added and he can move freely. 
* Gamepad support

## TODO

* Fix errors in all levels
* Scene progress all the way through
* Ninja and Yumi
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