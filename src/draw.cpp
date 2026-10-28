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

#include "draw.h"

#include "bruce.h"
#include "fighter.h"
#include "game.h"
#include "room.h"
#include "scene.h"

#include <cstdio>
#include <vector>

namespace {

// $8C00 is screen code $20 until V copies the line at $4707. A room enter
// writes $20 again. Room 19's own line owns that row, so the credit stays off.
bool creditOn = false;

bool SpritePixel(const uint8_t* frame, int x, int y) {
    const uint8_t bits = frame[y * kSpriteRowBytes + x / 8];
    return (bits & static_cast<uint8_t>(0x80 >> (x & 7))) != 0;
}

// The score line is the ROM charset, copied to $8800 at startup. The bytes
// are that font: digits, the words on the score bar, and the credit line.
uint8_t StatusGlyph(char ch, int row) {
    static const uint8_t kDigit[10][8] = {
        {0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00},
        {0x18, 0x18, 0x38, 0x18, 0x18, 0x18, 0x7E, 0x00},
        {0x3C, 0x66, 0x06, 0x0C, 0x30, 0x60, 0x7E, 0x00},
        {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00},
        {0x06, 0x0E, 0x1E, 0x66, 0x7F, 0x06, 0x06, 0x00},
        {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00},
        {0x3C, 0x66, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00},
        {0x7E, 0x66, 0x0C, 0x18, 0x18, 0x18, 0x18, 0x00},
        {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00},
        {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00},
    };
    static const uint8_t kT[8] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00};
    static const uint8_t kO[8] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00};
    static const uint8_t kP[8] = {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00};
    static const uint8_t kF[8] = {0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x60, 0x00};
    static const uint8_t kA[8] = {0x18, 0x3C, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00};
    static const uint8_t kB[8] = {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00};
    static const uint8_t kD[8] = {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00};
    static const uint8_t kE[8] = {0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x7E, 0x00};
    static const uint8_t kL[8] = {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00};
    static const uint8_t kS[8] = {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00};
    static const uint8_t kC[8] = {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00};
    static const uint8_t kN[8] = {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00};
    static const uint8_t kG[8] = {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00};
    static const uint8_t kR[8] = {0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00};
    static const uint8_t kU[8] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00};
    static const uint8_t kI[8] = {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00};
    static const uint8_t kStar[8] = {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00};
    static const uint8_t kParenL[8] = {0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00};
    static const uint8_t kParenR[8] = {0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00};
    static const uint8_t kDot[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00};
    static const uint8_t kJ[8] = {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00};
    static const uint8_t kV[8] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00};
    static const uint8_t kY[8] = {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00};
    const uint8_t* bits = nullptr;
    if (ch >= '0' && ch <= '9') {
        bits = kDigit[ch - '0'];
    } else if (ch == 'T') {
        bits = kT;
    } else if (ch == 'O') {
        bits = kO;
    } else if (ch == 'P') {
        bits = kP;
    } else if (ch == 'F') {
        bits = kF;
    } else if (ch == 'A') {
        bits = kA;
    } else if (ch == 'B') {
        bits = kB;
    } else if (ch == 'D') {
        bits = kD;
    } else if (ch == 'E') {
        bits = kE;
    } else if (ch == 'L') {
        bits = kL;
    } else if (ch == 'S') {
        bits = kS;
    } else if (ch == 'C') {
        bits = kC;
    } else if (ch == 'N') {
        bits = kN;
    } else if (ch == 'G') {
        bits = kG;
    } else if (ch == 'R') {
        bits = kR;
    } else if (ch == 'U') {
        bits = kU;
    } else if (ch == 'I') {
        bits = kI;
    } else if (ch == '*') {
        bits = kStar;
    } else if (ch == '(') {
        bits = kParenL;
    } else if (ch == ')') {
        bits = kParenR;
    } else if (ch == '.') {
        bits = kDot;
    } else if (ch == 'J') {
        bits = kJ;
    } else if (ch == 'V') {
        bits = kV;
    } else if (ch == 'Y') {
        bits = kY;
    }
    return bits != nullptr ? bits[row] : 0;
}

void DrawLine(const char* text, int column, int top) {
    const int origin = column * 8;
    for (int i = 0; text[i] != '\0'; ++i) {
        for (int row = 0; row < 8; ++row) {
            const uint8_t bits = StatusGlyph(text[i], row);
            for (int col = 0; col < 8; ++col) {
                if ((bits & static_cast<uint8_t>(0x80 >> col)) == 0) {
                    continue;
                }
                const int px = origin + i * 8 + col;
                const int py = top + row;
                if (px >= 0 && py >= 0 && px < kWidth && py < kHeight) {
                    g.pixels[static_cast<size_t>(py * kWidth + px)] = kPalette[0];
                }
            }
        }
    }
}

void DrawStatus() {
    // hook_frame_19 prints string 4 on row 0, column $0A, every logic frame.
    // The score digits stay on row 1, where init_hardware put them.
    // $4707 is the other row-0 line. V copies it while the cell is still $20.
    if (g.roomId == 19) {
        DrawLine("** CONGRATULATIONS **", 0x0A, 0);
    } else if (creditOn) {
        DrawLine("BRUCE LEE (C) 1984 DATASOFT REV.2 BY RJF", 0, 0);
    }
    // timed_caption. The row-0 pointer is $8C00 and the column is $2C, so
    // the word lands at $8C2C: row 1, column 4, on the grey bar. String 1
    // is "1UP". String 2 is "2UP" when $03 is 1. String 0 is three spaces,
    // the off phase, and the grey bar shows through.
    if (PlayerCaptionOn()) {
        DrawLine(gBrucePlayer == 0 ? "1UP" : "2UP", 4, 8);
    }
    char text[40];
    char falls[3];
    // stash_scores keeps eight BCD digits and draw_split_digits paints the
    // low six. 1000000 is shown as 000000.
    const int shown = g.score % 1000000;
    const int top = TopScore() % 1000000;
    SyncLives();
    FormatFalls(g.lives, falls);
    std::snprintf(text, sizeof text, "%06d  TOP %06d  FALLS %s", shown, top, falls);
    // The words sit on character row 1. Row 0 above them is the black bar,
    // except in room 19, where that row is the congratulations line.
    DrawLine(text, 8, 8);
}

void DrawSpark(int index) {
    int x = 0;
    int y = 0;
    const uint8_t* shape = nullptr;
    if (!BossSparkAt(index, x, y, shape)) {
        return;
    }
    for (int row = 0; row < 21; ++row) {
        for (int col = 0; col < 24; ++col) {
            const uint8_t bits = shape[row * 3 + col / 8];
            if ((bits & static_cast<uint8_t>(0x80 >> (col & 7))) == 0) {
                continue;
            }
            const int px = x + col;
            const int py = y + row;
            if (px >= 0 && py >= 0 && px < kWidth && py < kHeight) {
                g.pixels[static_cast<size_t>(py * kWidth + px)] = kPalette[7];
            }
        }
    }
}

}  // namespace

void ToggleCreditLine() {
    // Room 19's first cell is 0, not $20, so V fills the row with $20.
    // The same frame's hook then reprints the congratulations line.
    if (g.roomId == 19) {
        creditOn = false;
        return;
    }
    creditOn = !creditOn;
}

void ClearCreditLine() {
    creditOn = false;
}

bool ComposeWelcomeRoom(std::vector<uint32_t>& dest) {
    // Room 1. Its two spawn points are both on screen, so the Ninja and Yamo
    // can stand in the same picture. Bruce's spawn is the left one; he stands
    // just to the right of Yamo so neither fighter is covered.
    if (!ShowRoomPicture(1)) {
        return false;
    }
    const int bruceX = g.x;
    const int bruceY = g.y;
    const int bruceFacing = g.facing;
    const int brucePose = g.pose;
    std::vector<uint32_t> pixels = g.pixels;

    int leftX = 0;
    int leftY = 0;
    RoomSpawn(1, leftX, leftY);
    const int rightX = 0x95 * 2 - 24;
    const int rightY = 0x92 - 21;
    StandFighters(rightX, rightY, -1, leftX, leftY, 1);
    g.x = leftX + 40;
    g.y = leftY;
    g.facing = 1;
    g.pose = 0;
    Compose();
    dest = g.pixels;

    g.x = bruceX;
    g.y = bruceY;
    g.facing = bruceFacing;
    g.pose = brucePose;
    g.pixels = std::move(pixels);
    RestoreFighters();
    RestoreRoomPicture();
    return dest.size() == static_cast<size_t>(kWidth * kHeight);
}

void ComposeTitle(const uint8_t* bitmap) {
    // boot01_title.vsf is bitmap mode with multicolor on ($D011 $3B, $D016 $18).
    // $D018 is $18, so this bitmap is $2000 and the video matrix is $0400.
    // All 1000 matrix bytes are $1E, and color RAM is $0C. $D021 is black.
    // A pixel pair is two bits: 00 background, 01 the high nibble, 10 the low
    // nibble, 11 color RAM. $D020 is light blue and sits outside this picture.
    // The bitmap paints that same light blue along its outer edge, so on the
    // Commodore it continues the border. Those pixels are the grey of the
    // picture field. Light blue inside the picture stays: the buildings and
    // the frame lines.
    const int color[4] = {0x00, 0x01, 0x0E, 0x0C};
    std::vector<uint8_t> pairs(static_cast<size_t>(kWidth) * kHeight);
    for (int y = 0; y < kHeight; ++y) {
        uint32_t* row = g.pixels.data() + static_cast<size_t>(y) * kWidth;
        for (int x = 0; x < kWidth; ++x) {
            const int index = ((y / 8) * 40 + (x / 8)) * 8 + (y & 7);
            const int pair = (x & 7) / 2;
            const int bits = (bitmap[index] >> (6 - pair * 2)) & 3;
            pairs[static_cast<size_t>(y * kWidth + x)] = static_cast<uint8_t>(bits);
            row[x] = kPalette[color[bits]];
        }
    }
    const uint32_t field = kPalette[0x0C];
    std::vector<int> pending;
    auto take = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) {
            return;
        }
        const size_t i = static_cast<size_t>(y * kWidth + x);
        if (pairs[i] != 2) {
            return;
        }
        pairs[i] = 3;
        g.pixels[i] = field;
        pending.push_back(static_cast<int>(i));
    };
    for (int x = 0; x < kWidth; ++x) {
        take(x, 0);
        take(x, kHeight - 1);
    }
    for (int y = 0; y < kHeight; ++y) {
        take(0, y);
        take(kWidth - 1, y);
    }
    for (size_t n = 0; n < pending.size(); ++n) {
        const int i = pending[n];
        const int x = i % kWidth;
        const int y = i / kWidth;
        take(x - 1, y);
        take(x + 1, y);
        take(x, y - 1);
        take(x, y + 1);
    }
}

void Compose() {
    for (int y = 0; y < kHeight; ++y) {
        int bg, c01, c10;
        BandAt(y, bg, c01, c10);
        const uint32_t paper = kPalette[bg];
        uint32_t* row = g.pixels.data() + y * kWidth;
        for (int x = 0; x < kWidth; ++x) {
            const int ink = PixelColor(x, y);
            // init_hardware writes character $20 across $8C00 and character
            // $06 across $8C50. Color RAM for the first two rows stays $00,
            // and row 2 stays $08 from the $F8 fill. enter_room starts at
            // $D878, so it never recolors them.
            // Row 0: glyph $20 in the score charset is solid, hires color 0,
            // so the bar above the score is black. V replaces that glyph.
            // The credit ink is still color 0, and the clear bits show $D021,
            // which raster_irq holds at $FC (grey) through this row. Row 1 is
            // character 0, an empty glyph, so it shows that same grey. The
            // set bits of the score glyphs are color 0, drawn afterwards.
            // hook_enter_19 writes 0 across row 0. Glyph 0 is blank, so that
            // bar is the same grey, and the congratulations line sits on it.
            // Row 2: glyph $06 in the room charset is solid $FF. Color $08
            // is multicolor ink black, so the row above the picture is black.
            const bool creditRow = y < 8 && creditOn && g.roomId != 19;
            if (((y < 8 && g.roomId != 19) || (y >= 16 && y < 24)) && !creditRow) {
                row[x] = kPalette[0];
            } else if (y < 16) {
                row[x] = kPalette[0x0C];
            } else {
                row[x] = ink < 0 ? paper : kPalette[ink];
            }
        }
    }
    DrawSpark(0);
    DrawSpark(1);
    // Yamo, then the Ninja, then Bruce. Lower sprite numbers are in front,
    // and Bruce is sprites 0 and 1.
    DrawFighters();
    // The body is yellow. The second image is the black hair, eyes, and legs,
    // drawn on top. One sprite pixel is one screen pixel, which is the shape
    // in the reference picture.
    const int spriteColor[2] = {7, 0};
    for (int plane = 0; plane < 2; ++plane) {
        const uint8_t* frame = PoseBytes(g.pose, plane);
        for (int y = 0; y < kSpriteRows; ++y) {
            for (int x = 0; x < kSpriteCols; ++x) {
                if (!SpritePixel(frame, x, y)) {
                    continue;
                }
                const int px = g.x + PlaneShift(g.pose, plane) + x;
                const int py = g.y + y;
                if (px >= 0 && py >= 0 && px < kWidth && py < kHeight) {
                    g.pixels[static_cast<size_t>(py * kWidth + px)] = kPalette[spriteColor[plane]];
                }
            }
        }
    }
    DrawStatus();
}
