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

int PlaneShift(int pose, int plane) {
    // Mode 1 pictures are 32 screen pixels wide. $25FD slides a left-facing
    // mode-1 body left by 8 VIC units, and $264B does the same for detail
    // libraries $05 (punch left) and $19 (flying kick left, leg extended).
    // 8 VIC units are 16 screen pixels, which puts the torso back over the
    // stand picture. The fist and the kicking leg hang off that side.
    if (pose == kPosePunchLeft) {
        return -16;
    }
    if (pose == kPoseFlyLeftExtended && plane == 1) {
        return -16;
    }
    return 0;
}

namespace {

bool Scripted() {
    return g.jumpTimer != 0 || g.action != 0;
}

// State $05 at $2903. The timer starts at $0F and counts down.
// $0F and 0 are library 8 (legs together). $0C-$0E and 1 are library $1E.
// 2 through $0B are library $20, the arms-out jump.
int JumpPose() {
    const int t = g.jumpTimer;
    if (t == 0 || t >= 0x0F) {
        return 6;
    }
    if (t >= 0x0C || t == 1) {
        return 7;
    }
    return 8;
}

// States $06 and $07 at $2932 and $294A. Timer >= $0C, and 0, 1, 2, use the
// tucked picture. The counts in between use the extended leg.
int SidePose() {
    const bool tucked = g.actionTimer >= 0x0C || g.actionTimer < 3;
    if (g.action == kSideLeft) {
        return tucked ? 9 : 10;
    }
    return tucked ? 11 : 12;
}

// States $0C and $0D at $288B and $289F. Timer >= $0A is the tucked kick.
// The rest of the count is the leg extended.
int FlyPose() {
    const bool tucked = g.actionTimer >= 0x0A;
    if (g.action == kFlyLeft) {
        return tucked ? 15 : 16;
    }
    return tucked ? 17 : 18;
}

void ChoosePose(bool grounded) {
    if (g.action == kPunch) {
        g.pose = g.facing < 0 ? kPosePunchLeft : kPosePunchRight;
    } else if (g.action == kFlyLeft || g.action == kFlyRight) {
        g.pose = FlyPose();
    } else if (g.action == kSideLeft || g.action == kSideRight) {
        g.pose = SidePose();
    } else if (!grounded || g.vy != 0 || g.jumpTimer != 0) {
        g.pose = JumpPose();
    } else if (g.tick > 0) {
        g.pose = WalkPose();
    } else {
        g.pose = StandPose();
    }
}

// One original $9E step is two screen pixels. A wall stops that step only.
void StepX(int pixels) {
    const int step = pixels < 0 ? -2 : 2;
    const int n = pixels < 0 ? -pixels / 2 : pixels / 2;
    for (int i = 0; i < n; ++i) {
        if (!Probe(g.x + step, g.y).wall) {
            g.x += step;
        }
    }
}

// State $05 at $2016. Y is one screen pixel. The timer is the value before
// this frame's decrement. $0F-$0D wait, $0C rises 4, $0B-$08 rise 2,
// $07-$02 drop 2, and $01 does not move. The peak is 12 pixels.
int JumpDy() {
    const int t = g.jumpTimer;
    if (t >= 0x0D) {
        return 0;
    }
    if (t == 0x0C) {
        return -4;
    }
    if (t >= 8) {
        return -2;
    }
    if (t >= 2) {
        return 2;
    }
    return 0;
}

// States $06 and $07 at $20D2 and $2120. Two $9E steps every count, and
// this vertical change. Above $0A he is still rising.
int SideDy() {
    const int t = g.actionTimer;
    if (t > 0x0A) {
        return -2;
    }
    if (t == 0x0A) {
        return -1;
    }
    if (t >= 7) {
        return 0;
    }
    if (t == 6) {
        return 1;
    }
    return 2;
}

// States $0C and $0D at $2048 and $208D. One $9E step every count.
// $0C-$0A rise one pixel, $09-$03 travel flat, and below that he drops one.
int FlyDy() {
    const int t = g.actionTimer;
    if (t >= 0x0A) {
        return -1;
    }
    if (t >= 3) {
        return 0;
    }
    return 1;
}

void ClearAction() {
    g.action = 0;
    g.actionTimer = 0;
}

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

// The count is used on this same frame, then reduced. A count of 1 is the
// last one: $1D77 does not call the mover once the timer has reached 0.
void FinishTimer() {
    if (g.actionTimer <= 1) {
        ClearAction();
    } else {
        g.actionTimer -= 1;
    }
}

}  // namespace

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

void Tick() {
    // Holding both arrows cancels the step. Facing is then to the right,
    // because that key is applied after left. A jump, kick, or punch keeps
    // the direction it started with.
    const bool left = (GetAsyncKeyState(VK_LEFT) & 0x8000) != 0;
    const bool right = (GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0;
    const bool up = (GetAsyncKeyState(VK_UP) & 0x8000) != 0;
    const bool attack = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
    const bool moving = left != right;
    const int dir = left ? -1 : 1;
    if (!Scripted()) {
        if (left) {
            g.facing = -1;
        }
        if (right) {
            g.facing = 1;
        }
        g.tick = moving ? g.tick + 1 : 0;
    }

    const Contact below = Probe(g.x, g.y + 1);
    const bool grounded = below.wall || below.ground;
    // The descent stops on the floor. The rising counts are left alone, so a
    // sideways jump or a flying kick that started on the floor can leave it.
    if ((g.action == kSideLeft || g.action == kSideRight) && grounded && g.actionTimer < 7) {
        ClearAction();
    } else if ((g.action == kFlyLeft || g.action == kFlyRight) && grounded && g.actionTimer < 3) {
        ClearAction();
    }

    if (!Scripted()) {
        if (attack && !g.attackHeld) {
            // Fire with a direction is states $0C/$0D ($1F64, $1F6D), timer $0C.
            // Fire with no direction is state $11 ($1FBC), timer 4: the punch.
            if (moving) {
                g.action = dir < 0 ? kFlyLeft : kFlyRight;
                g.actionTimer = 0x0C;
                g.facing = dir;
            } else {
                g.action = kPunch;
                g.actionTimer = 4;
            }
            g.vy = 0;
            g.tick = 0;
        } else if (up && grounded && g.vy == 0 && !g.jumpHeld) {
            // Up alone is state $05 ($1FA0). Up with left or right is $06/$07.
            if (moving) {
                g.action = dir < 0 ? kSideLeft : kSideRight;
                g.actionTimer = 0x0F;
                g.facing = dir;
            } else {
                g.jumpTimer = 0x0F;
            }
            g.vy = 0;
            g.tick = 0;
        }
    }
    g.jumpHeld = up;
    g.attackHeld = attack;

    // Chosen before the step so Probe tests the picture that will be drawn.
    ChoosePose(grounded);

    if (g.jumpTimer != 0) {
        MoveVertical(JumpDy());
        g.jumpTimer -= 1;
    } else if (g.action == kSideLeft || g.action == kSideRight) {
        MoveVertical(SideDy());
        StepX(g.action == kSideLeft ? -4 : 4);
        FinishTimer();
    } else if (g.action == kFlyLeft || g.action == kFlyRight) {
        MoveVertical(FlyDy());
        StepX(g.action == kFlyLeft ? -2 : 2);
        FinishTimer();
    } else if (g.action == kPunch) {
        FinishTimer();
    } else {
        const int dx = (right ? 2 : 0) - (left ? 2 : 0);
        // Both strides are tested while he is walking, so the wider one cannot
        // step into a wall that the other stride would be unable to leave.
        if (dx != 0 && !Probe(g.x + dx, g.y).wall) {
            g.x += dx;
        }
        if (!grounded || g.vy < 0) {
            if (g.vy < 4) {
                g.vy += 1;
            }
            MoveVertical(g.vy);
        }
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
