// Bruce Lee, a Windows port.
// Copyright (C) 2026 Jacob Marner
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// No copyright is claimed over the original game, which is copyright 1984 Datasoft, Inc.

#pragma once

#include <cstdint>

constexpr int kRoomCount = 20;

// Load room 0..19. The screen, the colors, and the charset are the expanded
// bytes described in referencematerial/asm/rooms.asm. Lamps already taken in this room are
// painted as the bare mount. Returns false when a data file is missing.
// Asking for room 11 loads room 13 once rooms 12, 14, and 16 are clear.
bool EnterRoom(int roomId);

// Put roomId's picture in place of the one on screen. RestoreRoomPicture puts
// the previous picture back. No score, sound, or enter hook.
bool ShowRoomPicture(int roomId);
void RestoreRoomPicture();

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

// The waiting player's copy of the lamp counts and taken flags. Snapshot
// copies the live tables. Between lives, L_0FED exchanges them.
void CopyCollectedLamps();
void ExchangeCollectedLamps();

// clear_work_ram's visit flags. The next room entry arms them again.
void ResetRoomBonus();

// $10E0. The treasure ending sets this player's room-entry flags, then
// clears room 0 so the opening room does not pay again.
void RearmRoomBonus();

// clear_work_ram and the hardware init that follows game over. Difficulty,
// the room 16 lanterns, the scroll, and the visit clock go back to a cold start.
void ResetRun();

// $40 sits in the block between_lives swaps. The waiting player's copy is
// the difficulty he will meet on his next life.
void CopyDifficulty();
void ExchangeDifficulty();

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

// Room 19, from step_actors at $17C4. Call once per logic frame, before the
// stick is used. irq_service has already counted this frame's three rasters
// into $87, and $88 steps each time that byte wraps. Below 2 the stick is
// $0E, up. From 2 on it is $0F, no direction. The fire button is not touched.
int Room19Stick();

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
