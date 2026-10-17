#include "room.h"

#include "game.h"
#include "load.h"

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

// $7B. frame flips this before the room hook, so the hook runs every other
// logic frame. $4E97 is the trap-door step in room 1. The after-lamp hook
// stores 3, and $FF means the four pictures have been painted.
int hookGate = 0;
int trapTimer = 3;

int TopRow(int tileRow) {
    // Tile row 2 is character row 3. Each tile row after that is two
    // character rows, the picture and its shadow.
    return 3 + (tileRow - 2) * 2;
}

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

bool EnterRoom(int roomId) {
    if (roomId < 0 || roomId >= kRoomCount) {
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
        ReplaceLamp(lamp.col, lamp.row, lamp.tile);
    }
    // Room 1's after-lamp hook at $33F6. The closed door is what the packed
    // stream just drew, and the enter hook opens it again from this count.
    if (roomId == 1) {
        trapTimer = 3;
    }
    return true;
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
    const int top = TopRow(tileRow) * 40 + col;
    return g.room[static_cast<size_t>(top)] & 0x7F;
}

void ReplaceLamp(int col, int tileRow, int tile) {
    if (col < 0 || col >= 40 || tileRow < 2 || tileRow > 12) {
        return;
    }
    const int top = TopRow(tileRow) * 40 + col;
    g.room[static_cast<size_t>(top)] = static_cast<uint8_t>(tile);
    g.room[static_cast<size_t>(top + 40)] = static_cast<uint8_t>(tile);
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

void AdvanceTrapDoor() {
    // The flip at $1BFE. A stored 0 becomes 1 and this frame's hook sees 1.
    // A stored 1 becomes 0 and the hook runs.
    if (hookGate == 0) {
        hookGate = 1;
    } else {
        hookGate -= 1;
    }
    if (g.roomId != 1 || hookGate != 0 || trapTimer < 0) {
        return;
    }
    // $3682. Room 6's count has to be nonzero, and rooms 0, 1, and 2 empty.
    if (lampLeft[6] == 0 || lampLeft[0] != 0 || lampLeft[1] != 0 || lampLeft[2] != 0) {
        return;
    }
    trapTimer -= 1;
    // print_string of $12, $11, $10, then $0F. Each is five tiles at column
    // $11 of tile row $0C. $424F plays effect $8A on the first step. The
    // screen codes minus $20, with bit 7 off, are the tiles below.
    static const uint8_t picture[4][5] = {
        {0x11, 0x5C, 0x5D, 0x5E, 0x5F},
        {0x11, 0x22, 0x22, 0x03, 0x03},
        {0x5C, 0x5D, 0x5E, 0x03, 0x03},
        {0x22, 0x22, 0x03, 0x03, 0x03},
    };
    const int phase = trapTimer < 0 ? 3 : 2 - trapTimer;
    for (int i = 0; i < 5; ++i) {
        ReplaceLamp(0x11 + i, 0x0C, picture[phase][i]);
    }
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
        g.x = destX * 2 - 24;
        g.y = destY - 21;
        return true;
    }
    return false;
}
