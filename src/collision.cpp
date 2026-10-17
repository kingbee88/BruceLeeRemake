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
// Bit 0 is nibbles 1, 4, 5, 6, 7, 9, and 13: don't-fall, and not a wall.
// The roof dots are nibble 5, so his feet rest in that brown row.
// Bit 3 is nibbles 3, 7, and 11-15: $1CC7 pushes the move back out.
// On this screen those shapes are a wall only on property 0. The ladder
// cap is property 6, so a climb can pass it and he is not pinned to the
// black line beside the hole.
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
            // The ladder in room 0. It supports him, and it is not a wall,
            // which is why he already sticks to it. $2C is the bottom of it.
            const bool ladderTile = tile == 0x20 || tile == 0x21 || tile == 0x2C;
            if (ladderTile) {
                hit.ladder = true;
            }
            const int prop = g.color[static_cast<size_t>(cell)] & 7;
            const int nibbles[2] = {overlapped >> 4, overlapped & 0x0F};
            for (const int v : nibbles) {
                if (v == 0) {
                    continue;
                }
                const bool pushOut = v == 3 || v == 7 || v >= 11;
                const bool support = v == 1 || v == 4 || v == 5 || v == 6 || v == 7 || v == 9 || v == 13;
                if (pushOut && prop == 0 && !ladderTile) {
                    hit.wall = true;
                }
                if (support) {
                    hit.ground = true;
                    if (!ladderTile) {
                        hit.floor = true;
                    }
                }
            }
        }
    }
}

}  // namespace

void AddCurrent(Contact& hit, int sx, int sy, int row0, int row1) {
    // g.pose was chosen for this frame before the step. The jump, the sideways
    // jump, the flying kick, the punch, and a climb are wider than the fall.
    if (g.action != 0 || g.vy != 0 || g.jumpTimer != 0 || g.climbHold != 0) {
        AddPose(hit, g.pose, sx, sy, row0, row1);
    } else if (g.tick > 0) {
        const int base = g.facing < 0 ? 2 : 4;
        AddPose(hit, base, sx, sy, row0, row1);
        AddPose(hit, base + 1, sx, sy, row0, row1);
    } else {
        AddPose(hit, StandPose(), sx, sy, row0, row1);
    }
}

Contact Probe(int sx, int sy) {
    Contact hit;
    AddCurrent(hit, sx, sy, 0, kSpriteRows);
    return hit;
}

Contact ProbeFeet(int sx, int sy) {
    Contact hit;
    // The legs. Used so a climb down stops on the floor under him, not on
    // the platform his head is still overlapping at the top of the ladder.
    AddCurrent(hit, sx, sy, 16, kSpriteRows);
    return hit;
}

bool PoseWall(int pose, int sx, int sy) {
    Contact hit;
    AddPose(hit, pose, sx, sy, 0, kSpriteRows);
    return hit.wall;
}
