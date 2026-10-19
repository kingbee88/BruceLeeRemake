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

void ComposePlayerUp(int player) {
    uint8_t line[kLineBytes];
    std::memcpy(line, PlayerLines(), kLineBytes);
    if (player != 0) {
        const uint8_t* digits = PlayerDigits();
        line[14] = digits[2];
        line[15] = digits[3];
        line[30] = digits[6];
        line[31] = digits[7];
    }
    Clear(kBlue);
    // Between the lives the whole page stays on charset $8000, multicolor.
    // $D022 is not flat: between_raster stripes it down the letters.
    for (int i = 0; i < 16; ++i) {
        DrawCell(LogoCharset(), line[i] | 0x80, 12 + i, 7, true, -1);
        DrawCell(LogoCharset(), line[16 + i] | 0x80, 12 + i, 8, true, -1);
    }
}
