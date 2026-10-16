#include "scene.h"

#include "game.h"

void BandAt(int py, int& bg, int& c01, int& c10) {
    const int raster = py + 50;
    if (raster < 0x78) {
        bg = 0x0B;
        c01 = 0x01;
        c10 = 0x0E;
        return;
    }
    if (raster < 0x97) {
        bg = 0x0B;
        c01 = 0x02;
        c10 = 0x01;
        return;
    }
    bg = 0x0C;
    c01 = 0x02;
    c10 = 0x01;
}

int PixelColor(int x, int y) {
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) {
        return -1;
    }
    const int cell = (y / 8) * 40 + (x / 8);
    const uint8_t tile = g.room[static_cast<size_t>(cell)];
    if (tile == 0xDC) {
        return -1;
    }
    // Color RAM bit 3 turns multicolor on. The ink is only the low three
    // bits, so a stored $08 is black and a stored $0E is blue.
    int color = g.color[static_cast<size_t>(cell)] & 0x0F;
    if (color >= 8) {
        color &= 7;
    }
    // Even rows are the shadow copy. That copy is character|0x80, the lower
    // half of the same source glyph, stored in the second 1KB of the charset.
    const int half = ((y / 8) & 1) == 0 ? 128 : 0;
    const uint8_t bits = g.charset[static_cast<size_t>(tile + half) * 8 + static_cast<size_t>(y & 7)];
    const int pair = (bits >> (6 - 2 * ((x & 7) / 2))) & 3;
    int bg, c01, c10;
    BandAt(y, bg, c01, c10);
    const int colors[4] = {bg, c01, c10, color};
    const int ink = colors[pair];
    return ink == bg ? -1 : ink;
}
