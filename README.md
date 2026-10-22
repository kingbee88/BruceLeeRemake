# Bruce Lee

This project recreates the original *Bruce Lee* on the Commodore 64, written natively for Windows with no emulator. It is a homage to the original game and a way to preserve it for the future in source format.

The Windows port is copyright Jacob Marner and is free software under the GNU General Public License, version 3 or any later version. The full text is in `LICENSE`. No copyright is claimed over the original game, its pictures, sound, room data, manual, disk image, or box art, and those files are not relicensed by this grant. The Commodore 64 game is copyright 1984 Datasoft, Inc. It was made by Ron J. Fortier, with graphics by Kelly Day and music by John A. Fitzpatrick. This Windows port was made as a homage to that game.

This is still a work in progress. 

Latest release: (https://github.com/kingbee88/BruceLeeRemake/releases/download/v0.2-alpha/BruceLeeRemakev0.2.zip)

![The first screen, with Bruce standing by the lamp](cover.jpg)


## Overview

The game is a C++ rewrite of the original game. Pictures, the room layout, and the collision test come from the original bytes in `data/`. A Windows window, drawn with Direct2D, shows a 320 by 200 screen scaled up with nearest-neighbor filtering. Nothing is running inside a Commodore 64 emulator.

The `referencematerial/` contains a copy of the C64 floppy disk image, and a commented disassembly version of the game, and top of that is VICE snapshots from each room. None of the content in this folder are part of the build. It is purely they to help me understand the original game.

| Key | Action |
| --- | --- |
| Space or Enter | Leave the title picture |
| F3 | On the options page, switch 1 player / 2 players |
| F5 | On the options page, switch computer / opponent |
| F7 | Leave the options page |
| Left / Right | Walk |
| Up | Jump. With Left or Right held, jump sideways |
| Down | Duck. On a ladder, climb down |
| Space | Punch while standing. Flying kick while moving |
| Escape | Leave full screen, and close Settings if it is open |
| Ctrl+R | Restart the game |
| F1 | Open the game manual |
| Ctrl+F1 | About |
| Ctrl+, | Settings |
| Alt+F4 | Exit |

File also lists Save game, Load game, Quick save, and Quick load. Those stay disabled. Settings is the gear at the right of the menu bar. Full screen hides the menu bar; the shortcuts still work.

Double-click anywhere in the window to toggle a borderless full screen. The maximize button turns it on. The Settings window does the same, and Escape leaves full screen. The display mode stays as it is. `FullScreen=True` in `config.ini`, or on the command line, starts in that mode. The program writes the settings back to the `config.ini` it opened when it closes.

An Xbox pad, or any other XInput controller, does the same things. The keyboard keeps working while a pad is connected.

| Xbox control | Action |
| --- | --- |
| A | Leave the title picture. Leave the options page. Punch while standing. Flying kick while moving |
| D-pad or left stick | Move. Left and right walk, up jumps, down ducks or climbs |
| D-pad or left stick, up with left or right | Jump sideways |

`build.bat` compiles with the Visual Studio C++ tools. `build.bat debug` writes `build\Debug\BruceLee.exe` with optimizations off and a program database beside it. `build.bat release`, or `build.bat` with no argument, writes the optimized `build\Release\BruceLee.exe`. `build.bat all` builds both. In Cursor, with the CodeLLDB extension installed, Ctrl+Shift+B builds Debug and F5 launches it under the debugger. The Run and Debug menu can start Release as well.

`DESIGN.md` describes how a frame runs, how the character cells and sprite poses are stored, and how collision decides wall and floor.

## Scoring rules

| Event | Points |
| --- | --- |
| Chopping Ninja or Yamo | 100 |
| Kicking ninja or Yamo | 75 |
| Landing on a fighter | 50 |
| Knocking out the Ninja | 450 |
| Knocking out Yamo | 200 |
| Lantern | 125 |
| First time into a room, except room 0 | 2000 |
| Each height band in room 13 | 500 |
| Destroying the wizard | 3000 |

Room 0 never pays. After the treasure room, the other rooms pay 2000 again on the next visit. An extra life is given at 40,000, then every 30,000. The same life counter is what room 16's symbol increments. Past 99 lives the line shows A0, B0, and so on through F9. At 1,000,000 the six digits show 000000 and the total keeps going.

The original game has a bug in a landing that drives the health byte negative does not stop at one 50 and one knockout. The game keeps applying that landing until the byte is positive again, so a Yamo on 4 health pays 6,550 and is left at 125, and a Ninja pays 13,050. A landing that does not cross zero pays 50 once. Set FixBugsInOriginal option to True to fix this. Set it to False to get original scoring.

In the Game Manual the scoring of knocking out Yamo and Ninja is swapped. This port using the scoring from the original game, not the manual.

## Trivia 

Room 16 has a ying-yang symbol. Picking it up with add an extra life. It will reappear if you leave the room and re-enter. You can get up to 5 extra lives this way. This counter does not reset when completing the game, so it is 5 lives max for the entire session.

After completing the game, you will restart in room 0 again. After you complete the game the following will change:
* The Room 13 electric floors moves differently.
* The time to spawn for Ninja and Yamo is reduced.


## Current state

* All 20 room added and feature complete
* All animation of Bruce, Yamo and Ninja and their behaviors are in.
* Gamepad support
* Scoring complete - added option to correct a fix in the original scoring.
* Game over screen
* Full screen support
* Infinite Lives and Invulnerability support
* Original sound: the effects during play, and the tune on the options page

## TODO before v1.0

* 2 player support
* CRT filtering
* Bug fixing


## TODO after v1.0

* Convert assets to something easier to edit.
* Load/save game state
* Record/Replay gameplay - set gamespeed
* A level editor
* Option to play with improved graphics/audio