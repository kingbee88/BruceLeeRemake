#include "draw.h"

#include "bruce.h"
#include "game.h"
#include "scene.h"

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
    }
    return bits != nullptr ? bits[row] : 0;
}

void DrawStatus() {
    const char* text = "000000  TOP 000000  FALLS 04";
    const int origin = 8 * 8;
    for (int i = 0; text[i] != '\0'; ++i) {
        for (int row = 0; row < 8; ++row) {
            const uint8_t bits = StatusGlyph(text[i], row);
            for (int col = 0; col < 8; ++col) {
                if ((bits & static_cast<uint8_t>(0x80 >> col)) == 0) {
                    continue;
                }
                const int px = origin + i * 8 + col;
                if (px >= 0 && px < kWidth) {
                    g.pixels[static_cast<size_t>(row * kWidth + px)] = kPalette[0];
                }
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
            if (y < 8) {
                row[x] = kPalette[15];
            } else {
                row[x] = ink < 0 ? paper : kPalette[ink];
            }
        }
    }
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
