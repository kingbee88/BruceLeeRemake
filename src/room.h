#pragma once

constexpr int kRoomCount = 20;

// Load room 0..19. The screen, the colors, and the charset are the expanded
// bytes described in referencematerial/asm/rooms.asm. Lamps already taken in this room are
// painted as the bare mount. Returns false when a data file is missing.
bool EnterRoom(int roomId);

// wake_actor's cold-start position for this room, in screen pixels.
// g.x = xpos * 2 - 24 and g.y = ypos - 21.
void RoomSpawn(int roomId, int& x, int& y);

// The tile on this column and tile row, with the shadow bit cleared.
// -1 when the cell is outside the playfield.
int TileAt(int col, int tileRow);

// Write a tile on both rows of the pair. Lamps, beams, and the trap door
// all paint through this.
void WriteTile(int col, int tileRow, int tile);

// True when (col, row) is an untaken lamp in the current room. The mount
// tile is returned and the lamp stays taken if the room is entered again.
bool ClaimLamp(int col, int row, int& mount);

// Tiles 2, 3, 4, and 5, from $2E89. left, right, and up are the steps taken
// since the previous check. falling is state $0E. On success the new room
// is loaded and g.x / g.y are the arrival spot.
bool TakeExit(int tile, int xpos, int ypos, bool left, bool right, bool up, bool falling);

// One logic frame of the hook at $3A6F for the room he is in. $1BFE flips
// $7B first. Room 1 steps its trap door on the frames that flip allows.
// Room 5 slides its three beams on every frame. Call it before the special
// scan.
void AdvanceRoom();
