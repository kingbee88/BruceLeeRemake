#include "room.h"

#include "bruce.h"
#include "game.h"

#include <cstdint>

// The room hook at $3A6F, and the enter hook at $4E9C. Each room in the
// original calls print_string, slides a beam, or rewrites a glyph. The
// addresses in the comments are the 6502 hooks this follows.

namespace {

#include "strings.inc"

// $4E97 through $4E9B. The enter hook clears them, then stores its own.
uint8_t t97 = 0;
uint8_t t98 = 0;
uint8_t t99 = 0;
uint8_t t9A = 0;
uint8_t t9B = 0;

// $7B. frame flips this before the hook. A hook that tests it runs when
// the stored value was 0, which is the value 0 after the flip.
int hookGate = 0;

// $71..$79. Beams and the sliding hazard keep these across a room change.
int8_t col71 = 0;
int8_t col72 = 0;
int8_t col73 = 0;
int row74 = 0;
int row75 = 0;
int row76 = 0;
int8_t tim77 = 0;
int8_t tim78 = 0;
int8_t tim79 = 0;
uint8_t slideCol = 0;
uint8_t slideRow = 0;

// $60..$66 and $4321. The scrolling floor in rooms 4, 5, 6, 8, 9, and 11.
// The opening snapshot starts on the first step.
uint8_t surfaceWait = 1;
uint8_t surfaceStep = 0;
uint8_t surfaceDown = 0;
uint8_t surfaceCount = 1;
uint8_t surfaceSlot = 7;
// Last step written into each charset bank. -1 means that bank is still
// the file image. Entering a room reloads the file, so the bank is painted
// back to this step before the next scroll.
int bankStep[3] = {-1, -1, -1};

// $4FDE. Three crumble slots. A count of 0 is free.
struct Crumble {
    uint8_t n = 0;
    uint8_t col = 0;
    uint8_t row = 0;
};
Crumble crumb[3];

// $4E95. Room 16 spends one per matching cell. It is not put back on a win.
int extraLives = 5;

// $40. A finished game increments it. Room 13's movers read it.
int difficulty = 0;

// $014B. Eight $01 bytes. Player 0 uses the first three height bands.
uint8_t bandBonus[8] = {1, 1, 1, 1, 1, 1, 1, 1};

// $010F..$014A. Nine movers. Room 13 fills them on entry.
struct Mover {
    uint8_t active = 0;
    uint8_t col = 0;
    uint8_t row = 0;
    uint8_t low = 0;
    uint8_t high = 0;
};
Mover mover[9];

// Room 18. Two sparks, sprites 6 and 7. phase 0 waits, then the bolt falls.
uint8_t boltX[2] = {0x50, 0x5E};
uint8_t boltY[2] = {0x37, 0x37};
uint8_t boltPhase[2] = {};
uint8_t boltDelay[2] = {0, 0x10};
bool boltOn[2] = {};
uint8_t boltShape[2][63] = {};

// $49/$4A. Room 19 counts 1112 raster ticks. A logic frame is three of them.
int victory = 0;

// $87/$88. enter_room stores 0 in both. irq_service adds one to $87 on every
// raster, and adds one to $88 when $87 wraps, until bit 7 of $88 is set.
// step_actors reads $88 in room 19.
int play87 = 0;
int play88 = 0;

// Set when the hook itself changes the room, so Tick does not also move him.
bool relocated = false;

const uint8_t kSurfaceLow[24] = {
    0x0C, 0x33, 0x03, 0xCC, 0x03, 0x33, 0x0C, 0xC3,
    0xCC, 0xFF, 0x33, 0xFF, 0x3C, 0xFF, 0xF3, 0x3F,
    0x30, 0xCC, 0xC0, 0x33, 0xC0, 0xCC, 0x30, 0xC3,
};
const uint8_t kSurfaceHigh[24] = {
    0xC3, 0x30, 0x3F, 0x30, 0xC3, 0x00, 0xFF, 0x00,
    0xC3, 0xFF, 0x3C, 0xFF, 0xC3, 0xFF, 0x3C, 0xFF,
    0xC3, 0x0C, 0xFC, 0x0C, 0xC3, 0x00, 0xFF, 0x00,
};
const uint8_t kSurfaceDelay[8] = {0x84, 0x08, 0x06, 0x04, 0x02, 0x04, 0x06, 0x08};
const uint8_t kBoltMask[4] = {0x18, 0x3C, 0x3C, 0x18};

// $40 == 0, then $40 != 0. hook_enter_13 stores these nine slots.
const Mover kEasy[9] = {
    {0x02, 0x21, 0x07, 0x14, 0x21},
    {0x02, 0x13, 0x0A, 0x06, 0x13},
    {0x02, 0x02, 0x0C, 0x02, 0x21},
    {0x00, 0x00, 0x00, 0x00, 0x00},
    {0x02, 0x16, 0x0C, 0x02, 0x21},
    {0x82, 0x06, 0x05, 0x06, 0x22},
    {0x82, 0x16, 0x05, 0x06, 0x22},
    {0x82, 0x06, 0x07, 0x06, 0x14},
    {0x82, 0x14, 0x0A, 0x14, 0x22},
};
const Mover kHard[9] = {
    {0x02, 0x21, 0x07, 0x12, 0x21},
    {0x02, 0x15, 0x0A, 0x06, 0x15},
    {0x02, 0x02, 0x0C, 0x02, 0x25},
    {0x00, 0x00, 0x00, 0x00, 0x00},
    {0x02, 0x16, 0x0C, 0x02, 0x25},
    {0x82, 0x06, 0x05, 0x06, 0x26},
    {0x82, 0x16, 0x05, 0x06, 0x26},
    {0x82, 0x02, 0x07, 0x02, 0x12},
    {0x82, 0x16, 0x0A, 0x16, 0x26},
};

int8_t Byte(int value) {
    return static_cast<int8_t>(value);
}

uint8_t Noise() {
    static uint32_t state = 0xC64A71u;
    state = state * 1664525u + 1013904223u;
    return static_cast<uint8_t>(state >> 24);
}

void AddScore(int points) {
    g.score += points;
    if (g.score > 999999) {
        g.score = 999999;
    }
}

void PrintString(int id, int row, int col) {
    if (id < 0 || id >= 0x65) {
        return;
    }
    const uint8_t* text = kStrBytes + kStrOff[id];
    const int count = kStrLen[id];
    for (int i = 0; i < count; ++i) {
        WriteTile(col + i, row, text[i]);
    }
}

void PaintColor(int offset, int color) {
    if (offset < 0 || offset >= 1000) {
        return;
    }
    g.color[static_cast<size_t>(offset)] = static_cast<uint8_t>(color);
}

// A one-shot that starts at 0 fires once. One that starts at 1 fires twice.
// The test is BMI on the result, so $FF stops it.
bool Arm(uint8_t& timer) {
    if (timer & 0x80) {
        return false;
    }
    timer = static_cast<uint8_t>(timer - 1);
    return true;
}

// $3A8A. Three rows: blank, blank, and the lintel.
void OpenDoor(int row, int col) {
    PrintString(0x13, row, col);
    PrintString(0x13, row + 1, col);
    PrintString(0x14, row + 2, col);
}

// $3AA0 and $3AAA. The third row is then a solid tile.
void OpenDoorSolid(int row, int col, int third) {
    OpenDoor(row, col);
    PrintString(third, row + 2, col);
}

// $3AB4. The later charset draws its door from tiles $07 and $0E.
void OpenLateDoor(int row, int col) {
    PrintString(0x63, row, col);
    PrintString(0x63, row + 1, col);
    PrintString(0x64, row + 2, col);
}

// $4086. base $1F is the first beam. base $26 is the beam that adds 7 first.
void AdvanceBeam(int8_t& timer, int8_t& col, int limit, int8_t resetTimer, int8_t resetCol,
                 int row, int base) {
    timer = Byte(timer + 1);
    if (timer < 0) {
        return;
    }
    int frame = timer;
    if (timer == 3) {
        if (col < limit) {
            PrintString(base + 6, row, col);
            col = Byte(col + 1);
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
    PrintString(base + frame, row, col);
}

// $404E writes tile $65 behind and $64 ahead. $4063 writes $67 and $66.
void PaintSlide(int row, int oldCol, int newCol, bool heavy) {
    PrintString(heavy ? 0x55 : 0x1C, row, oldCol);
    PrintString(heavy ? 0x54 : 0x1B, row, newCol);
}

void StepSlide(bool right, int limit, int wrap, bool heavy) {
    const int oldCol = slideCol;
    if (right) {
        slideCol = static_cast<uint8_t>(slideCol + 1);
        if (oldCol >= limit) {
            slideCol = static_cast<uint8_t>(wrap);
        }
    } else {
        slideCol = static_cast<uint8_t>(slideCol - 1);
        if (oldCol < limit) {
            slideCol = static_cast<uint8_t>(wrap);
        }
    }
    PaintSlide(slideRow, oldCol, slideCol, heavy);
}

// $2C70. Each pattern byte is stored twice, filling glyph $61 and its
// shadow $E1, then $62/$E2 and $63/$E3. A wait of $84 keeps bit 7 set and
// does not scroll on the short counts.
void CopySurfaceGlyphs() {
    const uint8_t* pattern = g.roomId < 10 ? kSurfaceLow : kSurfaceHigh;
    const int step = surfaceStep & 7;
    for (int glyph = 0; glyph < 3; ++glyph) {
        const int picture = (0x61 + glyph) * 8;
        const int shadow = (0xE1 + glyph) * 8;
        if (shadow + 8 > static_cast<int>(g.charset.size())) {
            continue;
        }
        for (int i = 0; i < 4; ++i) {
            const uint8_t low = pattern[glyph * 8 + ((step + i) & 7)];
            const uint8_t high = pattern[glyph * 8 + ((step + 4 + i) & 7)];
            g.charset[static_cast<size_t>(picture + i * 2)] = low;
            g.charset[static_cast<size_t>(picture + i * 2 + 1)] = low;
            g.charset[static_cast<size_t>(shadow + i * 2)] = high;
            g.charset[static_cast<size_t>(shadow + i * 2 + 1)] = high;
        }
    }
    if (g.charsetId >= 0 && g.charsetId < 3) {
        bankStep[g.charsetId] = step;
    }
}

// The packed charset is the step-0 picture. A bank that has already scrolled
// is put back on the step it was showing, without advancing $64.
void RestoreSurfaceGlyphs() {
    if (g.charsetId < 0 || g.charsetId > 2 || bankStep[g.charsetId] < 0) {
        return;
    }
    const uint8_t saved = surfaceStep;
    surfaceStep = static_cast<uint8_t>(bankStep[g.charsetId]);
    CopySurfaceGlyphs();
    surfaceStep = saved;
}

void AdvanceSurfaceSlot() {
    surfaceSlot = static_cast<uint8_t>(surfaceSlot - 1);
    if (surfaceSlot & 0x80) {
        surfaceSlot = 7;
        surfaceDown ^= 1;
    }
    surfaceWait = kSurfaceDelay[surfaceSlot];
    surfaceCount = 0x10;
}

void StepSurfaceIndex() {
    if (surfaceDown == 0) {
        surfaceStep = static_cast<uint8_t>(surfaceStep + 1);
        if (surfaceStep >= 8) {
            surfaceStep = 0;
        }
    } else {
        surfaceStep = static_cast<uint8_t>(surfaceStep - 1);
        if (surfaceStep & 0x80) {
            surfaceStep = 7;
        }
    }
}

void AnimateSurface() {
    if (surfaceWait & 0x80) {
        const uint8_t low = static_cast<uint8_t>((surfaceWait & 0x7F) - 1);
        if (low != 0) {
            surfaceWait = static_cast<uint8_t>(low | 0x80);
            return;
        }
        surfaceWait = kSurfaceDelay[surfaceSlot];
        surfaceCount = static_cast<uint8_t>(surfaceCount - 1);
        if ((surfaceCount & 0x80) == 0) {
            return;
        }
        AdvanceSurfaceSlot();
        StepSurfaceIndex();
        CopySurfaceGlyphs();
        return;
    }
    surfaceWait = static_cast<uint8_t>(surfaceWait - 1);
    if (surfaceWait != 0) {
        return;
    }
    surfaceWait = kSurfaceDelay[surfaceSlot];
    surfaceCount = static_cast<uint8_t>(surfaceCount - 1);
    if (surfaceCount & 0x80) {
        AdvanceSurfaceSlot();
    }
    StepSurfaceIndex();
    CopySurfaceGlyphs();
}

// $3228. The count is the value before the increment.
void StepCrumble() {
    for (int slot = 0; slot < 3; ++slot) {
        Crumble& cell = crumb[slot];
        if (cell.n == 0) {
            continue;
        }
        const uint8_t shown = cell.n;
        cell.n = static_cast<uint8_t>(cell.n + 1);
        if (shown == 0x08 || shown == 0x10) {
            PrintString(0x2F, cell.row, cell.col);
            PrintString(0x30, cell.row + 1, cell.col);
        } else if (shown == 0x0C) {
            PrintString(0x31, cell.row, cell.col);
            PrintString(0x32, cell.row + 1, cell.col);
        } else if (shown == 0x14) {
            PrintString(0x00, cell.row, cell.col);
            const int colorAt = (PictureRow(cell.row) + 2) * 40 + (cell.col + 1);
            PaintColor(colorAt, 0xF8);
            PrintString(0x33, cell.row + 1, cell.col);
        } else if (shown == 0x18) {
            cell.n = 0;
        }
    }
}

void StartCrumble(int col, int row) {
    const int offset = PictureRow(row) * 40 + col;
    if (offset < 0 || offset >= 1000) {
        return;
    }
    // $306B points ($04) at color RAM, then stores $FD. The low three bits
    // become 5, so the next pass of this cell takes the skip. The screen
    // code is left as $60. That glyph is not a wall, and neither are the
    // flame tiles printed later.
    if ((g.color[static_cast<size_t>(offset)] & 7) == 5) {
        return;
    }
    for (int slot = 0; slot < 3; ++slot) {
        if (crumb[slot].n != 0) {
            continue;
        }
        crumb[slot].n = 1;
        crumb[slot].col = static_cast<uint8_t>(col - 1);
        crumb[slot].row = static_cast<uint8_t>(row - 1);
        PaintColor(offset, 0xFD);
        return;
    }
}

void Shock() {
    if (g.action == kZap) {
        return;
    }
    g.action = kZap;
    g.actionTimer = 0x10;
    g.hold = 0;
}

bool Overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

// $4133 writes four bytes, at sprite offsets 0, 3, 6, and 9. The mask
// table is read from the end, so the spark is the diamond 18 3C 3C 18.
void WriteBolt(int index) {
    uint8_t* shape = boltShape[index];
    for (int i = 0; i < 63; ++i) {
        shape[i] = 0;
    }
    for (int row = 0; row < 4; ++row) {
        shape[row * 3] = static_cast<uint8_t>(Noise() & kBoltMask[3 - row]);
    }
}

void StepBolts() {
    const int target = (g.x + 24) / 2;
    for (int i = 0; i < 2; ++i) {
        if (boltPhase[i] == 0) {
            if (boltDelay[i] != 0) {
                boltDelay[i] = static_cast<uint8_t>(boltDelay[i] - 1);
            } else {
                boltX[i] = i == 0 ? 0x50 : 0x5E;
                boltY[i] = 0x37;
                boltPhase[i] = 1;
                boltOn[i] = true;
            }
        }
        if (boltPhase[i] != 0) {
            boltY[i] = static_cast<uint8_t>(boltY[i] + 5);
            if (boltY[i] >= 0xE0) {
                boltPhase[i] = 0;
                boltDelay[i] = 2;
                boltOn[i] = false;
            } else {
                const int steps = boltY[i] < 0xA8 ? 3 : 2;
                for (int step = 0; step < steps; ++step) {
                    if (boltX[i] < target) {
                        boltX[i] = static_cast<uint8_t>(boltX[i] + 1);
                    } else if (boltX[i] > target) {
                        boltX[i] = static_cast<uint8_t>(boltX[i] - 1);
                    }
                }
            }
        }
        if (!boltOn[i]) {
            continue;
        }
        WriteBolt(i);
        const int sx = static_cast<int>(boltX[i]) * 2 - 24;
        const int sy = static_cast<int>(boltY[i]) - 21;
        if (Overlap(sx, sy, 8, 4, g.x, g.y, 24, 21)) {
            Shock();
        }
    }
}

void ResetBolts() {
    boltX[0] = 0x50;
    boltX[1] = 0x5E;
    boltY[0] = boltY[1] = 0x37;
    boltPhase[0] = boltPhase[1] = 0;
    boltDelay[0] = 0;
    boltDelay[1] = 0x10;
    boltOn[0] = boltOn[1] = false;
}

void StepMovers() {
    for (int i = 8; i >= 0; --i) {
        const uint8_t active = mover[i].active;
        if (active == 0) {
            continue;
        }
        const int oldCol = mover[i].col;
        const int row = mover[i].row;
        if (active & 0x80) {
            mover[i].col = static_cast<uint8_t>(mover[i].col + 1);
            if (mover[i].col >= mover[i].high) {
                mover[i].col = mover[i].low;
            }
        } else {
            mover[i].col = static_cast<uint8_t>(mover[i].col - 1);
            if (mover[i].col < mover[i].low) {
                mover[i].col = mover[i].high;
            }
        }
        PaintSlide(row, oldCol, mover[i].col, row == 5 || row == 0x0A);
    }
}

void HeightBonus() {
    const int ypos = g.y + 21;
    int band = -1;
    if (ypos >= 0x64 && ypos < 0x8A) {
        band = 0;
    } else if (ypos >= 0x8A && ypos < 0xB6) {
        band = 1;
    } else if (ypos >= 0xB6) {
        band = 2;
    }
    if (band >= 0 && bandBonus[band] != 0) {
        bandBonus[band] = static_cast<uint8_t>(bandBonus[band] - 1);
        AddScore(500);
    }
}

void WinGame() {
    difficulty += 1;
    ResetCollectedLamps();
    for (int i = 0; i < 8; ++i) {
        bandBonus[i] = 1;
    }
    EnterRoom(0);
    PlaceBruce();
    relocated = true;
}

void BlinkTile(uint8_t& timer, uint8_t& phase, int row, int col) {
    timer = static_cast<uint8_t>(timer - 1);
    if ((timer & 0x80) == 0) {
        return;
    }
    if (phase != 0) {
        phase = 0;
        PrintString(0x1E, row, col);
        timer = 5;
    } else {
        phase = static_cast<uint8_t>(phase + 1);
        PrintString(0x1D, row, col);
    }
}

void Frame0() {
    if (LampsLeft(6) != 0 || hookGate != 0) {
        return;
    }
    BlinkTile(t97, t98, 9, 7);
}

void Frame1() {
    if (hookGate != 0 || (t97 & 0x80) != 0) {
        return;
    }
    if (LampsLeft(6) == 0 || LampsLeft(0) != 0 || LampsLeft(1) != 0 || LampsLeft(2) != 0) {
        return;
    }
    t97 = static_cast<uint8_t>(t97 - 1);
    PrintString(static_cast<uint8_t>(t97 + 0x10), 0x0C, 0x11);
}

void Frame2() {
    if (LampsLeft(6) == 0 && (t98 & 0x80) == 0) {
        t98 = static_cast<uint8_t>(t98 - 1);
        PrintString(0x1A, 0x0B, 0x25);
        PrintString(0x07, 0x0C, 0x25);
    }
    if (LampsLeft(6) != 0 || hookGate != 0) {
        return;
    }
    BlinkTile(t99, t97, 9, 0x21);
}

void Frame3() {
    if (hookGate == 0 && (t97 & 0x80) == 0 && LampsLeft(3) == 0) {
        t97 = static_cast<uint8_t>(t97 - 1);
        PrintString(static_cast<uint8_t>(t97 + 0x10), 0x0C, 0x1D);
    }
    t99 = static_cast<uint8_t>(t99 - 1);
    if ((t99 & 0x80) == 0) {
        return;
    }
    t99 = 0x20;
    if (t98 != 0) {
        t98 = static_cast<uint8_t>(t98 - 1);
        PrintString(0x06, 6, 9);
        PrintString(0x60, 6, 0x15);
    } else {
        t98 = static_cast<uint8_t>(t98 + 1);
        PrintString(0x60, 6, 9);
        PrintString(0x06, 6, 0x15);
    }
}

void Frame4() {
    if (LampPresent(0x4D70)) {
        t99 = static_cast<uint8_t>(t99 + 1);
        if (t99 == 0x20) {
            PrintString(0x34, 4, 0x0B);
        } else if (t99 == 0x40) {
            t99 = 0;
            PrintString(0x36, 4, 0x0B);
        }
    } else if (Arm(t97)) {
        PrintString(0x16, 8, 0x0B);
        PrintString(0x16, 9, 0x0B);
        PrintString(0x25, row75, col72);
    }
    if (LampPresent(0x4D74)) {
        t9A = static_cast<uint8_t>(t9A + 1);
        if (t9A == 0x20) {
            PrintString(0x35, 4, 0x1C);
        } else if (t9A == 0x40) {
            t9A = 0;
            PrintString(0x37, 4, 0x1C);
        }
    } else if (Arm(t98)) {
        surfaceDown = 0;
        surfaceSlot = 7;
        PrintString(0x16, 8, 0x19);
        PrintString(0x16, 9, 0x19);
        PrintString(0x25, row76, col73);
    }
    AdvanceBeam(tim77, col71, 0x03, Byte(0xA0), 0x01, row74, 0x26);
    if (LampPresent(0x4D70)) {
        AdvanceBeam(tim78, col72, 0x0C, Byte(0xE4), 0x0B, row75, 0x1F);
    }
    if (LampPresent(0x4D74)) {
        AdvanceBeam(tim79, col73, 0x1B, Byte(0xE4), 0x1A, row76, 0x1F);
    }
    AnimateSurface();
}

void Frame5() {
    if (!LampPresent(0x4D99) && Arm(t99)) {
        OpenDoor(0x0A, 0x19);
        OpenDoor(0x0A, 0x1A);
        PrintString(0x25, row75, col72);
    }
    if (!LampPresent(0x4D91) && Arm(t9A)) {
        OpenDoor(0x0A, 0);
        PrintString(0x15, 0x0B, 0);
    }
    const bool side = LampsLeft(6) == 0 &&
        (!LampPresent(0x4D81) || !LampPresent(0x4D89));
    if (side && Arm(t9B)) {
        PrintString(0x19, 2, 0x22);
        OpenDoor(3, 0x1E);
        OpenDoor(6, 0x20);
    }
    AdvanceBeam(tim77, col71, 0x1C, Byte(0xE0), 0x1B, row74, 0x1F);
    if (LampPresent(0x4D99)) {
        AdvanceBeam(tim78, col72, 0x1C, Byte(0xE0), 0x1B, row75, 0x1F);
    }
    AdvanceBeam(tim79, col73, 0x02, Byte(0xE2), 0x01, row76, 0x26);
    AnimateSurface();
}

void Frame6() {
    if (LampsLeft(6) == 0 && Arm(t97)) {
        OpenDoorSolid(5, 1, 0x17);
    }
    StepCrumble();
    AnimateSurface();
}

void Frame7() {
    StepCrumble();
    const bool upper = !LampPresent(0x4DC3) && !LampPresent(0x4DC7) &&
        !LampPresent(0x4DCB) && !LampPresent(0x4DCF) && !LampPresent(0x4DD3);
    if (upper && Arm(t97)) {
        OpenDoorSolid(2, 0x21, 0x17);
        return;
    }
    const bool lower = !LampPresent(0x4DD7) && !LampPresent(0x4DDB) &&
        !LampPresent(0x4DDF) && !LampPresent(0x4DE3) && !LampPresent(0x4DE7) &&
        !LampPresent(0x4DEB);
    if (lower && Arm(t98)) {
        OpenDoorSolid(5, 0x26, 0x17);
        return;
    }
    AdvanceBeam(tim77, col71, 0x24, Byte(0xE0), 0x22, row74, 0x26);
    const bool right = LampPresent(0x4DEF) || LampPresent(0x4DF3);
    if (right) {
        StepSlide(true, 0x23, 5, false);
    } else {
        StepSlide(false, 6, 0x24, false);
    }
}

void Frame8() {
    if (LampsLeft(7) == 0 && !LampPresent(0x4E00) && Arm(t99)) {
        OpenDoor(7, 0x0E);
    }
    if (!LampPresent(0x4DF8) && !LampPresent(0x4DFC) && Arm(t9A)) {
        if (LampPresent(0x4E04)) {
            surfaceDown = 1;
            surfaceSlot = 7;
        }
        OpenDoor(7, 0x19);
    }
    if (LampsLeft(8) == 0 && !LampPresent(0x4E11) && !LampPresent(0x4E15) && Arm(t9B)) {
        OpenDoorSolid(2, 0x26, 0x2D);
    }
    if (!LampPresent(0x4E00) && Arm(t97)) {
        OpenDoor(0x0A, 1);
    }
    if (LampsLeft(7) == 0 && LampsLeft(8) == 0 && Arm(t98)) {
        OpenDoor(0x0A, 0x26);
    }
    AdvanceBeam(tim77, col71, 0x0B, Byte(0xDA), 0x06, row74, 0x1F);
    AdvanceBeam(tim78, col72, 0x20, Byte(0xDA), 0x1B, row75, 0x1F);
    AnimateSurface();
}

void Frame9() {
    if ((t99 & 0x80) == 0 && !LampPresent(0x4E09)) {
        t99 = static_cast<uint8_t>(t99 - 1);
        surfaceDown = 1;
        surfaceSlot = 7;
    }
    if (!LampPresent(0x4E0D) && Arm(t97)) {
        OpenDoor(7, 0x17);
        surfaceDown = 0;
        surfaceSlot = 7;
    }
    if (LampsLeft(9) == 0 && Arm(t98)) {
        surfaceDown = 0;
        surfaceSlot = 7;
        PrintString(0x18, 2, 0x23);
    }
    const bool left = !LampPresent(0x4E11) && !LampPresent(0x4E15);
    if (left) {
        StepSlide(false, 5, 0x18, false);
    } else {
        StepSlide(true, 0x18, 4, false);
    }
    AdvanceBeam(tim77, col71, 0x14, Byte(0xC0), 0x10, row74, 0x26);
    AdvanceBeam(tim78, col72, 0x14, Byte(0xC0), 0x10, row75, 0x26);
    AnimateSurface();
}

void Frame10() {
    if ((t97 & 0x80) == 0 && !LampPresent(0x4E1E) && !LampPresent(0x4E22)) {
        t97 = static_cast<uint8_t>(t97 - 1);
        OpenLateDoor(4, 0x26);
    }
    StepCrumble();
}

void Frame11() {
    if ((t97 & 0x80) == 0 && !LampPresent(0x4E33)) {
        t97 = static_cast<uint8_t>(t97 - 1);
        surfaceDown = 0;
        surfaceSlot = 7;
    }
    StepCrumble();
    AnimateSurface();
}

void Frame12() {
    if ((t97 & 0x80) == 0 && !LampPresent(0x4E40)) {
        t97 = static_cast<uint8_t>(t97 - 1);
        OpenLateDoor(0x0A, 0x26);
    }
    if ((t98 & 0x80) == 0 && LampsLeft(12) == 0 &&
        !LampPresent(0x4E52) && !LampPresent(0x4E56)) {
        t98 = static_cast<uint8_t>(t98 - 1);
        OpenLateDoor(7, 0x26);
    }
    StepCrumble();
}

void Frame13() {
    HeightBonus();
    if (hookGate == 0) {
        StepMovers();
    }
}

void Frame14() {
    if ((t97 & 0x80) == 0 && !LampPresent(0x4E4A) && !LampPresent(0x4E4E)) {
        t97 = static_cast<uint8_t>(t97 - 1);
        OpenLateDoor(4, 0x26);
    }
    StepCrumble();
}

void Frame15() {
    if (!LampPresent(0x4E5B) && Arm(t97)) {
        PrintString(0x57, 3, 0x11);
        PrintString(0x58, 4, 0x11);
    }
    if (!LampPresent(0x4E5F) && !LampPresent(0x4E63) && !LampPresent(0x4E6B) && Arm(t98)) {
        PrintString(0x57, 3, 0x17);
        PrintString(0x58, 4, 0x17);
    }
    if (LampsLeft(15) == 0 && Arm(t99)) {
        PrintString(0x59, 9, 0x25);
        PrintString(0x5A, 8, 0x25);
        PrintString(0x5B, 7, 0x25);
    }
}

void Frame16() {
    if ((t97 & 0x80) == 0 && LampsLeft(16) == 0) {
        t97 = static_cast<uint8_t>(t97 - 1);
        OpenLateDoor(7, 1);
    }
    StepCrumble();
}

void Frame17() {
    t99 = static_cast<uint8_t>(t99 - 1);
    if (t99 & 0x80) {
        t99 = 0x0A;
        const int ink = (g.color[0x80] & 7) ^ 2;
        for (int i = 0; i < 28; ++i) {
            PaintColor(0x80 + i, ink | 0xF8);
        }
    }
    t9A = static_cast<uint8_t>(t9A - 1);
    if (t9A & 0x80) {
        t9A = 1;
        StepSlide(true, 0x1B, 0x0B, false);
    }
    if (t97 != 0 && !LampPresent(0x4E7D)) {
        t97 = static_cast<uint8_t>(t97 - 1);
        PrintString(0x5D, 0x0A, 0x25);
    }
    if (t98 != 0 && !LampPresent(0x4E81)) {
        t98 = static_cast<uint8_t>(t98 - 1);
        PrintString(0x5C, 0x0A, 0x18);
    }
    StepCrumble();
    AdvanceBeam(tim77, col71, 0x23, Byte(0xC8), 0x08, row74, 0x1F);
    AdvanceBeam(tim78, col72, 0x25, Byte(0xCA), 0x08, row75, 0x1F);
    AdvanceBeam(tim79, col73, 0x25, Byte(0xCA), 0x08, row76, 0x1F);
}

// $3D4E. t9A selects the second body on the way up from 0, then the first.
void BossBody(int phase) {
    PrintString(0x38 + phase, 2, 0x10);
    int body = 0;
    if (t9A == 0) {
        t9A = 1;
        body = 6;
    } else {
        t9A = static_cast<uint8_t>(t9A - 1);
    }
    PrintString(0x3B + phase + body, 3, 0x12);
    PrintString(0x3E + phase + body, 4, 0x12);
}

int BossPhase() {
    const int xpos = (g.x + 24) / 2;
    if (xpos >= 0x76) {
        return 2;
    }
    if (xpos >= 0x3A) {
        return 1;
    }
    return 0;
}

// $3DCD. One ink across the wizard while the explosion counts down.
void BossFlash(int color) {
    for (int col = 0; col <= 0x11; ++col) {
        PaintColor(5 * 40 + 11 + col, color);
        PaintColor(6 * 40 + 11 + col, color);
        PaintColor(7 * 40 + 11 + col, color);
        PaintColor(8 * 40 + 11 + col, color);
        if (col >= 0x0C) {
            continue;
        }
        PaintColor(3 * 40 + 14 + col, color);
        PaintColor(4 * 40 + 14 + col, color);
        PaintColor(9 * 40 + 14 + col, color);
        PaintColor(10 * 40 + 14 + col, color);
        PaintColor(11 * 40 + 14 + col, color);
        PaintColor(12 * 40 + 14 + col, color);
    }
}

void Frame18() {
    if (t97 != 0) {
        if (!LampPresent(0x4E86)) {
            t97 = 0;
            boltOn[0] = boltOn[1] = false;
            PrintString(0x61, 0x0B, 0);
            AddScore(3000);
            t9A = 0;
            t98 = 0x50;
            BossBody(1);
            return;
        }
        StepBolts();
        t98 = static_cast<uint8_t>(t98 - 1);
        if (t98 != 0) {
            return;
        }
        t98 = 8;
        BossBody(BossPhase());
        return;
    }
    if (t98 == 0) {
        PrintString(0x5E, 0x0B, 0x25);
        PrintString(0x5F, 0x0C, 0x26);
        return;
    }
    const uint8_t shown = t98;
    const int ink = (shown == 1 ? 0 : (Noise() & 7)) | 0xF8;
    BossFlash(ink);
    t98 = static_cast<uint8_t>(t98 - 1);
    t9A ^= 1;
    PrintString(0x47 + t9A, 3, 0x0C);
    PrintString(0x49 + t9A, 3, 0x19);
    PrintString(0x4B + t9A, 6, 0x0E);
    PrintString(0x4D + t9A, 6, 0x17);
}

void Frame19() {
    if (victory <= 0) {
        WinGame();
        return;
    }
    victory -= 3;
    if (victory <= 0) {
        WinGame();
        return;
    }
    static const int kRows[8] = {0x2AC, 0x2D4, 0x2FC, 0x324, 0x34C, 0x374, 0x39C, 0x3C4};
    const int ink = (Noise() & 7) | 0xF8;
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 29; ++col) {
            PaintColor(kRows[row] + col, ink);
        }
    }
}

void Enter1() { t97 = 3; }
void Enter3() { t97 = 3; }

void Enter4() {
    t9A = 0x1F;
    col71 = 1;
    row74 = 0x0B;
    col72 = 0x0B;
    row75 = 7;
    row76 = 7;
    col73 = 0x1A;
    tim77 = Byte(0xFF);
    tim78 = Byte(0xFF);
    tim79 = Byte(0xF4);
}

void Enter5() {
    for (int offset : {0x9A, 0x9B, 0x9C, 0xC2, 0xC3, 0xC4}) {
        PaintColor(offset, 0xFC);
    }
    col71 = col72 = 0x1B;
    col73 = 1;
    row74 = 9;
    row75 = 0x0A;
    row76 = 5;
    tim77 = Byte(0xFF);
    tim78 = Byte(0xFC);
    tim79 = Byte(0xF8);
}

void Enter7() {
    slideCol = 5;
    slideRow = 0x0B;
    col71 = 0x22;
    row74 = 4;
    tim77 = Byte(0xFF);
}

void Enter8() {
    col71 = 6;
    col72 = 0x1B;
    row74 = row75 = 5;
    tim77 = Byte(0xFF);
    tim78 = Byte(0xF2);
}

void Enter9() {
    for (int col = 0; col < 4; ++col) {
        PaintColor(0x9B + col, 0xFA);
        PaintColor(0xC3 + col, 0xFA);
    }
    slideCol = 4;
    slideRow = 0x0C;
    col71 = col72 = 0x10;
    row74 = 5;
    row75 = 6;
    tim77 = Byte(0xFC);
    tim78 = Byte(0xFF);
}

void Enter12() { t97 = 1; }

void Enter13() {
    const Mover* table = difficulty == 0 ? kEasy : kHard;
    for (int i = 0; i < 9; ++i) {
        mover[i] = table[i];
    }
}

void Enter14() { t97 = 1; }
void Enter15() { t97 = t98 = t99 = 1; }

void Enter16() {
    static const int kCells[8] = {0x31C, 0x31D, 0x344, 0x345, 0x36C, 0x36D, 0x394, 0x395};
    for (int cell : kCells) {
        PaintColor(cell, 0xFF);
    }
    if (extraLives == 0) {
        PrintString(0x56, 0x0A, 0x24);
        PrintString(0x56, 0x0B, 0x24);
    }
}

void Enter17() {
    PaintColor(0x31E, 0xFF);
    t97 = 1;
    t98 = 1;
    col71 = col72 = col73 = 8;
    row74 = 3;
    slideRow = 5;
    row75 = 7;
    row76 = 9;
    slideCol = 0x0B;
    tim77 = Byte(0xF8);
    tim78 = Byte(0xF1);
    tim79 = Byte(0xEA);
}

void Enter18() {
    for (int offset = 0x78; offset < 1000; ++offset) {
        PaintColor(offset, 0xFE);
    }
    t97 = 1;
    t98 = 1;
    ResetBolts();
}

void Enter19() {
    victory = 0x0458;
    play87 = 0;
    play88 = 0;
}

}  // namespace

int Room19Stick() {
    // The three rasters of the wait at the end of the previous logic frame.
    // The first call of a visit therefore sees the entry frame's wait, and
    // $88 is still 0.
    play87 += 3;
    if (play87 >= 256) {
        play87 -= 256;
        if (play88 < 0x80) {
            play88 += 1;
        }
    }
    return play88 < 2 ? 0x0E : 0x0F;
}

bool AdvanceRoom() {
    if (hookGate == 0) {
        hookGate = 1;
    } else {
        hookGate -= 1;
    }
    relocated = false;
    switch (g.roomId) {
    case 0: Frame0(); break;
    case 1: Frame1(); break;
    case 2: Frame2(); break;
    case 3: Frame3(); break;
    case 4: Frame4(); break;
    case 5: Frame5(); break;
    case 6: Frame6(); break;
    case 7: Frame7(); break;
    case 8: Frame8(); break;
    case 9: Frame9(); break;
    case 10: Frame10(); break;
    case 11: Frame11(); break;
    case 12: Frame12(); break;
    case 13: Frame13(); break;
    case 14: Frame14(); break;
    case 15: Frame15(); break;
    case 16: Frame16(); break;
    case 17: Frame17(); break;
    case 18: Frame18(); break;
    case 19: Frame19(); break;
    default: break;
    }
    return relocated;
}

void BeginRoomEffects() {
    // $0A38 and $0A3C. Entering any room starts the scroll upward and
    // restarts the delay slot. The step and the countdown keep running,
    // so a long stay in the room is what reverses it.
    surfaceDown = 0;
    surfaceSlot = 7;
    t97 = t98 = t99 = t9A = t9B = 0;
    switch (g.roomId) {
    case 1: Enter1(); break;
    case 3: Enter3(); break;
    case 4: Enter4(); break;
    case 5: Enter5(); break;
    case 7: Enter7(); break;
    case 8: Enter8(); break;
    case 9: Enter9(); break;
    case 12: Enter12(); break;
    case 13: Enter13(); break;
    case 14: Enter14(); break;
    case 15: Enter15(); break;
    case 16: Enter16(); break;
    case 17: Enter17(); break;
    case 18: Enter18(); break;
    case 19: Enter19(); break;
    default: break;
    }
    RestoreSurfaceGlyphs();
}

void TouchRoomTile(int tile, int col, int row, bool prize, bool ladder) {
    if (tile == 0x62 && ladder && g.action != kZap) {
        g.action = kCarry;
        g.hold = 0;
    }
    if ((tile == 0x2D || tile == 0x2E) && g.roomId == 16 && g.y + 21 >= 0xA0) {
        extraLives -= 1;
        if (g.lives < 99) {
            g.lives += 1;
        }
        PrintString(0x56, 0x0A, 0x24);
        PrintString(0x56, 0x0B, 0x24);
    }
    if (!prize) {
        return;
    }
    const bool late = g.roomId >= 10 && tile == 0x66;
    const bool span = tile >= 0x54 && tile <= 0x5A && tile != 0x57;
    const bool hot = tile == 0x0F || tile == 0x40 || tile == 0x64 || late || span ||
        (tile >= 0x71 && tile <= 0x7C);
    if (hot && g.roomId != 18) {
        Shock();
    }
    if (tile == 0x60) {
        StartCrumble(col, row);
    }
}

int SurfaceShove() {
    if (surfaceWait != 1) {
        return 0;
    }
    return surfaceDown == 0 ? -1 : 1;
}

bool BossSparkAt(int index, int& x, int& y, const uint8_t*& shape) {
    if (g.roomId != 18 || index < 0 || index > 1 || !boltOn[index]) {
        return false;
    }
    x = static_cast<int>(boltX[index]) * 2 - 24;
    y = static_cast<int>(boltY[index]) - 21;
    shape = boltShape[index];
    return true;
}
