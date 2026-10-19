#include "draw.h"

#include "bruce.h"
#include "game.h"
#include "room.h"
#include "scene.h"

#include <cstdio>

namespace {

bool SpritePixel(const uint8_t* frame, int x, int y) {
    const uint8_t bits = frame[y * kSpriteRowBytes + x / 8];
    return (bits & static_cast<uint8_t>(0x80 >> (x & 7))) != 0;
}

// The score line is the ROM charset, copied to $8800 at startup. These are
// the same shapes: digits, and the words on the room 0 bar.
uint8_t StatusGlyph(char ch, int row) {
    static const uint8_t kDigit[10][8] = {
        {0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00},
        {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
        {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x30, 0x7E, 0x00},
        {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00},
        {0x0C, 0x1C, 0x3C, 0x6C, 0x7E, 0x0C, 0x0C, 0x00},
        {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00},
        {0x3C, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x3C, 0x00},
        {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00},
        {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00},
        {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x0C, 0x38, 0x00},
    };
    static const uint8_t kT[8] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00};
    static const uint8_t kO[8] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00};
    static const uint8_t kP[8] = {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00};
    static const uint8_t kF[8] = {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00};
    static const uint8_t kA[8] = {0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00};
    static const uint8_t kL[8] = {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00};
    static const uint8_t kS[8] = {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00};
    static const uint8_t kC[8] = {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00};
    static const uint8_t kN[8] = {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00};
    static const uint8_t kG[8] = {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00};
    static const uint8_t kR[8] = {0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00};
    static const uint8_t kU[8] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00};
    static const uint8_t kI[8] = {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00};
    static const uint8_t kStar[8] = {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00};
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
    if (g.roomId == 19) {
        DrawLine("** CONGRATULATIONS **", 0x0A, 8);
        return;
    }
    char text[40];
    const int shown = g.score > 999999 ? 999999 : g.score;
    int lives = g.lives;
    if (lives < 0) {
        lives = 0;
    } else if (lives > 99) {
        lives = 99;
    }
    std::snprintf(text, sizeof text, "%06d  TOP 000000  FALLS %02d", shown, lives);
    // The words sit on character row 1. Row 0 above them is the black bar.
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

void ComposeTitle(const uint8_t* bitmap) {
    const uint32_t ink = kPalette[1];
    const uint32_t paper = kPalette[0];
    for (int y = 0; y < kHeight; ++y) {
        uint32_t* row = g.pixels.data() + static_cast<size_t>(y) * kWidth;
        for (int x = 0; x < kWidth; ++x) {
            const int index = ((y / 8) * 40 + (x / 8)) * 8 + (y & 7);
            const bool on = (bitmap[index] & static_cast<uint8_t>(0x80 >> (x & 7))) != 0;
            row[x] = on ? ink : paper;
        }
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
            // so the bar above the score is black. Row 1 is character 0,
            // an empty glyph, so it shows $D021. raster_irq holds that at
            // $FC (grey) until raster $40, inside the bottom of this row.
            // The set bits of the score glyphs are color 0, drawn afterwards.
            // Row 2: glyph $06 in the room charset is solid $FF. Color $08
            // is multicolor ink black, so the row above the picture is black.
            if (y < 8 || (y >= 16 && y < 24)) {
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
