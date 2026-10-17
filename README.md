# Bruce Lee

This project recreates the original *Bruce Lee* on the Commodore 64, written natively for Windows with no emulator. It is a homage to the original game and a way to preserve it for the future.

This is still a work in progress. Current state: the first screen draws and the Bruce Lee character is playable.

![The first screen, with Bruce standing by the lamp](docs/first-room.png)

## Overview

The game is a C++ rewrite of the original rules. Pictures, the room layout, and the collision test come from the original bytes in `data/`. A Windows window, drawn with Direct2D, shows a 320 by 200 screen scaled up with nearest-neighbor filtering. Nothing is running inside a Commodore 64 emulator.

The window opens on the title picture. Space or Enter starts the first room, with Bruce on the floor to the right of the lamp. Each frame reads the keys, tries a move, keeps it only when the original overlap test allows it, picks the matching pose, and paints the room, Bruce, and the score line.

| Key | Action |
| --- | --- |
| Space or Enter | Leave the title screen |
| Left / Right | Walk |
| Up | Jump. With Left or Right held, jump sideways |
| Space | Punch while standing. Flying kick while moving |
| Escape | Close the window |

`build.bat` compiles with the Visual Studio C++ tools and writes `build\BruceLee.exe`. The executable looks for `charset.bin`, `room0.bin`, `room0.col`, `bruce.bin`, and `title.bin` beside itself, in `data\` one directory up, or in `data\` beside the executable. `config.ini` is read beside the exe, then in the parent folder. `ShowTitleScreen=False` in that file, or on the command line, skips the title.

`DESIGN.md` describes how a frame runs, how the character cells and sprite poses are stored, and how collision decides wall and floor.
