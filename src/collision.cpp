#include "collision.h"

#include "bruce.h"
#include "game.h"

namespace {

// Fold one pose into `hit`. Both planes are ORed, then each character
// column under the sprite is ANDed with that column's glyph byte. The two
// nibbles of the AND are what the original scan at $537E records.
// One plane can sit 16 pixels left of the other; the shift is applied
// before the planes are combined.
// Nibbles 3, 7, and 11-15 are solid: a wall on property 0, a floor on any
// other property. Nibbles 1, 4, 5, 6, 9, and 13 are the floor edge.
// Other patterns, including the statue, do not set either flag.
void AddPose(Contact& hit, int pose, int sx, int sy) {
    for (int y = 0; y < kSpriteRows; ++y) {
        const int py = sy + y;
        uint8_t cols[40] = {};
        bool used[40] = {};
        for (int plane = 0; plane < 2; ++plane) {
            const uint8_t* frame = PoseBytes(pose, plane);
            const int ox = sx + PlaneShift(pose, plane);
            for (int x = 0; x < kSpriteCols; ++x) {
                const uint8_t bits = frame[y * kSpriteRowBytes + x / 8];
                if ((bits & static_cast<uint8_t>(0x80 >> (x & 7))) == 0) {
                    continue;
                }
                const int px = ox + x;
                if (px < 0 || px >= kWidth || py < 0 || py >= kHeight) {
                    hit.wall = true;
                    continue;
                }
                const int col = px / 8;
                cols[col] |= static_cast<uint8_t>(0x80 >> (px & 7));
                used[col] = true;
            }
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
    // g.pose was chosen for this frame before the step. The jump, the sideways
    // jump, the flying kick, and the punch are wider than the fall.
    if (g.action != 0 || g.vy != 0 || g.jumpTimer != 0) {
        AddPose(hit, g.pose, sx, sy);
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
