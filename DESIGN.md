# Bruce Lee remake

This is a Windows port of the first room of the Commodore 64 game. It is a C++ rewrite of the rules, not an emulator. The pictures and the collision test come from the original bytes.

A frame is one pass through `Tick` in `src/bruce.cpp`, then the window copies `g.pixels` to the screen. Arrow keys move Bruce. Up jumps. Space punches while he is standing and flying-kicks while he is moving. Escape closes the window.

## How a frame runs

1. `main.cpp` loads the four playable data files into `g` and calls `PlaceBruce`, which stands him on the floor to the right of the lamp.
2. Each frame, `Tick` reads the arrow keys.
3. Left and right change `g.facing` and try a two-pixel step. The step is kept when `Probe` does not report a wall.
4. Up on its own, while he is on the ground and the key was not already held, starts the standing jump (state `$05`). He rises 4 pixels, then 2 pixels on each of the next four counts, and comes back down 2 pixels on each of six counts. The top of the hop is 12 pixels. Up while left or right is held starts the sideways jump (states `$06` and `$07`): 4 pixels sideways every count, rising 11 pixels, then dropping.
5. Space while left or right is held starts the flying kick (states `$0C` and `$0D`). He hops 3 pixels, travels flat, then drops. Space with neither direction held is the punch (state `$11`), a short picture with no step. Moving down stops on a floor or a wall. Moving up stops only on a wall, so he can leave the floor.
6. The picture is then chosen. The standing jump counts down from 15 the way state `$05` does: legs together, then the tuck, then the arms-out jump. The sideways jump counts down from 15 the way states `$06` and `$07` do, tucked at the ends and the leg extended in between. The flying kick counts down from 12 the way states `$0C` and `$0D` do. Otherwise a walk pose while a direction is held, or a stand pose. Walking off a ledge uses the legs-together picture.
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

## Character cells

The screen is 40 columns by 25 rows of cells. Each cell is 8 by 8 screen pixels. `charset.bin` holds the pictures. `room0.bin` holds a tile number for every cell, and that number picks a picture.

One tile number is written into two cells, one above the other, so that number covers 8 by 16 pixels. The two cells use different glyphs:

- The odd row draws glyph N, the primary half.
- The even row under it draws glyph N+128, the shadow half.

Each of those glyphs is its own 8 by 8 picture. Tile 11, the floor, is the clear case: glyph 11 is empty, so row 23 is blank, and glyph 139 is the stripe, so row 24 is the line you see.

The shadow name comes from the original loader. It writes the tile into the top cell and the same number with bit 7 set into the bottom cell. Bit 7 selects the upper 1KB of the character set, which is the other half of that picture. Each row keeps its own byte and its own 8 pixel rows. The next pair starts on the following row.

The pairs in this room are (3, 4), (5, 6), and so on through (23, 24). Both bytes of a pair hold the same tile number. Rows 0–2 are the score area and are empty (`$DC`) in `room0.bin`. The score text is drawn over row 0.

The packed game data stored each tile once. This file is the expanded screen, so the number is already in both cells. Rendering just reads the cell.

A hardware cell is only 8 pixels tall, so a taller picture is two cells stacked. In multicolor mode one colored dot is two screen dots wide and one screen dot tall, which makes it look flat. The artwork is 8 of those rows. Painting each row twice makes the dot square and the picture 16 screen lines tall: 8 in the primary cell and 8 in the shadow cell.

Multicolor mode is on for the whole screen, and a cell can still opt out. Bit 3 of its color byte selects the wide dots. The room bytes are `$08` and `$0E`, so the level is 160 dots across. The score digits leave that bit clear. Each bit is one screen dot, and a digit is 8 dots wide. Those shapes are the ordinary Commodore font.

The level art is 22 character rows, 176 screen lines. With every art row painted twice, that is 88 unique rows. The level is 160 by 88 dots. The other 24 screen lines are the score row and the two blank rows under it.

## Data files

The running game loads the first four. `title.bin` is the original title picture and is not drawn by this room.

### `data/charset.bin` — 2048 bytes

The room's character set, 256 glyphs of 8 bytes. A byte is one row of a glyph. Bit 7 is the leftmost pixel.

The file is two halves of 1024 bytes:

- Glyphs 0–127 are the primary half. Odd character rows use these.
- Glyphs 128–255 are the shadow half. Even character rows use these.

The room file stores a tile number without the extra 128. `PixelColor` and the collision test add 128 on even rows. Each half of a glyph is four source rows, and each of those rows is stored twice, which is the vertical doubling described above.

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

The expanded screen, 25 rows by 40 columns, one byte per cell. Byte `row * 40 + col` is the tile number. In each pair, the odd row and the even row under it store the same number. Rows 0–2 are `$DC`.

### `data/room0.col` — 1000 bytes

Color RAM, one byte per cell, same index as `room0.bin`.

The low three bits are the collision property. Bit 3 turns multicolor on. The values in this room are `$08` and `$0E`:

| Byte | Property (`byte & 7`) | Drawn ink | What collision does with a solid pattern |
| --- | --- | --- | --- |
| `$08` | 0 | black | wall. A walk or a jump into it is rejected |
| `$0E` | 6 | blue | floor only. It stops a fall and does not stop a walk |

The drawn ink is the low three bits. `$08` is black and `$0E` is light blue.

### `data/bruce.bin` — 3192 bytes

Nineteen poses. Each pose is two planes, and each plane is 21 rows of 4 bytes (32 pixels). Bit 7 is the leftmost pixel. One sprite bit is one screen pixel. Left and right are separate drawings; nothing is mirrored.

The walk and the standing jump use one source byte per row (library mode 0). After the VIC doubles it, the ink is 16 pixels and the other 16 in the row are empty. The punch, and the black plane of each extended flying kick, use two source bytes per row (mode 1), so the ink can reach all 32 pixels.

```
offset = (pose * 2 + plane) * 84
plane 0 = yellow body
plane 1 = black hair, eyes, and legs
row byte = offset + y * 4 + (x / 8)
```

| Pose | Picture |
| --- | --- |
| 0 | stand, facing right |
| 1 | stand, facing left |
| 2, 3 | walk left, the two strides |
| 4, 5 | walk right, the two strides |
| 6 | legs together: the fall, and jump count 15 |
| 7 | jump tuck, counts 12–14 and 1 |
| 8 | jump, arms out, counts 2–11 |
| 9, 10 | sideways jump left, tucked then extended |
| 11, 12 | sideways jump right, tucked then extended |
| 13 | punch left |
| 14 | punch right |
| 15, 16 | flying kick left, tucked then extended |
| 17, 18 | flying kick right, tucked then extended |

`g.tick / 6` alternates the two strides. While he is walking, both strides are tested, so the wider one cannot step into a wall that the next stride could not leave. In the air, and during a punch or a kick, the test uses the picture chosen for this frame.

Punch left is drawn 16 pixels left of `g.x`. The black plane of flying kick left, extended, is shifted the same way. That is the original adjustment of `$9E` by -8, so the torso stays where the stand picture was and the fist or the leg reaches past it.

These are the original library images, in order: 2/3, 0/1, `$0E`/`$0F`, `$10`/`$11`, `$12`/`$13`, `$14`/`$15`, 8/9, `$1E`/`$1F`, `$20`/`$21`, `$22`/`$23`, `$24`/`$25`, `$26`/`$27`, `$28`/`$29`, 4/5, 6/7, `$16`/`$17`, `$18`/`$19`, `$1A`/`$1B`, `$1C`/`$1D`. The even library index is the body and the next index is the detail. Library 8 is the fall. The jump the player sees for most of the arc is library `$20`. The sideways jump is libraries `$22`–`$29`. The punch is libraries 4–7. The flying kick is libraries `$16`–`$1D`.

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
