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

#include "front.h"

#include "game.h"

#include "load.h"

#include <cstdint>
#include <cstring>

namespace {

// data/front.bin, taken from the menu snapshot:
//   2048 bytes  charset at $8000. Rows 0-9 of the options page, and the
//               whole player page. Multicolor glyphs, 8 bytes each.
//   2048 bytes  charset at $8800. Rows 10-24 of the options page, the
//               score font with a blank in glyph 0. Hires.
//   1000 bytes  screen at $8C00 with 1 PLAYER and COMPUTER already lit.
//   32 bytes    the two player lines at $4A5D, 16 glyphs each, player 1's
//               digit already filled in. between_rooms ORs $80 at draw time.
//   8 bytes     $4A7D. Player 2's digit is bytes 2, 3 and 6, 7.
constexpr int kLogoBytes = 2048;
constexpr int kFontBytes = 2048;
constexpr int kScreenBytes = 1000;
constexpr int kLineBytes = 32;
constexpr int kDigitBytes = 8;
constexpr int kFrontBytes = kLogoBytes + kFontBytes + kScreenBytes + kLineBytes + kDigitBytes;

constexpr int kBlue = 6;
constexpr int kWhite = 1;
constexpr int kLightGrey = 15;
constexpr int kYellow = 7;

std::vector<uint8_t> gFront;

const uint8_t* LogoCharset() {
    return gFront.data();
}

const uint8_t* MenuFont() {
    return gFront.data() + kLogoBytes;
}

const uint8_t* MenuScreen() {
    return gFront.data() + kLogoBytes + kFontBytes;
}

const uint8_t* PlayerLines() {
    return gFront.data() + kLogoBytes + kFontBytes + kScreenBytes;
}

const uint8_t* PlayerDigits() {
    return PlayerLines() + kLineBytes;
}

void Clear(int color) {
    const uint32_t pixel = kPalette[color];
    for (uint32_t& sample : g.pixels) {
        sample = pixel;
    }
}

// $D022 while between_raster runs. The interrupt waits until raster $6E,
// then walks dark grey, grey, light grey, white, and back. One wait there
// is about one PAL line. Screen y 0 is raster $33, the same origin the
// in-game splits use. The player glyphs are multicolor %01, so this is
// their ink.
int PlayerInk(int y) {
    const int raster = y + 51;
    if (raster < 113) {
        return 0x0B;
    }
    if (raster < 115) {
        return 0x0C;
    }
    if (raster < 117) {
        return kLightGrey;
    }
    if (raster < 118) {
        return kWhite;
    }
    if (raster < 120) {
        return kLightGrey;
    }
    if (raster < 122) {
        return 0x0C;
    }
    return 0x0B;
}

void DrawCell(const uint8_t* charset, int code, int col, int row, bool multi, int d022) {
    const uint8_t* glyph = charset + code * 8;
    const int originX = col * 8;
    const int originY = row * 8;
    for (int yy = 0; yy < 8; ++yy) {
        const int bits = glyph[yy];
        const int py = originY + yy;
        if (py < 0 || py >= kHeight) {
            continue;
        }
        uint32_t* dest = g.pixels.data() + static_cast<size_t>(py) * kWidth;
        if (multi) {
            const int ink = d022 < 0 ? PlayerInk(py) : d022;
            const int color[4] = {kBlue, ink, kLightGrey, kYellow};
            for (int xx = 0; xx < 4; ++xx) {
                const uint32_t pixel = kPalette[color[(bits >> (6 - xx * 2)) & 3]];
                const int px = originX + xx * 2;
                if (px >= 0 && px + 1 < kWidth) {
                    dest[px] = pixel;
                    dest[px + 1] = pixel;
                }
            }
        } else {
            for (int xx = 0; xx < 8; ++xx) {
                const int px = originX + xx;
                if (px < 0 || px >= kWidth) {
                    continue;
                }
                const bool on = (bits & (0x80 >> xx)) != 0;
                dest[px] = kPalette[on ? kWhite : kBlue];
            }
        }
    }
}

// flip_menu_a / flip_menu_b. origin 0 is the 1 PLAYER row, $50 is the
// COMPUTER row. Bit 6 of twenty glyphs swaps the plain and inverse letters.
// $4D is the dash, which stays put. draw_menu_word then rewrites the four
// joins at the ends of the highlighted word.
void LightOption(uint8_t* screen, int origin) {
    const int start = origin == 0 ? 0x1F0 : 0x240;
    for (int i = 0; i < 0x14; ++i) {
        if (screen[start + i] != 0x4D) {
            screen[start + i] = static_cast<uint8_t>(screen[start + i] ^ 0x40);
        }
    }
    screen[0x1EF + origin] = 0x00;
    screen[0x1F8 + origin] = 0x42;
    screen[0x1FA + origin] = 0x40;
    screen[0x204 + origin] = 0x56;
}

}  // namespace

bool LoadFront() {
    gFront = LoadFile(L"front.bin", static_cast<size_t>(kFrontBytes));
    return !gFront.empty();
}

void ComposeMenu(bool twoPlayers, bool humanOpponent) {
    uint8_t screen[kScreenBytes];
    std::memcpy(screen, MenuScreen(), kScreenBytes);
    // The stored page is the cold start: both options 0. flip_menu runs
    // only for the option that is set.
    if (twoPlayers) {
        LightOption(screen, 0);
    }
    if (humanOpponent) {
        LightOption(screen, 0x50);
    }
    Clear(kBlue);
    // open_room's raster uses charset $8000 until raster $82, which is the
    // top of character row 10, then the score font at $8800. The top is
    // multicolor ($D016 $18, color RAM $0F). The lower page is hires white.
    for (int row = 0; row < 25; ++row) {
        const bool logo = row < 10;
        const uint8_t* charset = logo ? LogoCharset() : MenuFont();
        for (int col = 0; col < 40; ++col) {
            DrawCell(charset, screen[row * 40 + col], col, row, logo, kLightGrey);
        }
    }
}

void DrawPlayerLines(int player, int topRow, int d022) {
    uint8_t line[kLineBytes];
    std::memcpy(line, PlayerLines(), kLineBytes);
    if (player != 0) {
        const uint8_t* digits = PlayerDigits();
        line[14] = digits[2];
        line[15] = digits[3];
        line[30] = digits[6];
        line[31] = digits[7];
    }
    for (int i = 0; i < 16; ++i) {
        DrawCell(LogoCharset(), line[i] | 0x80, 12 + i, topRow, true, d022);
        DrawCell(LogoCharset(), line[16 + i] | 0x80, 12 + i, topRow + 1, true, d022);
    }
}

void ComposePlayerUp(int player) {
    Clear(kBlue);
    // Between the lives the whole page stays on charset $8000, multicolor.
    // $D022 is not flat: between_raster stripes it down the letters.
    DrawPlayerLines(player, 7, -1);
}

void ComposeGameOver(int score) {
    // $42CD and $42DF, eighteen codes each. Bit 7 selects the upper half of
    // charset $8000. Row 1 and row 2, column 11.
    static const uint8_t kTop[18] = {
        0x10, 0x12, 0x00, 0x02, 0x18, 0x1A, 0x0C, 0x0E, 0x5C,
        0x5C, 0x1C, 0x1E, 0x2C, 0x2E, 0x0C, 0x0E, 0x24, 0x26,
    };
    static const uint8_t kBottom[18] = {
        0x11, 0x13, 0x01, 0x03, 0x19, 0x1B, 0x0D, 0x0F, 0x5C,
        0x5C, 0x1D, 0x1F, 0x2D, 0x2F, 0x0D, 0x0F, 0x25, 0x27,
    };
    Clear(kBlue);
    // game_over_raster keeps $D022 and $D023 at light grey. The letters are
    // multicolor pair 01, so the ink is flat, not the PLAYER-page stripe.
    for (int i = 0; i < 18; ++i) {
        DrawCell(LogoCharset(), kTop[i] | 0x80, 11 + i, 1, true, kLightGrey);
        DrawCell(LogoCharset(), kBottom[i] | 0x80, 11 + i, 2, true, kLightGrey);
    }
    // $8CD4 and $8CFC. Same PLAYER lines as between_rooms, one row higher.
    DrawPlayerLines(0, 5, kLightGrey);
    // draw_digit. Six nibbles from $439C, each a 2x2 glyph block on rows 8 and 9.
    int shown = score % 1000000;
    if (shown < 0) {
        shown = 0;
    }
    int place = 100000;
    for (int i = 0; i < 6; ++i) {
        const int nibble = shown / place;
        shown %= place;
        place /= 10;
        const int base = (0x34 + nibble * 4) | 0x80;
        const int col = 14 + i * 2;
        DrawCell(LogoCharset(), base, col, 8, true, kLightGrey);
        DrawCell(LogoCharset(), base + 2, col + 1, 8, true, kLightGrey);
        DrawCell(LogoCharset(), base + 1, col, 9, true, kLightGrey);
        DrawCell(LogoCharset(), base + 3, col + 1, 9, true, kLightGrey);
    }
}

namespace {

// One multicolor glyph, 4 dots wide. localY is the row inside the 16-pixel
// letter, so the ink follows the PLAYER page stripe even though the band
// sits in the middle of the screen. On that page the glyphs start at
// screen y 56, and between_raster walks the greys across them.
void PaintMulti(const uint8_t* glyph, int x, int y, int localY) {
    for (int row = 0; row < 8; ++row) {
        const int py = y + row;
        if (py < 0 || py >= kHeight) {
            continue;
        }
        const int ink = PlayerInk(56 + localY + row);
        const int color[4] = {kBlue, ink, kLightGrey, kYellow};
        const int bits = glyph[row];
        uint32_t* dest = g.pixels.data() + static_cast<size_t>(py) * kWidth;
        for (int col = 0; col < 4; ++col) {
            const uint32_t pixel = kPalette[color[(bits >> (6 - col * 2)) & 3]];
            const int px = x + col * 2;
            if (px >= 0 && px + 1 < kWidth) {
                dest[px] = pixel;
                dest[px + 1] = pixel;
            }
        }
    }
}

void PaintLetter(const uint8_t* topLeft, const uint8_t* topRight,
                 const uint8_t* bottomLeft, const uint8_t* bottomRight,
                 int x, int y) {
    PaintMulti(topLeft, x, y, 0);
    PaintMulti(topRight, x + 8, y, 0);
    PaintMulti(bottomLeft, x, y + 8, 8);
    PaintMulti(bottomRight, x + 8, y + 8, 8);
}

// Charset $8000 has the PLAYER letters and not S or D. These two use that
// same grid: 8 multicolor dots, pair 01 for the ink, four blank rows, and
// the stroke of P and C. Each letter is top-left, top-right, bottom-left,
// bottom-right, 8 bytes a glyph.
const uint8_t kLetterS[32] = {
    0x00, 0x00, 0x00, 0x00, 0x05, 0x15, 0x14, 0x14,
    0x00, 0x00, 0x00, 0x00, 0x50, 0x54, 0x14, 0x00,
    0x15, 0x15, 0x00, 0x00, 0x14, 0x14, 0x15, 0x05,
    0x50, 0x54, 0x14, 0x14, 0x14, 0x14, 0x54, 0x50,
};
const uint8_t kLetterD[32] = {
    0x00, 0x00, 0x00, 0x00, 0x15, 0x15, 0x14, 0x14,
    0x00, 0x00, 0x00, 0x00, 0x50, 0x54, 0x14, 0x14,
    0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x15, 0x15,
    0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x54, 0x50,
};

void PaintCharsetLetter(int code, int x, int y) {
    const uint8_t* charset = LogoCharset();
    PaintLetter(charset + ((code | 0x80) * 8),
                charset + (((code + 2) | 0x80) * 8),
                charset + (((code + 1) | 0x80) * 8),
                charset + (((code + 3) | 0x80) * 8),
                x, y);
}

void PaintRawLetter(const uint8_t* raw, int x, int y) {
    PaintLetter(raw, raw + 8, raw + 16, raw + 24, x, y);
}

}  // namespace

void DrawRestartBar(int left, int right, int top, int height, float progress) {
    if (progress < 0.f || height < 3 || right - left < 3) {
        return;
    }
    if (progress > 1.f) {
        progress = 1.f;
    }
    const uint32_t frame = kPalette[kLightGrey];
    const uint32_t fill = kPalette[kWhite];
    const uint32_t blue = kPalette[kBlue];
    for (int y = top; y < top + height; ++y) {
        if (y < 0 || y >= kHeight) {
            continue;
        }
        uint32_t* row = g.pixels.data() + static_cast<size_t>(y) * kWidth;
        for (int x = left; x < right; ++x) {
            if (x < 0 || x >= kWidth) {
                continue;
            }
            const bool border = y == top || y + 1 == top + height || x == left || x + 1 == right;
            row[x] = border ? frame : blue;
        }
    }
    const int innerLeft = left + 1;
    const int innerWidth = (right - 1) - innerLeft;
    int filled = static_cast<int>(static_cast<float>(innerWidth) * progress + 0.5f);
    if (filled > innerWidth) {
        filled = innerWidth;
    }
    for (int y = top + 1; y + 1 < top + height; ++y) {
        if (y < 0 || y >= kHeight) {
            continue;
        }
        uint32_t* row = g.pixels.data() + static_cast<size_t>(y) * kWidth;
        for (int x = innerLeft; x < innerLeft + filled; ++x) {
            if (x >= 0 && x < kWidth) {
                row[x] = fill;
            }
        }
    }
}

void DrawPausedBanner(float restartProgress) {
    const uint32_t blue = kPalette[kBlue];
    const int bandBottom = kPausedBandTop + kPausedBandHeight;
    for (int y = kPausedBandTop; y < bandBottom; ++y) {
        if (y < 0 || y >= kHeight) {
            continue;
        }
        uint32_t* row = g.pixels.data() + static_cast<size_t>(y) * kWidth;
        for (int x = 0; x < kWidth; ++x) {
            row[x] = blue;
        }
    }
    if (gFront.empty()) {
        return;
    }
    // P A U are in the logo charset. S and D are the matching shapes above.
    // E is the GAME OVER E. Six letters, 16 pixels each, centered in the band.
    constexpr int kLetter = 16;
    constexpr int kCount = 6;
    const int textW = kCount * kLetter;
    const int textX = (kWidth - textW) / 2;
    const int textY = kPausedBandTop + (kPausedBandHeight - kLetter) / 2;
    const int codes[kCount] = {0x20, 0x00, 0x28, -1, 0x0C, -2};
    for (int i = 0; i < kCount; ++i) {
        const int x = textX + i * kLetter;
        if (codes[i] == -1) {
            PaintRawLetter(kLetterS, x, textY);
        } else if (codes[i] == -2) {
            PaintRawLetter(kLetterD, x, textY);
        } else {
            PaintCharsetLetter(codes[i], x, textY);
        }
    }
    // Under the word. The glyph block is 16 pixels, and the band has room
    // below it for a 6-pixel bar.
    DrawRestartBar(textX, textX + textW, textY + kLetter + 4, 6, restartProgress);
}

// Hires, the same single pixels as the score glyphs, in the C64 palette.
// # and K are the red edge. * and + are the bulb. W is the lens. The body
// is light grey, grey, and dark grey. Each dot is drawn as a 2 by 2 block.
namespace {

constexpr int kCameraDotsW = 24;
constexpr int kCameraDotsH = 18;
constexpr int kCameraScale = 2;
static_assert(kCameraFlashW == kCameraDotsW * kCameraScale, "camera flash width");
static_assert(kCameraFlashH == kCameraDotsH * kCameraScale, "camera flash height");

constexpr char kCameraRows[kCameraDotsH][kCameraDotsW + 1] = {
    "...........#............",
    "..........#*#...........",
    ".........#*+*#..........",
    "....*...#*+++*#...*.....",
    ".........#*+*#..........",
    "..........#*#...........",
    "...........#............",
    ".......KKKKKKKK.........",
    "....KKKKKKKKKKKKKKKK....",
    "....KLLLLLLLLLLLLLLK....",
    "....KLGGGGGGGGGGGGKL....",
    "....KLGGKKKKKKGGGGKL....",
    "....KLGGKWWWWKGGGGKL....",
    "....KLGGKWWWWKGGGGKL....",
    "....KLGGKKKKKKGGGGKL....",
    "....KLGGGGGGGGGGGGKL....",
    "....KDDDDDDDDDDDDDDK....",
    "....KKKKKKKKKKKKKKKK....",
};

constexpr bool CameraRowsFit() {
    for (int y = 0; y < kCameraDotsH; ++y) {
        int n = 0;
        while (kCameraRows[y][n] != '\0') {
            ++n;
        }
        if (n != kCameraDotsW) {
            return false;
        }
    }
    return true;
}

static_assert(CameraRowsFit(), "camera flash row width");

// The pop, then a cooler star, then the camera with the lens still lit.
constexpr int kBurstMs = 180;
constexpr int kGlowMs = 380;
static_assert(kGlowMs < kCameraFlashMs, "camera flash phases");

int FlashInk(char mark, int age) {
    const bool burst = age < kBurstMs;
    const bool glow = age < kGlowMs;
    switch (mark) {
    case '#':
        return glow ? 2 : -1;
    case '*':
        if (burst) {
            return 1;
        }
        return glow ? 7 : -1;
    case '+':
        if (burst) {
            return 7;
        }
        return glow ? 8 : -1;
    case 'W':
        return burst ? 1 : 7;
    case 'K':
        return 2;
    case 'L':
        return 15;
    case 'G':
        return 12;
    case 'D':
        return 11;
    default:
        return -1;
    }
}

}  // namespace

void DrawCameraFlash(int ageMs) {
    if (ageMs < 0 || ageMs >= kCameraFlashMs) {
        return;
    }
    if (g.pixels.size() < static_cast<size_t>(kWidth) * kHeight) {
        return;
    }
    for (int y = 0; y < kCameraDotsH; ++y) {
        for (int x = 0; x < kCameraDotsW; ++x) {
            const int ink = FlashInk(kCameraRows[y][x], ageMs);
            if (ink < 0) {
                continue;
            }
            const uint32_t color = kPalette[ink];
            for (int sy = 0; sy < kCameraScale; ++sy) {
                const int py = kCameraFlashY + y * kCameraScale + sy;
                if (py < 0 || py >= kHeight) {
                    continue;
                }
                uint32_t* row = g.pixels.data() + static_cast<size_t>(py) * kWidth;
                for (int sx = 0; sx < kCameraScale; ++sx) {
                    const int px = kCameraFlashX + x * kCameraScale + sx;
                    if (px >= 0 && px < kWidth) {
                        row[px] = color;
                    }
                }
            }
        }
    }
}
