#include "collision.h"

#include "bruce.h"
#include "game.h"

namespace {

// $537E stores each AND nibble, and $5431 packs the watched slots into $B6.
// Solid patterns 3, 7 and 11-15 become bit 3 while the cell property is 0.
// A nonzero property swaps those patterns into bit 2. Bit 0 is the edge
// patterns 1, 4, 5, 6, 7, 9 and 13. Statue ink is outside both sets, so
// walking into it is left as it is.
void AddPose(Contact& hit, int pose, int sx, int sy) {
    uint8_t mask[kFrameBytes] = {};
    for (int plane = 0; plane < 2; ++plane) {
        const uint8_t* frame = PoseBytes(pose, plane);
        for (int i = 0; i < kFrameBytes; ++i) {
            mask[i] |= frame[i];
        }
    }
    for (int y = 0; y < kSpriteRows; ++y) {
        const int py = sy + y;
        uint8_t cols[40] = {};
        bool used[40] = {};
        for (int x = 0; x < kSpriteCols; ++x) {
            const uint8_t bits = mask[y * 3 + x / 8];
            if ((bits & static_cast<uint8_t>(0x80 >> (x & 7))) == 0) {
                continue;
            }
            const int px = sx + x;
            if (px < 0 || px >= kWidth || py < 0 || py >= kHeight) {
                hit.wall = true;
                continue;
            }
            const int col = px / 8;
            cols[col] |= static_cast<uint8_t>(0x80 >> (px & 7));
            used[col] = true;
        }
        if (py < 0 || py >= kHeight) {
            continue;
        }
        const int row = py / 8;
        const int half = (row & 1) == 0 ? 128 : 0;
        const int rowByte = py & 7;
        for (int col = 0; col < 40; ++col) {
            if (!used[col]) {
                continue;
            }
            const int cell = row * 40 + col;
            const uint8_t tile = g.room[static_cast<size_t>(cell)];
            if (tile == 0xDC) {
                continue;
            }
            const uint8_t bg = g.charset[static_cast<size_t>(tile + half) * 8 + static_cast<size_t>(rowByte)];
            const uint8_t overlapped = static_cast<uint8_t>(cols[col] & bg);
            if (overlapped == 0) {
                continue;
            }
            const int prop = g.color[static_cast<size_t>(cell)] & 7;
            const int nibbles[2] = {overlapped >> 4, overlapped & 0x0F};
            for (const int v : nibbles) {
                if (v == 0) {
                    continue;
                }
                const bool solid = v == 3 || v == 7 || v >= 11;
                const bool edge = v == 1 || v == 4 || v == 5 || v == 6 || v == 7 || v == 9 || v == 13;
                if (solid && prop == 0) {
                    hit.wall = true;
                }
                if ((solid && prop != 0) || edge) {
                    hit.ground = true;
                }
            }
        }
    }
}

}  // namespace

Contact Probe(int sx, int sy) {
    Contact hit;
    if (g.vy != 0) {
        AddPose(hit, 6, sx, sy);
    } else if (g.tick > 0) {
        const int base = g.facing < 0 ? 2 : 4;
        AddPose(hit, base, sx, sy);
        AddPose(hit, base + 1, sx, sy);
    } else {
        AddPose(hit, StandPose(), sx, sy);
    }
    return hit;
}

bool PoseWall(int pose, int sx, int sy) {
    Contact hit;
    AddPose(hit, pose, sx, sy);
    return hit.wall;
}
