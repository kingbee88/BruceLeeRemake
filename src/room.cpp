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

#include "room.h"

#include "draw.h"
#include "game.h"
#include "load.h"
#include "sound.h"

#include <cstdint>
#include <cwchar>

namespace {

struct RoomLamp {
    uint8_t col;
    uint8_t row;
    uint8_t tile;
};

struct RoomExit {
    uint8_t room;
    uint8_t xpos;
    uint8_t ypos;
};

struct RoomInfo {
    uint8_t charset;
    uint8_t spawnX;
    uint8_t spawnY;
    uint8_t bands[12];
    uint8_t splits[4];
    uint8_t lampCount;
    RoomLamp lamps[13];
    RoomExit exits[4];
};

#include "roomdata.inc"

// One flag per record in the room's list at $4CE2. Cleared for a new game.
bool lampTaken[kRoomCount][13] = {};

// $4CCE, copied to $2A. claim_lamp decrements the byte for the room in $29.
// $2A, $2B, and $2C are rooms 0, 1, and 2. $30 is room 6.
uint8_t lampLeft[kRoomCount] = {
    0x06, 0x06, 0x0A, 0x02, 0x02, 0x0A, 0x08, 0x0D, 0x04, 0x05,
    0x03, 0x03, 0x03, 0x00, 0x04, 0x06, 0x02, 0x02, 0x01, 0x00,
};

const uint8_t kLampLeft0[kRoomCount] = {
    0x06, 0x06, 0x0A, 0x02, 0x02, 0x0A, 0x08, 0x0D, 0x04, 0x05,
    0x03, 0x03, 0x03, 0x00, 0x04, 0x06, 0x02, 0x02, 0x01, 0x00,
};

// First flag byte of each room's list at $4CE2.
const int kLampAt[kRoomCount] = {
    0x4D0C, 0x4D25, 0x4D3E, 0x4D67, 0x4D70, 0x4D79, 0x4DA2, 0x4DC3,
    0x4DF8, 0x4E09, 0x4E1E, 0x4E2B, 0x4E38, 0x4E45, 0x4E4A, 0x4E5B,
    0x4E74, 0x4E7D, 0x4E86, 0x4E8B,
};

// $30D4. Carry clear there is an open exit. Room 11's doors sit in the
// middle of the screen, so that room does not ask for an edge.
bool EdgeOpen(int xpos, int ypos, bool left, bool right, bool up, bool falling) {
    if (g.roomId == 0x0B) {
        return true;
    }
    if (xpos < 0x10 && left) {
        return true;
    }
    if (xpos > 0xA1 && right) {
        return true;
    }
    if (ypos > 0xC5 && falling) {
        return true;
    }
    if (ypos < 0x31 && up) {
        return true;
    }
    return false;
}

}  // namespace

// $0300. clear_work_ram stores 1 in forty bytes, then clears both players'
// flags for room 0. Player 0 is the even byte of each pair.
static uint8_t roomBonus[40] = {};
static bool roomBonusLive = false;

static void ArmRoomBonus() {
    if (roomBonusLive) {
        return;
    }
    roomBonusLive = true;
    for (int i = 0; i < 40; ++i) {
        roomBonus[i] = 1;
    }
    roomBonus[0] = 0;
    roomBonus[1] = 0;
}

// $0A14. A set flag pays $07 into score byte 1 and $D0 into byte 2, which
// is 2000, and the shift spends the flag. Room 0's flag stays clear.
static void PayRoomBonus(int roomId) {
    ArmRoomBonus();
    if (roomId < 0 || roomId >= kRoomCount) {
        return;
    }
    const int index = roomId * 2;
    if (roomBonus[index] == 0) {
        return;
    }
    roomBonus[index] = static_cast<uint8_t>(roomBonus[index] >> 1);
    AddScoreByte(1, 0x07);
    AddScoreByte(2, 0xD0);
}

void RearmRoomBonus() {
    roomBonusLive = true;
    for (int i = 38; i >= 0; i -= 2) {
        roomBonus[i] = 1;
    }
    roomBonus[0] = 0;
}

bool EnterRoom(int roomId) {
    if (roomId < 0 || roomId >= kRoomCount) {
        return false;
    }
    // $09F6. Entering room 11 with $36, $38, and $3A all zero opens room
    // 13 instead. Those bytes are the lamps left in rooms 12, 14, and 16.
    // enter_room_at then places him on room 13's spawn.
    if (roomId == 0x0B && LampsLeft(12) == 0 && LampsLeft(14) == 0 && LampsLeft(16) == 0) {
        roomId = 0x0D;
    }
    const RoomInfo& info = kRooms[roomId];
    const wchar_t* charsetName = L"charset.bin";
    if (info.charset == 1) {
        charsetName = L"charset1.bin";
    } else if (info.charset == 2) {
        charsetName = L"charset2.bin";
    }
    wchar_t roomName[32];
    wchar_t colorName[32];
    swprintf_s(roomName, L"room%d.bin", roomId);
    swprintf_s(colorName, L"room%d.col", roomId);
    std::vector<uint8_t> charset = LoadFile(charsetName, 2048);
    std::vector<uint8_t> screen = LoadFile(roomName, 1000);
    std::vector<uint8_t> color = LoadFile(colorName, 1000);
    if (charset.empty() || screen.empty() || color.empty()) {
        return false;
    }
    // $09F3. Entering a room clears the effect slots and the SID.
    SilenceSound();
    g.charset = std::move(charset);
    g.room = std::move(screen);
    g.color = std::move(color);
    g.roomId = roomId;
    g.charsetId = info.charset;
    for (int i = 0; i < 12; ++i) {
        g.bands[i] = info.bands[i];
    }
    for (int i = 0; i < 4; ++i) {
        g.splits[i] = info.splits[i];
    }
    for (int i = 0; i < info.lampCount; ++i) {
        if (!lampTaken[roomId][i]) {
            continue;
        }
        const RoomLamp& lamp = info.lamps[i];
        WriteTile(lamp.col, lamp.row, lamp.tile);
    }
    // Taken lamps are painted, then the enter hook at $4E9C. The packed
    // picture is still the closed door, the unlit beam, and the full floor.
    BeginRoomEffects();
    // enter_room_body pays for the room that was loaded. Room 11's redirect
    // has already replaced that with room 13, so room 11 is not paid then.
    PayRoomBonus(g.roomId);
    // $09D2 writes screen code $20 across row 0, the solid black bar.
    ClearCreditLine();
    return true;
}

// The picture ShowRoomPicture replaced. One level deep is enough: the welcome
// shot borrows a room and puts this back before the game runs.
struct HeldPicture {
    std::vector<uint8_t> charset;
    std::vector<uint8_t> room;
    std::vector<uint8_t> color;
    int roomId = 0;
    int charsetId = 0;
    uint8_t bands[12] = {};
    uint8_t splits[4] = {};
};

HeldPicture gHeldPicture;
bool gPictureHeld = false;

bool ShowRoomPicture(int roomId) {
    if (gPictureHeld || roomId < 0 || roomId >= kRoomCount) {
        return false;
    }
    const RoomInfo& info = kRooms[roomId];
    const wchar_t* charsetName = L"charset.bin";
    if (info.charset == 1) {
        charsetName = L"charset1.bin";
    } else if (info.charset == 2) {
        charsetName = L"charset2.bin";
    }
    wchar_t roomName[32];
    wchar_t colorName[32];
    swprintf_s(roomName, L"room%d.bin", roomId);
    swprintf_s(colorName, L"room%d.col", roomId);
    std::vector<uint8_t> charset = LoadFile(charsetName, 2048);
    std::vector<uint8_t> screen = LoadFile(roomName, 1000);
    std::vector<uint8_t> color = LoadFile(colorName, 1000);
    if (charset.empty() || screen.empty() || color.empty()) {
        return false;
    }
    gHeldPicture.charset = std::move(g.charset);
    gHeldPicture.room = std::move(g.room);
    gHeldPicture.color = std::move(g.color);
    gHeldPicture.roomId = g.roomId;
    gHeldPicture.charsetId = g.charsetId;
    for (int i = 0; i < 12; ++i) {
        gHeldPicture.bands[i] = g.bands[i];
        g.bands[i] = info.bands[i];
    }
    for (int i = 0; i < 4; ++i) {
        gHeldPicture.splits[i] = g.splits[i];
        g.splits[i] = info.splits[i];
    }
    g.charset = std::move(charset);
    g.room = std::move(screen);
    g.color = std::move(color);
    g.roomId = roomId;
    g.charsetId = info.charset;
    gPictureHeld = true;
    return true;
}

void RestoreRoomPicture() {
    if (!gPictureHeld) {
        return;
    }
    g.charset = std::move(gHeldPicture.charset);
    g.room = std::move(gHeldPicture.room);
    g.color = std::move(gHeldPicture.color);
    g.roomId = gHeldPicture.roomId;
    g.charsetId = gHeldPicture.charsetId;
    for (int i = 0; i < 12; ++i) {
        g.bands[i] = gHeldPicture.bands[i];
    }
    for (int i = 0; i < 4; ++i) {
        g.splits[i] = gHeldPicture.splits[i];
    }
    gPictureHeld = false;
}

void RoomSpawn(int roomId, int& x, int& y) {
    const RoomInfo& info = kRooms[roomId];
    x = info.spawnX * 2 - 24;
    y = info.spawnY - 21;
}

int TileAt(int col, int tileRow) {
    if (col < 0 || col >= 40 || tileRow < 2 || tileRow > 12) {
        return -1;
    }
    const int top = PictureRow(tileRow) * 40 + col;
    return g.room[static_cast<size_t>(top)] & 0x7F;
}

void WriteTile(int col, int tileRow, int tile) {
    if (col < 0 || col >= 40 || tileRow < 2 || tileRow > 12) {
        return;
    }
    const int top = PictureRow(tileRow) * 40 + col;
    g.room[static_cast<size_t>(top)] = static_cast<uint8_t>(tile);
    g.room[static_cast<size_t>(top + 40)] = static_cast<uint8_t>(tile);
}

void WritePicture(int col, int tileRow, int tile) {
    if (col < 0 || col >= 40 || tileRow < 2 || tileRow > 12) {
        return;
    }
    const int top = PictureRow(tileRow) * 40 + col;
    g.room[static_cast<size_t>(top)] = static_cast<uint8_t>(tile);
}

bool LampPresent(int address) {
    for (int room = 0; room < kRoomCount; ++room) {
        const int base = kLampAt[room];
        const int count = kRooms[room].lampCount;
        if (address < base || address >= base + count * 4) {
            continue;
        }
        const int delta = address - base;
        if (delta % 4 != 0) {
            return true;
        }
        return !lampTaken[room][delta / 4];
    }
    return true;
}

int LampsLeft(int room) {
    if (room < 0 || room >= kRoomCount) {
        return 0;
    }
    return lampLeft[room];
}

void ResetRoomBonus() {
    roomBonusLive = false;
}

void ResetCollectedLamps() {
    for (int room = 0; room < kRoomCount; ++room) {
        lampLeft[room] = kLampLeft0[room];
        for (int i = 0; i < 13; ++i) {
            lampTaken[room][i] = false;
        }
    }
}

bool ClaimLamp(int col, int row, int& mount) {
    const RoomInfo& info = kRooms[g.roomId];
    for (int i = 0; i < info.lampCount; ++i) {
        const RoomLamp& lamp = info.lamps[i];
        if (lamp.col != col || lamp.row != row) {
            continue;
        }
        if (lampTaken[g.roomId][i]) {
            return false;
        }
        lampTaken[g.roomId][i] = true;
        if (lampLeft[g.roomId] > 0) {
            lampLeft[g.roomId] -= 1;
        }
        mount = lamp.tile;
        return true;
    }
    return false;
}

bool TakeExit(int tile, int xpos, int ypos, bool left, bool right, bool up, bool falling) {
    if (tile < 2 || tile > 5) {
        return false;
    }
    const RoomInfo& info = kRooms[g.roomId];
    for (int slot = tile - 2; slot < 4; ++slot) {
        if (!EdgeOpen(xpos, ypos, left, right, up, falling)) {
            return false;
        }
        const RoomExit& exit = info.exits[slot];
        // $2F4F. A zero xpos tries the next tile's slot. Tile 5 has no such
        // test. A zero xpos is the unused record, and every real arrival in
        // the tables has a nonzero xpos, so the fourth slot is skipped too.
        if (exit.xpos == 0) {
            if (slot < 3) {
                continue;
            }
            return false;
        }
        const int dest = exit.room;
        const int destX = exit.xpos;
        const int destY = exit.ypos;
        if (!EnterRoom(dest)) {
            return false;
        }
        // $3217. Room 11's redirect has already loaded room 13, and that
        // arrival is the spawn, not this exit's xpos and ypos.
        if (g.roomId != dest) {
            RoomSpawn(g.roomId, g.x, g.y);
        } else {
            g.x = destX * 2 - 24;
            g.y = destY - 21;
        }
        return true;
    }
    return false;
}
