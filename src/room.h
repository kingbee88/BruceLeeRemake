#pragma once

#include <cstdint>

constexpr int kRoomCount = 20;

// Load room 0..19. The screen, the colors, and the charset are the expanded
// bytes described in referencematerial/asm/rooms.asm. Lamps already taken in this room are
// painted as the bare mount. Returns false when a data file is missing.
bool EnterRoom(int roomId);

// wake_actor's cold-start position for this room, in screen pixels.
// g.x = xpos * 2 - 24 and g.y = ypos - 21.
void RoomSpawn(int roomId, int& x, int& y);

// Character row of a tile row. Tile row 2 is character row 3. Each tile row
// after that is the picture row and the shadow row under it.
inline int PictureRow(int tileRow) {
    return 3 + (tileRow - 2) * 2;
}

// The tile on this column and tile row, with the shadow bit cleared.
// -1 when the cell is outside the playfield.
int TileAt(int col, int tileRow);

// Write a tile on both rows of the pair. Lamps, beams, and the trap door
// all paint through this.
void WriteTile(int col, int tileRow, int tile);

// Write one byte on the picture row only. The shadow row is left alone.
void WritePicture(int col, int tileRow, int tile);

// $4CE2 flag. True while that lamp is still in the room. A taken flag is 0.
bool LampPresent(int address);

// $2A + room. Lamps still to take in that room.
int LampsLeft(int room);

// copy_room_defaults and L_33CA. A finished game puts the lamps back.
void ResetCollectedLamps();

// True when (col, row) is an untaken lamp in the current room. The mount
// tile is returned and the lamp stays taken if the room is entered again.
bool ClaimLamp(int col, int row, int& mount);

// Tiles 2, 3, 4, and 5, from $2E89. left, right, and up are the steps taken
// since the previous check. falling is state $0E. On success the new room
// is loaded and g.x / g.y are the arrival spot.
bool TakeExit(int tile, int xpos, int ypos, bool left, bool right, bool up, bool falling);

// One logic frame of the hook at $3A6F. $1BFE flips $7B first. Doors, beams,
// the scrolling floor, and the boss run here, before the special scan.
// True means the hook moved him to another room, the way an exit does.
bool AdvanceRoom();

// The enter hook at $4E9C, after taken lamps are painted.
void BeginRoomEffects();

// match_special for the tiles that are not lamps or doors. prize is hit bit
// 1. ladder is hit bit 2. A beam or a hot floor sets the shock state.
void TouchRoomTile(int tile, int col, int row, bool prize, bool ladder);

// $60 == 1 shoves him with the scrolling floor. -1 is up, +1 is down.
int SurfaceShove();

// Room 18's two bolts. False when that bolt is off. shape is 21 rows of 3
// bytes, the sprite at $8340 / $8380.
bool BossSparkAt(int index, int& x, int& y, const uint8_t*& shape);
