# Bruce Lee remake

This is a Windows port of the first room of the Commodore 64 game. It is a C++ rewrite of the rules, not an emulator. The pictures and the collision test come from the original bytes.

A frame is one pass through `Tick` in `src/bruce.cpp`, then the window copies `g.pixels` to the screen. Arrow keys move Bruce. Escape closes the window.

## How a frame runs

1. `main.cpp` loads the four playable data files into `g` and calls `PlaceBruce`, which stands him on the floor to the right of the lamp.
2. Each frame, `Tick` reads the arrow keys.
3. Left and right change `g.facing` and try a two-pixel step. The step is kept when `Probe` does not report a wall.
4. Up starts a jump when he is on the ground and the key was not already held. Vertical speed starts at -8 (up) and increases by 1 each frame until it reaches 4 (down).
5. Moving down stops on a floor or a wall. Moving up stops only on a wall, so he can leave the floor.
6. The picture is then chosen: the air pose while `g.vy` is not zero, a walk pose while a direction is held, otherwise a stand pose.
7. If that new picture sits inside a wall, he is pushed sideways until it does not.
8. `Compose` paints the room, Bruce, and the score line into `g.pixels`. The window shows that buffer scaled up with nearest-neighbor filtering.

`g.x` and `g.y` are the top-left of his sprite in screen pixels. The screen is 320 by 200. A character cell is 8 by 8 pixels, and there are 40 columns and 25 rows.

## Source files

| File | Role |
| --- | --- |
| `src/game.h`, `src/game.cpp` | Screen size, the C64 palette, and the `Game` state every other file shares |
| `src/load.cpp` | `LoadFile` reads one data file. It looks beside the exe, then in `data\` one directory up, then in `data\` beside the exe. A missing file or a short read comes back empty |
| `src/scene.cpp` | Turns one screen pixel into a color. `BandAt` is the raster split (background, `$D022`, `$D023`). `PixelColor` returns a palette index, or -1 where the cell is empty or the same as the background |
| `src/collision.cpp` | The original overlap test. `Probe` reports wall and floor for the pose he is about to use. `PoseWall` asks the same question for one picture only |
| `src/bruce.cpp` | Which picture to show, where he starts, and `Tick` |
| `src/draw.cpp` | `Compose` fills `g.pixels` |
| `src/main.cpp` | The window, Direct2D, and the message loop |

`StandPose` and `WalkPose` return an index into `bruce.bin`. `PoseBytes` points at one of the two pictures inside that pose: plane 0 is the yellow body, plane 1 is the black detail drawn on top.

## Palette

`kPalette` is the Pepto C64 palette, packed as `0xAARRGGBB`. Indexes used by this room:

| Index | Color | Where it shows up |
| --- | --- | --- |
| 0 | black | sprite detail, score text, collision ink `$08` |
| 1 | white | upper raster colors |
| 2 | red | middle and lower `$D022` |
| 7 | yellow | Bruce's body |
| 11 (`$0B`) | dark grey | background above raster `$97` |
| 12 (`$0C`) | grey | background from raster `$97` down |
| 14 (`$0E`) | light blue | top `$D023`, and collision ink `$0E` |
| 15 | light grey | the score bar |

Screen y 0 is raster line 50. `BandAt` changes colors at raster `$78` and `$97`.

## Data files

The running game loads the first four. `title.bin` is the original title picture and is not drawn by this room.

### `data/charset.bin` — 2048 bytes

The room's character set, 256 glyphs of 8 bytes. A byte is one row of a glyph. Bit 7 is the leftmost pixel.

The file is two halves of 1024 bytes:

- Glyphs 0–127 are the primary half. Odd character rows use these.
- Glyphs 128–255 are the shadow half. Even character rows use these.

The room file stores a tile number without that extra 128. `PixelColor` and the collision test add 128 on even rows. That matches the original, which stores the same tile on two screen rows and sets bit 7 on the lower of the pair. Each half of a glyph is four source rows, duplicated to eight display rows when the charset was expanded.

Pixels are multicolor. A byte holds four dots, two bits each, from the top of the byte:

| Bits | Meaning |
| --- | --- |
| `00` | the band background from `BandAt` |
| `01` | the band's `$D022` color |
| `10` | the band's `$D023` color |
| `11` | the ink in `room0.col` |

Each dot is drawn as two screen pixels, so a cell is 8 pixels wide.

The byte for screen pixel `(x, y)` is:

```
tile = room0[(y / 8) * 40 + (x / 8)]
half = 128 on an even character row, else 0
byte = charset[(tile + half) * 8 + (y & 7)]
```

Tile `$DC` is an empty cell. It has no ink and no collision.

### `data/room0.bin` — 1000 bytes

The first room's screen, 25 rows by 40 columns, row by row. Byte `row * 40 + col` is the tile number described above. The same tile is stored on an even row and the odd row under it; the charset half, not this byte, makes those two rows look different.

### `data/room0.col` — 1000 bytes

Color RAM, one byte per cell, same index as `room0.bin`.

The low three bits are the collision property. Bit 3 turns multicolor on. The values in this room are `$08` and `$0E`:

| Byte | Property (`byte & 7`) | Drawn ink | What collision does with a solid pattern |
| --- | --- | --- | --- |
| `$08` | 0 | black | wall. A walk or a jump into it is rejected |
| `$0E` | 6 | blue | floor only. It stops a fall and does not stop a walk |

The drawn ink is the low three bits. `$08` is black and `$0E` is light blue.

### `data/bruce.bin` — 882 bytes

Seven poses. Each pose is two planes, and each plane is 21 rows of 3 bytes (24 pixels). Bit 7 is the leftmost pixel. One sprite bit is one screen pixel. Left and right are separate drawings; nothing is mirrored.

```
offset = (pose * 2 + plane) * 63
plane 0 = yellow body
plane 1 = black hair, eyes, and legs
row byte = offset + y * 3 + (x / 8)
```

| Pose | Picture |
| --- | --- |
| 0 | stand, facing right |
| 1 | stand, facing left |
| 2, 3 | walk left, the two strides |
| 4, 5 | walk right, the two strides |
| 6 | in the air |

`g.tick / 6` alternates the two strides. While he is walking, both strides are tested, so the wider one cannot step into a wall that the next stride could not leave.

These are the original library images, in order: 2/3, 0/1, `$0E`/`$0F`, `$10`/`$11`, `$12`/`$13`, `$14`/`$15`, 8/9. The even library index is the body and the next index is the detail.

### `data/title.bin` — 8000 bytes

The title picture, copied from address `$2000` in the unpacked game. It is the standard 320 by 200 hires bitmap. It is not a row of 320 pixels followed by the next row. The VIC chip stores 25 rows of 40 cells, and each cell is eight bytes, one byte per pixel row of that 8 by 8 cell.

```
byte = ((y / 8) * 40 + (x / 8)) * 8 + (y & 7)
bit  = 0x80 >> (x & 7)
```

A set bit is ink. This file has no color bytes. The current room does not load it.

## Collision

`Probe` is the test the original runs before it will keep a move. It ORs the yellow and black planes into one mask, then ANDs that mask with the character bytes underneath. Each AND byte is split into two 4-bit values.

| AND nibble | On property 0 | On any other property |
| --- | --- | --- |
| 3 or 11–15 | wall | floor |
| 7 | wall and floor | floor |
| 1, 4, 5, 6, 9, or 13 | floor | floor |
| anything else, including the statue's patterns | ignored | ignored |

A pixel past the edge of the 320 by 200 screen counts as a wall.

The statue is drawn, and its color bytes are mostly property 0, but its character patterns are not in the table. He runs through that art. The red line he stands on is an edge pattern, so it holds him up. The side borders and the solid underside of the floor are wall patterns, so they stop him.
