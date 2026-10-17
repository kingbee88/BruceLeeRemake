#include "collision.h"

#include "bruce.h"
#include "game.h"

namespace {

// Fold one pose into `hit`. Both planes are ORed, then each character
// column under the sprite is ANDed with that column's glyph byte. The two
// nibbles of the AND are what the original scan at $537E records.
// One plane can sit 16 pixels left of the other; the shift is applied
// before the planes are combined.
// $5592 rolls $4F3F in first, so that counter is bit 3 and $4F3C is bit 0.
// Bit 0 is nibbles 1, 4, 5, 6, 7, 9, and 13: don't-fall.
// The roof dots are nibble 5, so his feet rest in that brown row.
// Bit 3 is nibbles 3, 7, and 11-15 on property 0: $1CC7 pushes the move out.
// On any other property those nibbles are bit 2, a climb. The ladder rails
// and the black line of a property-6 roof are that case, so a climb passes
// through them and up or down rides them instead of jumping.
void AddPose(Contact& hit, int pose, int sx, int sy, int row0, int row1) {
    for (int y = row0; y < row1; ++y) {
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
                // Nibbles 3, 7, and 11-15 are bit 3 on property 0 and bit 2
                // on every other property. The property swap at $55E6 moves
                // the flag between those two counters. Bit 0 is the same list
                // on either property. The roof's black line is nibble 15, so
                // on a property-6 cell it is a climb, and on property 0 a wall.
                const bool solid = v == 3 || v == 7 || v >= 11;
                const bool bit0 = v == 1 || v == 4 || v == 5 || v == 6 || v == 7 || v == 9 || v == 13;
                if (solid && prop == 0) {
                    hit.wall = true;
                }
                if (solid && prop != 0) {
                    hit.ladder = true;
                }
                if (bit0) {
                    hit.ground = true;
                    if (!(solid && prop != 0)) {
                        hit.floor = true;
                    }
                }
                // $4F3D, rolled into $B6 as bit 1. $2E39 collects a lamp only
                // while this bit is set. The hanger is nibble 2.
                if (v == 2 || v == 6 || v == 8 || v == 9 || v == 10) {
                    hit.bit1 = true;
                }
            }
        }
    }
}

}  // namespace

void AddCurrent(Contact& hit, int sx, int sy, int row0, int row1) {
    // A walk tests both strides, so the wider one cannot step into a wall
    // the other stride would be unable to leave. Every other state uses the
    // picture already chosen.
    if (g.action == kWalkLeft || g.action == kWalkRight) {
        const int base = g.facing < 0 ? 2 : 4;
        AddPose(hit, base, sx, sy, row0, row1);
        AddPose(hit, base + 1, sx, sy, row0, row1);
    } else {
        AddPose(hit, g.pose, sx, sy, row0, row1);
    }
}

Contact Probe(int sx, int sy) {
    Contact hit;
    AddCurrent(hit, sx, sy, 0, kSpriteRows);
    return hit;
}

Contact ProbeFeet(int sx, int sy) {
    Contact hit;
    // The legs. A full-sprite probe is what climb and landing use.
    AddCurrent(hit, sx, sy, 16, kSpriteRows);
    return hit;
}

Contact ProbeUpper(int sx, int sy) {
    Contact hit;
    AddCurrent(hit, sx, sy, 0, kSpriteRows - 6);
    return hit;
}

Contact ProbePose(int pose, int sx, int sy) {
    Contact hit;
    AddPose(hit, pose, sx, sy, 0, kSpriteRows);
    return hit;
}

bool PoseWall(int pose, int sx, int sy) {
    Contact hit;
    AddPose(hit, pose, sx, sy, 0, kSpriteRows);
    return hit.wall;
}
