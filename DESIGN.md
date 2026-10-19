# Bruce Lee remake

This is a Windows port of the first room of the Commodore 64 game. It is a C++ rewrite of the rules, not an emulator. The pictures and the collision test come from the original bytes.

A frame is one pass through `Tick` in `src/bruce.cpp`, then the window copies `g.pixels` to the screen. The window waits 60ms between calls. That is three PAL screens, the count the game stores in `$48`. The window opens on the title picture. Space or Enter opens the options page. F3 and F5 change the two options there, and F7 opens the PLAYER page, which holds and then starts the room. Arrow keys move Bruce. Up jumps. On a ladder, up and down climb. Down on a floor ducks. Space punches while he is standing, and flying-kicks once a walk has already been going. Escape closes the window.

`config.ini` beside the exe is read first. If that file is not there, `config.ini` in the parent folder is read. `Name=Value` on the command line overrides the file. The names and the defaults are the commented lines in the root `config.ini`. `ShowTitleScreen` chooses the title picture, the options page, and the PLAYER page. `InfiniteLives`, `EnemiesCantHitYou`, `StartRoom`, `FullScreen`, and `ShowCRTFilter` are read and not used yet.

## How a frame runs

1. `main.cpp` loads the room files into `g`. With `ShowTitleScreen` on, it paints `title.bin` and waits for Space or Enter. That opens the options page in `front.bin`. F7 then shows the PLAYER page. Then `PlaceBruce` stands him on the floor to the right of the lamp. `ShowTitleScreen` off skips those three pages.
2. Each logic frame, `Tick` reads the arrow keys and space. Opposite directions cancel. Space is read as a level, so holding it repeats once the move allows it.
3. If the pose hold is running, it counts down and the stick is not read. Climb holds for 3 logic frames. Duck holds for 1. Every other Bruce move holds for 0.
4. Otherwise the repeat count reloads to 6 when the state is idle or has just changed, and counts down while the same state continues. Left or right walks two xpos steps (4 screen pixels) until that count reaches 0, then three steps (6 pixels). The two stride pictures alternate every logic frame. The screen edge clears the walk. A wall refuses the step and leaves the walk in place.
5. Up on a floor, with no ladder and no wall in the overlap, starts the standing jump (state `$05`, timer 15). That frame does not move. Three counts stay still, one count rises 4 pixels, four counts rise 2, six counts drop 2, and the last count stays still. The top of the hop is 12 pixels. A ladder overlap cancels the jump. A wall overlap refuses to start it. Up with left or right starts the sideways jump (states `$06` and `$07`, timer 15): 4 screen pixels sideways on each of the 15 counts, rising 11 pixels and then dropping 11. On a ladder that stick clears the state.
6. Space during a walk, once the repeat count is below 4, starts the flying kick (states `$0C` and `$0D`, timer 12) instead of the next step. The kick waits until the following logic frame to move. It then rises 1 pixel on each of three counts, travels flat for seven, and drops 1 on each of the last two. Every one of those 12 counts also steps 2 screen pixels. A ladder or a wall cancels it. If he is still in the air when the timer ends, he falls. Space with no direction, and no ladder, is the punch (state `$11`): 4 logic frames of the punch picture, then 4 frames of standing while the stick is ignored. Up with space jumps. Down with space ducks.
7. Down on a floor ducks (state `$1A`). Down-left and down-right turn him to face that way, except at the far screen edge. He does not walk while the duck is held. On a ladder, down climbs and a diagonal clears the state.
8. Up or down on a ladder climbs 3 pixels, or 4 once the repeat count has run out, then holds for 3 logic frames. The two climb pictures alternate while he climbs or walks across a ladder. Standing on a ladder keeps the climb picture already showing. Only an overlap counts: he does not reach below his feet. The screen edge is the only thing that cuts a burst short.
9. With neither ground nor a ladder under him, the first airborne frame drops 1 pixel. Later falling frames drop 2. The second falling frame also steps one xpos toward the way he faces when his torso is still on a ledge. The next frame lands him if his feet overlap ground or a ladder, so he rests in the first ground pixels. Open air shows the arms-out jump. Overlapping a wall shows the legs together.
10. The picture is chosen after the step, from the overlap taken before it. The standing jump, sideways jump, and flying kick follow the timers above. A fresh move shows its first picture on the frame it starts, and moves on the frame after.
11. If that new picture sits inside a wall, he is pushed sideways until it does not.
12. `Compose` paints the room, Bruce, and the score line into `g.pixels`. The window shows that buffer scaled up with nearest-neighbor filtering.

Before the move, the overlap is tested for a lamp. Bit 1 of that test is nibble 2, the hanger, and the same bit on a floor. The cells looked at are his column and the two to the right, on his tile row and the row below. A cell tiled `$13`, `$14`, or `$31` that is in this room's lamp list is replaced with the bare mount (`$12` or `$1F` on both rows of the pair) and 125 is added to the score. The six lamps are columns 31 and 38 of tile row 7, and columns 2, 25, 29, and 38 of tile row 10.

The lamp flicker is `$40BC`, once per logic frame, before that test. Three counters each reload to a count from 1 to 15. Tile `$31` rewrites rows 2 and 3 of shadow glyph `$B1`, and tile `$14` does the same to glyph `$94`. The new byte keeps the mount and moves one dot through the four multicolor pairs: background, `$D022`, `$D023`, and the ink. Every lamp that uses that tile changes on the same frame. The bare mount is a different glyph, so a taken lamp stays still. The same pass also writes glyphs `$93` and `$86`, and glyph `$E7`. This room does not show those three.

`g.x` and `g.y` are the top-left of his sprite in screen pixels. The screen is 320 by 200. A character cell is 8 by 8 pixels, and there are 40 columns and 25 rows.

## Source files

| File | Role |
| --- | --- |
| `src/game.h`, `src/game.cpp` | Screen size, the C64 palette, and the `Game` state every other file shares |
| `src/load.cpp` | `LoadFile` reads one data file. It looks beside the exe, then in `data\` one directory up, then in `data\` beside the exe. A missing file or a short read comes back empty |
| `src/config.cpp` | `LoadConfig` reads `config.ini` beside the exe, then in the parent folder, then applies the command line |
| `src/scene.cpp` | Turns one screen pixel into a color. `BandAt` is the raster split (background, `$D022`, `$D023`). `PixelColor` returns a palette index, or -1 where the cell is empty or the same as the background |
| `src/collision.cpp` | The original overlap test. `Probe` reports wall and floor for the pose he is about to use. `PoseWall` asks the same question for one picture only |
| `src/bruce.cpp` | Which picture to show, where he starts, and `Tick` |
| `src/draw.cpp` | `Compose` fills `g.pixels`. `ComposeTitle` paints `title.bin` |
| `src/front.cpp` | `ComposeMenu` and `ComposePlayerUp` paint the two pages in `front.bin` |
| `src/main.cpp` | The window, Direct2D, and the message loop |

`StandPose` and `WalkPose` return an index into `bruce.bin`. `PoseBytes` points at one of the two pictures inside that pose: plane 0 is the yellow body, plane 1 is the black detail drawn on top.

## Palette

`kPalette` is the Colodore C64 palette, the one VICE 3.10 displays, packed as `0xAARRGGBB`. Indexes used by this room:

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

The running game loads the first four for the room. `title.bin` is the original title picture, drawn when `ShowTitleScreen` is on.

### `data/charset.bin` — 2048 bytes

The room's character set, 256 glyphs of 8 bytes. A byte is one row of a glyph. Bit 7 is the leftmost pixel.

The file is two halves of 1024 bytes:

- Glyphs 0–127 are the primary half. Odd character rows use these.
- Glyphs 128–255 are the shadow half. Even character rows use these.

The room file stores a tile number without the extra 128. `PixelColor` and the collision test add 128 on even rows. Each half of a glyph is four source rows, and each of those rows is stored twice, which is the vertical doubling described above. Rows 2 and 3 of glyphs `$93`, `$94`, `$B1`, `$86`, and `$E7` are rewritten each time a lamp timer elapses, so those bytes in memory drift away from the file.

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
| `$0E` | 6 | blue | floor or climb, from the nibble. It does not stop a walk |

The drawn ink is the low three bits. `$08` is black and `$0E` is light blue.

### `data/bruce.bin` — 3864 bytes

Twenty-three poses. Each pose is two planes, and each plane is 21 rows of 4 bytes (32 pixels). Bit 7 is the leftmost pixel. One sprite bit is one screen pixel. Left and right are separate drawings; nothing is mirrored.

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
| 6 | legs together: a fall that overlaps a wall, and jump count 15 |
| 7 | jump tuck, counts 12–14 and 1 |
| 8 | jump, arms out, counts 2–11. Also a fall in open air |
| 9, 10 | sideways jump left, tucked then extended |
| 11, 12 | sideways jump right, tucked then extended |
| 13 | punch left |
| 14 | punch right |
| 15, 16 | flying kick left, tucked then extended |
| 17, 18 | flying kick right, tucked then extended |
| 19, 20 | climb, the two steps |
| 21 | duck, facing left |
| 22 | duck, facing right |

The two strides alternate every logic frame. While he is walking, both strides are tested, so the wider one cannot step into a wall that the next stride could not leave. Every other state uses the picture already chosen.

Punch left is drawn 16 pixels left of `g.x`. The black plane of flying kick left, extended, and the black plane of the right-facing duck, are shifted the same way. That is the original adjustment of `$9E` by -8, so the torso stays where the stand picture was and the fist, the leg, or the duck's detail hangs off that side.

These are the original library images, in order: 2/3, 0/1, `$0E`/`$0F`, `$10`/`$11`, `$12`/`$13`, `$14`/`$15`, 8/9, `$1E`/`$1F`, `$20`/`$21`, `$22`/`$23`, `$24`/`$25`, `$26`/`$27`, `$28`/`$29`, 5/4, 7/6, `$16`/`$17`, `$18`/`$19`, `$1A`/`$1B`, `$1C`/`$1D`, `$0A`/`$0B`, `$0C`/`$0D`, `$40`/`$41`, `$42`/`$43`. The first of each pair is the yellow plane and the second is the black detail, except the punch: libraries 4 and 6 are black (`$F0`) and 5 and 7 are yellow (`$F7`), so those pairs are stored yellow first. Library 8 is the legs-together picture. The jump the player sees for most of the arc is library `$20`. The sideways jump is libraries `$22`–`$29`. The punch is libraries 4–7. The flying kick is libraries `$16`–`$1D`. The climb is libraries `$0A`–`$0D`. The duck is libraries `$40`–`$43`.

### `data/title.bin` — 8000 bytes

The title picture, copied from address `$2000` in the title snapshot. It is a multicolor bitmap, not a row of 320 pixels followed by the next row. The VIC chip stores 25 rows of 40 cells, and each cell is eight bytes, one byte per pixel row of that 8 by 8 cell. Each byte holds four pixel pairs.

```
byte = ((y / 8) * 40 + (x / 8)) * 8 + (y & 7)
pair = (byte >> (6 - ((x & 7) / 2) * 2)) & 3
```

`$D011` is `$3B` and `$D016` is `$18`, so multicolor bitmap mode is on. `$D018` is `$18`: the bitmap is `$2000` and the video matrix is `$0400`. Every matrix byte is `$1E`, and color RAM is `$0C`. `$D021` is black. Pair `00` is that background, `01` is the high nibble (white), `10` is the low nibble (light blue), and `11` is color RAM (grey). `$D020` is the light blue border and is not part of the picture. The bitmap paints that same light blue along its outer edge, so on the Commodore it continues the border. Those pixels are drawn in the grey of the picture field. Light blue inside the picture is the buildings and the frame. `ShowTitleScreen` paints it and waits for Space or Enter. That opens the options page. `ShowTitleScreen=False` skips the picture, the options page, and the PLAYER page, and opens the room.

### `data/front.bin` — 5136 bytes

The options page and the PLAYER page, copied from the menu snapshot. 2048 bytes are the charset at `$8000`, then 2048 bytes of the charset at `$8800`, then the 1000 screen codes at `$8C00`, then the 32 player-line glyphs at `$4A5D`, then the 8 digit glyphs at `$4A7D`. The stored screen has 1 PLAYER and COMPUTER highlighted. F3 and F5 run `flip_menu_a` and `flip_menu_b`. F7 shows PLAYER 1, which `between_rooms` holds for `$40` PAL frames, or `$80` when two players are selected.

## Collision

`Probe` is the test the original runs before it will keep a move. It ORs the yellow and black planes into one mask, then ANDs that mask with the character bytes underneath. Each AND byte is split into two 4-bit values.

`$5592` rolls `$4F3F` in first, so that counter is bit 3 and `$4F3C` is bit 0. `$1DCA` keeps him from falling when bit 0 or bit 2 is set (`and #$05`). Bit 3 is the push-out at `$1CC7`. A wall does not hold him up.

Bit 0 is the same nibbles on every property. The solid nibbles are bit 3 on property 0 and bit 2 on any other property. The swap is `$55E6`, `$5658`, and `$56CA`. Bit 2 is a climb. Up and down on that overlap climb, including where bit 0 is set as well. Down on bit 0 with no bit 2 ducks.

| AND nibble | Flags | Result |
| --- | --- | --- |
| 1, 4, or 5 | bit 0 | floor. The roof dots are nibble 5 |
| 6 or 9 | bits 0 and 1 | floor. Bit 1 is not a Bruce floor test |
| 2, 8, or 10 | bit 1 | neither. He falls through these |
| 7 or 13 | bit 0, plus bit 3 or bit 2 | floor. A wall on property 0, a climb on any other property |
| 3, 11, 12, 14, or 15 | bit 3 or bit 2 | a wall on property 0, a climb otherwise. Not a floor |
| anything else | none | ignored |

A pixel past the edge of the 320 by 200 screen counts as a wall.

The roof tile `$2A` is `00 00 00 00 55 55 FF FF` on its shadow half. `$55` is nibble 5, so a fall steps onto that first dotted row and stays. The brown row under the sole stays visible. `$FF` is nibble 15. On a property-6 cell that is a climb, which is the black line capping the hole, so up and down there climb. On property 0 the same line is a wall and blocks a sideways step. The floor tile `$11` is `00 00 55 55 AA AA FF FF`. `$AA` is nibble 10, so he falls through it and stops on the `$55`. The statue's other patterns are not in the table, so he runs through that art.

The ladder rails are climb nibbles on property 6, so overlapping one sticks and does not push him out. He climbs through the rails and walks off the top. The same test is what makes the property-6 roof line climbable. A climb burst does not stop on the floor underneath; the next logic frame sees the ladder bit clear and the move ends.
