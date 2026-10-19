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

// Room 5, the hook at $38AB. $71/$77 and $72/$78 slide the two beams under
// the middle gap. $73/$79 slides the beam at the left of the upper level.
// The enter hook at $343E reloads all six. A timer is a byte, so $E0 is -32.
int8_t beamColA = 0x1B;
int8_t beamColB = 0x1B;
int8_t beamColC = 0x01;
int8_t beamTimerA = static_cast<int8_t>(0xFF);
int8_t beamTimerB = static_cast<int8_t>(0xFC);
int8_t beamTimerC = static_cast<int8_t>(0xF8);

// One print_string frame. $4086 adds $1F to the accumulator, and $40B4 adds
// $07 before that. Tiles are the screen codes minus $20, up to the $A0 stop.
struct BeamTiles {
    uint8_t count;
    uint8_t a;
    uint8_t b;
};

const BeamTiles kBeamAB[7] = {
    {1, 0x56, 0x00},
    {1, 0x55, 0x00},
    {2, 0x55, 0x56},
    {2, 0x54, 0x55},
    {2, 0x00, 0x55},
    {2, 0x00, 0x54},
    {2, 0x00, 0x00},
};

const BeamTiles kBeamC[7] = {
    {1, 0x5A, 0x00},
    {1, 0x59, 0x00},
    {2, 0x59, 0x5A},
    {2, 0x58, 0x59},
    {2, 0x00, 0x59},
    {2, 0x00, 0x58},
    {2, 0x00, 0x00},
};

void StampBeam(const BeamTiles& frame, int col, int tileRow) {
    const uint8_t tiles[2] = {frame.a, frame.b};
    for (int i = 0; i < frame.count; ++i) {
        WriteTile(col + i, tileRow, tiles[i]);
    }
}

// $3915, $3944, and $3978. Count 3 steps the beam one column when it is
// still left of the limit: erase with accumulator 6, then draw accumulator 2
// at the new column. Count 7 snaps the column back and reloads the pause.
void AdvanceBeam(int8_t& timer, int8_t& col, int limit, int8_t resetTimer, int8_t resetCol,
                 int tileRow, const BeamTiles* frames) {
    timer = static_cast<int8_t>(timer + 1);
    if (timer < 0) {
        return;
    }
    int frame = timer;
    if (timer == 3) {
        if (col < limit) {
            StampBeam(frames[6], col, tileRow);
            col = static_cast<int8_t>(col + 1);
            timer = 2;
            frame = 2;
        } else {
            frame = 3;
        }
    } else if (timer == 7) {
        timer = resetTimer;
        col = resetCol;
        return;
    }
    StampBeam(frames[frame], col, tileRow);
}

void AdvanceRoom5Beams() {
    // $4D99 is 1 in the opening snapshot and the loaded code never stores it,
    // so the second beam runs along with the other two.
    AdvanceBeam(beamTimerA, beamColA, 0x1C, static_cast<int8_t>(0xE0), 0x1B, 9, kBeamAB);
    AdvanceBeam(beamTimerB, beamColB, 0x1C, static_cast<int8_t>(0xE0), 0x1B, 10, kBeamAB);
    AdvanceBeam(beamTimerC, beamColC, 0x02, static_cast<int8_t>(0xE2), 0x01, 5, kBeamC);
}

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
        WriteTile(lamp.col, lamp.row, lamp.tile);
    }
    // Room 1's after-lamp hook at $33F6. The closed door is what the packed
    // stream just drew, and the enter hook opens it again from this count.
    if (roomId == 1) {
        trapTimer = 3;
    }
    if (roomId == 5) {
        beamColA = 0x1B;
        beamColB = 0x1B;
        beamColC = 0x01;
        beamTimerA = static_cast<int8_t>(0xFF);
        beamTimerB = static_cast<int8_t>(0xFC);
        beamTimerC = static_cast<int8_t>(0xF8);
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

void WriteTile(int col, int tileRow, int tile) {
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

// Room 1, the hook at $3679. The door opens once rooms 0, 1, and 2 have no
// lamps left and room 6 still has one. $7B has already flipped for this
// frame, and the hook runs only when that byte is 0.
void AdvanceTrapDoor() {
    if (hookGate != 0 || trapTimer < 0) {
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
        WriteTile(0x11 + i, 0x0C, picture[phase][i]);
    }
}

void AdvanceRoom() {
    // $1BFE. A stored 0 becomes 1. A stored 1 becomes 0, which is the frame
    // a hook that shares $7B is allowed to run.
    if (hookGate == 0) {
        hookGate = 1;
    } else {
        hookGate -= 1;
    }
    // The animation for the room he is standing in.
    switch (g.roomId) {
    case 1:
        AdvanceTrapDoor();
        break;
    case 5:
        // $38AB ignores $7B, so the beams move on every logic frame.
        AdvanceRoom5Beams();
        break;
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
