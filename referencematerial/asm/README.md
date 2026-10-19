# Assembly

These files are readings of the original Commodore 64 *Bruce Lee*. They are not assembled into the Windows port. The port follows the same addresses.

The disk holds one program, `BRUCE LEE+2 /REM`. Only the loader is stored as plain machine code. The game itself is packed, and the listing of the running program is a memory image, not a copy of the disk file.

## disk-boot.asm

The loader on `../Bruce Lee.d64`. It loads at `$0801` like a BASIC file. The BASIC line is `SYS 2059`, which enters the copy stub at `$080B`. That stub copies an Exomizer decruncher to `$00F0` and jumps into it. The decruncher reads the packed bytes downward from `$A447` and writes the program downward from `$CF78`. When the stream is finished, the loader switches the ROMs back in and jumps to `$2800`.

Addresses in this file are where the instruction runs. The copy stub runs at `$080B`. The decruncher is stored at `$A548` and runs at `$00F0`.

## game.asm

The code in the running game, taken from a snapshot with room 0 on screen. It starts at `boot`, which expands the charsets and falls into `start`. Movement, collision, poses, the joystick read, room entry, the raster interrupt, and the per-room hooks are here. `X = 0` is Bruce. The other two fighters use the same fields.

Picture bytes, sprite images, and other data are left as unnamed stretches. The room tables are in `rooms.asm`.

## title.asm

The bytes at `$C000` in `../emulatorsnapshots/boot01_title.vsf`. They are not in `game.asm`. By the options page that RAM has been overwritten, and `$0400` is screen memory. This file is the part that shows the title picture, asks the crack's two questions, and copies the game down to `$0400`.

## rooms.asm

What changes from room to room. The running game has twenty rooms, and `$29` holds the index, 0 through 19. `game.asm` walks the tables here: the packed picture for each room, the charset, and how many lamps are left. The notes on room 1's trap door and room 5's energy beams are here too. The instructions are `hook_enter_1`, `hook_frame_1`, `hook_enter_5`, and `hook_frame_5` in `game.asm`.
