#include "scene.h"

#include "game.h"

void BandAt(int py, int& bg, int& c01, int& c10) {
    // raster_split stores the next three bytes when the raster reaches the
    // value in $47F5. $FD points the chain back at the top of the screen.
    const int raster = py + 50;
    int band = 0;
    for (int i = 0; i < 3; ++i) {
        const int split = g.splits[i];
        if (split == 0xFD) {
            break;
        }
        if (raster >= split) {
            band = i + 1;
        } else {
            break;
        }
    }
    bg = g.bands[band * 3];
    c01 = g.bands[band * 3 + 1];
    c10 = g.bands[band * 3 + 2];
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
