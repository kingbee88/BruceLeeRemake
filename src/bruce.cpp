#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "bruce.h"

#include "collision.h"
#include "draw.h"
#include "game.h"

int StandPose() {
    return g.facing < 0 ? 1 : 0;
}

int WalkPose() {
    const int step = (g.tick / 6) & 1;
    return g.facing < 0 ? (2 + step) : (4 + step);
}

const uint8_t* PoseBytes(int pose, int plane) {
    return g.frames.data() + static_cast<size_t>(pose * 2 + plane) * kFrameBytes;
}

void PlaceBruce() {
    // Just to the right of the lamp, feet on the red line at the bottom.
    g.x = 36;
    g.y = 194 - kSpriteRows;
    for (int n = 0; n < 48 && PoseWall(g.pose, g.x, g.y); ++n) {
        g.y -= 1;
    }
}

namespace {

void MoveVertical(int dy) {
    const int step = dy > 0 ? 1 : -1;
    const int n = dy > 0 ? dy : -dy;
    for (int i = 0; i < n; ++i) {
        const Contact hit = Probe(g.x, g.y + step);
        // Down stops on the don't-fall flags as well as a wall. Up is
        // cancelled only by the solid flag, the same bit $1CC7 pushes out.
        if (step > 0 ? (hit.wall || hit.ground) : hit.wall) {
            g.vy = 0;
            return;
        }
        g.y += step;
    }
}

}  // namespace

void Tick() {
    const bool left = (GetAsyncKeyState(VK_LEFT) & 0x8000) != 0;
    const bool right = (GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0;
    const bool up = (GetAsyncKeyState(VK_UP) & 0x8000) != 0;
    if (left) {
        g.facing = -1;
    }
    if (right) {
        g.facing = 1;
    }
    const int dx = (right ? 2 : 0) - (left ? 2 : 0);
    if (dx != 0) {
        g.tick += 1;
    } else {
        g.tick = 0;
    }
    // Both strides are tested while he is walking, so the wider one cannot
    // step into a wall that the other stride would be unable to leave.
    if (dx != 0 && !Probe(g.x + dx, g.y).wall) {
        g.x += dx;
    }

    const Contact below = Probe(g.x, g.y + 1);
    const bool grounded = below.wall || below.ground;
    if (up && grounded && g.vy == 0 && !g.jumpHeld) {
        g.vy = -8;
    }
    g.jumpHeld = up;
    if (!grounded || g.vy < 0) {
        if (g.vy < 4) {
            g.vy += 1;
        }
        MoveVertical(g.vy);
    }
    if (g.vy != 0) {
        g.pose = 6;
    } else if (g.tick > 0) {
        g.pose = WalkPose();
    } else {
        g.pose = StandPose();
    }
    // A new pose can overlap a wall the previous pose cleared.
    // $1CC7 pushes him back out when the solid flag is set.
    if (PoseWall(g.pose, g.x, g.y)) {
        const int prefer = g.facing < 0 ? 1 : -1;
        const int start = g.x;
        for (int i = 1; i <= kSpriteCols && PoseWall(g.pose, g.x, g.y); ++i) {
            if (!PoseWall(g.pose, start + prefer * i, g.y)) {
                g.x = start + prefer * i;
            } else if (!PoseWall(g.pose, start - prefer * i, g.y)) {
                g.x = start - prefer * i;
            }
        }
    }
    Compose();
}
