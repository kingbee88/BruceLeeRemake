// Bruce Lee, a Windows port.
// Copyright (C) 2026 Jacob Marner
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// No copyright is claimed over the original game, which is copyright 1984 Datasoft, Inc.

#include "fighter.h"

#include "bruce.h"
#include "collision.h"
#include "config.h"
#include "game.h"
#include "load.h"
#include "room.h"
#include "sound.h"

namespace {

constexpr int kMinX = 2;
constexpr int kMaxX = 302;
constexpr int kMinY = 25;
constexpr int kMaxY = 178;
constexpr int kStepUp = 0;
constexpr int kStepDown = 1;
constexpr int kStepLeft = 2;
constexpr int kStepRight = 3;
constexpr int kStepGens = 11;

// $135B and $131F. Counts down while that fighter is gone. 0 spawns him.
constexpr uint8_t kNinjaDelay[20] = {
    0xEE, 0xEE, 0xEE, 0x00, 0xEE, 0x60, 0xEE, 0xC0, 0x00, 0x00,
    0x30, 0x00, 0x20, 0xEE, 0x30, 0x50, 0x30, 0xEE, 0x00, 0x00};
constexpr uint8_t kYamoDelay[20] = {
    0xA0, 0xA0, 0xA0, 0x00, 0x70, 0x0C, 0x30, 0x18, 0x00, 0x00,
    0x0C, 0x00, 0x04, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00};
// $136F for the Ninja. $1333 is Yamo's first test; the sign still comes from $136F.
constexpr uint8_t kNinjaFlag[20] = {
    0x80, 0x80, 0x00, 0x00, 0x80, 0x02, 0x00, 0x80, 0x00, 0x00,
    0x80, 0x00, 0x80, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00};
constexpr uint8_t kYamoFlag[20] = {
    0x02, 0x02, 0x00, 0x00, 0x80, 0x80, 0x02, 0x02, 0x00, 0x00,
    0x02, 0x00, 0x80, 0x00, 0x02, 0x00, 0x80, 0x00, 0x00, 0x00};
constexpr uint8_t kMainX[20] = {
    0x1A, 0x18, 0x30, 0x20, 0x94, 0x22, 0x86, 0x1C, 0x14, 0x12,
    0x34, 0x40, 0x18, 0x98, 0x18, 0x1C, 0x18, 0x18, 0x18, 0x12};
constexpr uint8_t kMainY[20] = {
    0xC4, 0x62, 0xC4, 0xC0, 0xB4, 0x51, 0xB3, 0x44, 0x46, 0xC6,
    0x8C, 0x4C, 0x5C, 0x32, 0x5C, 0xC3, 0x5C, 0xB4, 0xC6, 0x95};
constexpr uint8_t kAltX[20] = {
    0x9F, 0x95, 0x96, 0x38, 0x96, 0x28, 0x81, 0x1F, 0x00, 0x00,
    0x96, 0x00, 0x70, 0x00, 0x92, 0x7A, 0x81, 0x00, 0x00, 0x00};
constexpr uint8_t kAltY[20] = {
    0x62, 0x92, 0xC4, 0xC4, 0xB4, 0xC2, 0x72, 0x74, 0x00, 0x00,
    0x5C, 0x00, 0x5C, 0x00, 0xBE, 0xC4, 0x8E, 0x00, 0x00, 0x00};

// $5C33 for the Ninja's libraries. Poses 16, 18, and 33 swap the planes.
int NinjaInk(int pose, int plane) {
    const bool swap = pose == 16 || pose == 18 || pose == kPoseKnockLeft1;
    if (swap) {
        return plane == 0 ? 0 : 5;
    }
    return plane == 0 ? 5 : 0;
}

struct Fighter {
    int kind = 0;
    bool on = false;
    int delay = 0;
    int x = 0;
    int y = 0;
    int facing = 1;
    int pose = 0;
    int action = 0;
    int actionTimer = 0;
    int health = 0;
    int step = 0;
    int carryDelay = 0;  // $AD. Counts down between his own steps on the ladder.
    int carryMark = 0;   // $AA. Set while state $17 runs. A step shifts it to 0.
    int repeat = 6;      // $CB. A walk burst is two xpos, or three once this hits 0.
    int prevAction = 0;  // $C2. The state release_hold saw last frame.
    int hist[kStepGens][4] = {};
    const uint8_t* frames = nullptr;
};

std::vector<uint8_t> ninjaBytes;
std::vector<uint8_t> yamoBytes;
Fighter ninja;
Fighter yamo;
uint32_t rng = 0xA5C31E89u;

int Noise() {
    rng = rng * 1664525u + 1013904223u;
    return static_cast<int>((rng >> 16) & 0xFF);
}

int XposOf(int x) {
    return (x + 24) / 2;
}

int YposOf(int y) {
    return y + 21;
}

// $2DDD. Bit 7 is the sign. The magnitude stops at $30.
int Packed(int mine, int his) {
    int d = mine - his;
    const bool neg = d < 0;
    if (neg) {
        d = -d;
    }
    if (d > 0x30) {
        d = 0x30;
    }
    return neg ? (d | 0x80) : d;
}

void Bind(const Fighter& f) {
    SetActorView(f.frames, f.action, f.facing, f.pose);
}

Contact At(const Fighter& f) {
    Bind(f);
    const Contact hit = Probe(f.x, f.y);
    ClearActorView();
    return hit;
}

void StepX(Fighter& f, int dir) {
    if (dir < 0) {
        if (f.x < kMinX) {
            return;
        }
        f.x -= 2;
        f.hist[0][kStepLeft] += 1;
    } else {
        if (f.x >= kMaxX) {
            return;
        }
        f.x += 2;
        f.hist[0][kStepRight] += 1;
    }
    f.facing = dir;
}

void StepY(Fighter& f, int dir) {
    if (dir < 0) {
        // move_up refuses ypos below $2E. ypos $2E itself still steps.
        if (f.y < kMinY) {
            return;
        }
        f.y -= 1;
        f.hist[0][kStepUp] += 1;
    } else if (f.y < kMaxY) {
        f.y += 1;
        f.hist[0][kStepDown] += 1;
    }
}

// $1C77. A frame that stepped shifts the counters into the older slots.
// A frame that did not leaves those slots where they are.
void RememberSteps(Fighter& f) {
    bool any = false;
    for (int d = 0; d < 4; ++d) {
        if (f.hist[0][d] != 0) {
            any = true;
        }
    }
    if (any) {
        for (int gen = kStepGens - 1; gen > 0; --gen) {
            for (int d = 0; d < 4; ++d) {
                f.hist[gen][d] = f.hist[gen - 1][d];
            }
        }
    }
    for (int d = 0; d < 4; ++d) {
        f.hist[0][d] = 0;
    }
}

bool OlderSteps(const Fighter& f) {
    for (int gen = 1; gen < kStepGens; ++gen) {
        for (int d = 0; d < 4; ++d) {
            if (f.hist[gen][d] != 0) {
                return true;
            }
        }
    }
    return false;
}

void SlideAfterPush(Fighter& f) {
    int next[kStepGens][4] = {};
    for (int gen = 0; gen <= 8; ++gen) {
        for (int d = 0; d < 4; ++d) {
            next[gen][d] = f.hist[gen + 2][d];
        }
    }
    for (int gen = 0; gen < kStepGens; ++gen) {
        for (int d = 0; d < 4; ++d) {
            f.hist[gen][d] = next[gen][d];
        }
    }
}

// $1CC7. The steps that put him in the wall are walked back: up by moving
// down, down by moving up, left by moving right, right by moving left.
// State $18 and timer $40. While that timer is running he does not chase.
void PushOut(Fighter& f, const Contact& here) {
    int tries = 8;
    for (;;) {
        const int up = f.hist[1][kStepUp];
        const int down = f.hist[1][kStepDown];
        const int left = f.hist[1][kStepLeft];
        const int right = f.hist[1][kStepRight];
        const bool sideways = down || left || right;
        for (int i = 0; i < up; ++i) {
            StepY(f, 1);
        }
        for (int i = 0; i < down; ++i) {
            StepY(f, -1);
        }
        for (int i = 0; i < left; ++i) {
            StepX(f, 1);
        }
        for (int i = 0; i < right; ++i) {
            StepX(f, -1);
        }
        SlideAfterPush(f);
        tries -= 1;
        if (tries < 0 || here.ladder) {
            break;
        }
        if (!sideways) {
            RememberSteps(f);
            continue;
        }
        if (tries != 7) {
            StepX(f, f.facing < 0 ? 1 : -1);
        }
        break;
    }
    f.action = kStuck;
    f.actionTimer = 0x40;
}

void BeginFall(Fighter& f) {
    f.action = kFall;
    f.actionTimer = 0;
}

void ContinueFall(Fighter& f) {
    const Contact here = At(f);
    if (here.ground || here.ladder) {
        f.action = 0;
        f.actionTimer = 0;
        return;
    }
    const int n = f.actionTimer == 0 ? 1 : 2;
    f.actionTimer = 1;
    for (int i = 0; i < n; ++i) {
        StepY(f, 1);
    }
    f.action = kFall;
}

// L_1E6D and L_1EAE. Two xpos, and a third when $CB has reached 0.
// chase_walk then keeps the timer: the INC before keep_state cancels its DEC.
void WalkBurst(Fighter& f) {
    const int dir = f.facing < 0 ? -1 : 1;
    const int n = f.repeat == 0 ? 3 : 2;
    for (int i = 0; i < n; ++i) {
        StepX(f, dir);
    }
}

void StartSidestep(Fighter& f, int dx) {
    int dest = XposOf(g.x);
    if (dx & 0x80) {
        dest -= 0x0F;
    } else {
        dest += 0x0F;
    }
    int dist = dest - XposOf(f.x);
    if (dist < 0) {
        dist = -dist;
    }
    if (dist < 5) {
        dist = 5;
    }
    f.action = 0x15;
    f.actionTimer = dist;
}

void Chase(Fighter& f) {
    const Contact here = At(f);
    if (!here.ground) {
        if (here.ladder) {
            WalkBurst(f);
            f.action = 0x14;
            return;
        }
        BeginFall(f);
        ContinueFall(f);
        return;
    }
    const int dy = Packed(YposOf(f.y), YposOf(g.y));
    const int dx = Packed(XposOf(f.x), XposOf(g.x));
    const int adx = dx & 0x7F;
    if ((dy & 0x7F) >= 0x10) {
        if (dy & 0x80) {
            WalkBurst(f);
            f.action = 0x14;
            return;
        }
        f.facing = (dx & 0x80) ? 1 : -1;
        f.action = 0;
        f.actionTimer = 6;
        return;
    }
    int faceTest = f.facing < 0 ? 0 : 1;
    if ((dx & 0x80) == 0) {
        faceTest ^= 1;
    }
    if (faceTest != 1) {
        if (adx >= 0x21) {
            f.facing = f.facing < 0 ? 1 : -1;
            WalkBurst(f);
            f.action = 0x14;
            return;
        }
        StartSidestep(f, dx);
        return;
    }
    if (f.kind == 1) {
        if (adx < 5) {
            StartSidestep(f, dx);
            return;
        }
        if (adx < 0x0C) {
            f.action = kPunch;
            f.actionTimer = 4;
            return;
        }
        if (adx < 0x13) {
            f.action = f.facing < 0 ? kFlyLeft : kFlyRight;
            f.actionTimer = 0x0C;
            return;
        }
    } else if (adx >= 0x0B) {
        WalkBurst(f);
        f.action = 0x14;
        return;
    } else if (adx < 5) {
        StartSidestep(f, dx);
        return;
    } else {
        f.action = 0x16;
        f.actionTimer = 0x13;
        return;
    }
    WalkBurst(f);
    f.action = 0x14;
}

void Sidestep(Fighter& f) {
    const Contact here = At(f);
    // $23D1. Bit 0 only. A ladder without a floor is a fall.
    if (!here.ground) {
        BeginFall(f);
        ContinueFall(f);
        return;
    }
    if (f.actionTimer <= 1) {
        f.facing = f.facing < 0 ? 1 : -1;
        f.action = 0x14;
        f.actionTimer = 0x80;
        return;
    }
    WalkBurst(f);
    f.action = 0x15;
    f.actionTimer -= 1;
}

void RecoverKick(Fighter& f) {
    // $2DC0 stores $18 over the stand, and only after enemy_turn has seen
    // ground or a ladder. In the air the next frame is a fall.
    const Contact here = At(f);
    f.action = 0;
    f.actionTimer = (here.ground || here.ladder) ? 0x18 : 0;
}

void Fly(Fighter& f) {
    const Contact here = At(f);
    const int t = f.actionTimer;
    if (here.ladder || here.wall || t <= 0) {
        RecoverKick(f);
        return;
    }
    // $2058 and $209D. Shared with Bruce. Timer $0C is the first kick frame.
    if (t == 0x0C) {
        PlayEffect(0x84);
    }
    const int dir = f.action == kFlyLeft ? -1 : 1;
    if (t >= 0x0A) {
        StepY(f, -1);
        StepX(f, dir);
    } else if (t >= 3) {
        StepX(f, dir);
    } else if (here.ground) {
        RecoverKick(f);
        return;
    } else {
        StepY(f, 1);
        StepX(f, dir);
    }
    f.actionTimer -= 1;
    if (f.actionTimer <= 0) {
        RecoverKick(f);
    }
}

void Punch(Fighter& f) {
    const Contact here = At(f);
    // $2270. A wall or a ladder is land, timer 0. Timer 1 is $228C: state 0
    // and a 6-frame stand, and only while the floor or a ladder is there.
    // A ladder or a wall is land, timer 0. Timer 1 is stand_down: state 0
    // and timer 6. That stand does not ask whether he is on the floor.
    if (here.ladder || here.wall) {
        f.action = 0;
        f.actionTimer = 0;
        return;
    }
    if (f.actionTimer <= 1) {
        f.action = 0;
        f.actionTimer = 6;
        return;
    }
    f.actionTimer -= 1;
}

void Flinch(Fighter& f) {
    // $2415. Under 2 the stand from $228C runs, timer 6.
    if (f.actionTimer < 2) {
        f.action = 0;
        f.actionTimer = 6;
        return;
    }
    const int face = f.facing;
    StepX(f, face < 0 ? 1 : -1);
    StepX(f, face < 0 ? 1 : -1);
    f.facing = face;
    f.action = kFlinch;
    f.actionTimer -= 1;
}

void Knock(Fighter& f) {
    // $244B. A living fighter leaves once the timer is under 2. Negative
    // health stays here until the count is gone, which is the knockdown.
    if (f.health >= 0 && f.actionTimer < 2) {
        f.action = 0;
        f.actionTimer = 6;
        return;
    }
    if (f.actionTimer <= 0) {
        f.action = 0;
        return;
    }
    const Contact here = At(f);
    if (!here.ground && !here.ladder) {
        StepY(f, 1);
    }
    const int face = f.facing;
    const int back = face < 0 ? 1 : -1;
    // $2462. Yamo, and only on count $0A, is thrown further than the others.
    if (f.actionTimer == 0x0A && f.kind == 2) {
        const int extra = back < 0 ? 8 : 4;
        for (int i = 0; i < extra; ++i) {
            StepX(f, back);
        }
        f.facing = face;
    }
    StepX(f, back);
    StepX(f, back);
    f.facing = face;
    f.action = kKnock;
    f.actionTimer -= 1;
}

void StompPose(Fighter& f) {
    if (f.actionTimer <= 0) {
        f.action = 0;
        return;
    }
    f.action = kStomp;
    f.actionTimer -= 1;
}

void Appear(Fighter& f) {
    if (f.action == 0x10) {
        if (f.actionTimer <= 1) {
            // $225D. The frame that leaves state $10.
            PlayEffect(0x81);
            f.facing = f.x >= g.x ? -1 : 1;
            f.action = 0x12;
            f.actionTimer = 0x10;
            return;
        }
        f.actionTimer -= 1;
        return;
    }
    if (f.actionTimer <= 1) {
        f.action = 0;
        f.actionTimer = 6;
        return;
    }
    f.actionTimer -= 1;
}

void Strike(Fighter& f) {
    const Contact here = At(f);
    // $22C1. Bit 3 cancels it. Timer 1 is the same 6-frame stand as a punch.
    // Bit 3 is land, timer 0. Timer 1 is stand_down, timer 6, with no
    // ground test. A shorter stand is why he used to strike again at once.
    // The other frames only add a step to the push counter. xpos stays put.
    if (here.wall) {
        f.action = 0;
        f.actionTimer = 0;
        return;
    }
    // $22D0. Yamo's strike, the frame whose timer is still $0C.
    if (f.actionTimer == 0x0C) {
        PlayEffect(0x85);
    }
    if (f.actionTimer <= 1) {
        f.action = 0;
        f.actionTimer = 6;
        return;
    }
    f.hist[0][f.facing < 0 ? kStepLeft : kStepRight] += 1;
    f.actionTimer -= 1;
}

void Idle(Fighter& f) {
    // State 0 with a timer is the stand after a punch, a kick, or a spawn.
    // move_for_state only counts it down. The fall waits until it is 0.
    if (f.actionTimer > 0) {
        f.actionTimer -= 1;
        return;
    }
    const Contact here = At(f);
    if (!here.ground && !here.ladder) {
        BeginFall(f);
        ContinueFall(f);
        return;
    }
    f.action = 0x14;
    f.actionTimer = 0x14;
    Chase(f);
}

// $2E52 clears state $17 before the cells are read. $310C puts it back when
// one of those cells is tile $62 and hit bit 2 is set. Timer $10, and $AA
// cleared. The same test catches Bruce in TouchRoomTile.
void CatchSurface(Fighter& f) {
    if (f.action == kCarry) {
        f.action = 0;
        f.actionTimer = 0;
    }
    if (f.action == kZap || !f.on) {
        return;
    }
    const int xpos = XposOf(f.x);
    const int ypos = YposOf(f.y);
    if (xpos < 0x0C) {
        return;
    }
    int rowY = ypos;
    if (g.roomId == 0x0D) {
        rowY += 4;
    }
    if (rowY < 0x0C) {
        return;
    }
    const Contact pose = At(f);
    if (!pose.ladder) {
        return;
    }
    const int col0 = (xpos - 0x0C) >> 2;
    const int row0 = (rowY - 0x0C) >> 4;
    for (int dr = 0; dr < 2; ++dr) {
        for (int dc = 0; dc < 3; ++dc) {
            if (TileAt(col0 + dc, row0 + dr) != 0x62) {
                continue;
            }
            f.action = kCarry;
            f.actionTimer = 0x10;
            f.carryMark = 0;
            return;
        }
    }
}

// $2187. Bit 2 clear leaves the ladder. A computer fighter's stick is the way
// he is facing, right $07 or left $0B, and that step waits on $AD. $60 == 1
// then moves him two pixels with the surface, up when $65 is clear.
void CarryFighter(Fighter& f) {
    // Bit 2 was already required to get here, from the overlap before the
    // move. $1CC7, not this routine, deals with a wall. choose_pose reads
    // $AA after this and flips libraries $0A and $0C only when a step
    // shifted it back to 0.
    f.carryMark = 1;
    const int dir = f.facing < 0 ? -1 : 1;
    if (f.carryDelay > 0) {
        f.carryDelay -= 1;
    } else {
        StepX(f, dir);
        StepX(f, dir);
        // $21EB. The same blip as Bruce's own step on the ladder.
        PlayEffect(0x80);
        f.carryMark >>= 1;
        f.carryDelay = 4;
    }
    const int shove = SurfaceShove();
    if (shove < 0) {
        StepY(f, -1);
        StepY(f, -1);
    } else if (shove > 0) {
        StepY(f, 1);
        StepY(f, 1);
    }
    f.action = kCarry;
}

void Act(Fighter& f) {
    if (f.actionTimer > 0 && f.action != 0 && f.action != kFall) {
        switch (f.action) {
        case kFlyLeft:
        case kFlyRight:
            Fly(f);
            break;
        case kPunch:
            Punch(f);
            break;
        case kFlinch:
            Flinch(f);
            break;
        case kKnock:
            Knock(f);
            break;
        case kStomp:
            StompPose(f);
            break;
        case 0x10:
        case 0x12:
            Appear(f);
            break;
        case 0x14:
            Chase(f);
            break;
        case 0x15:
            Sidestep(f);
            break;
        case 0x16:
            Strike(f);
            break;
        default:
            f.actionTimer -= 1;
            break;
        }
        return;
    }
    if (f.action == kFall) {
        ContinueFall(f);
        return;
    }
    Idle(f);
}

void Choose(Fighter& f) {
    const Contact here = At(f);
    const int t = f.actionTimer;
    if (f.action == kPunch) {
        f.pose = f.facing < 0 ? kPosePunchLeft : kPosePunchRight;
    } else if (f.action == kFlyLeft || f.action == kFlyRight) {
        const bool tucked = t >= 0x0A;
        f.pose = f.action == kFlyLeft ? (tucked ? 15 : 16) : (tucked ? 17 : 18);
    } else if (f.action == kFall) {
        f.pose = here.wall ? kPoseLegs : kPoseArms;
    } else if (f.action == 0x10) {
        f.pose = kPoseSpawn;
    } else if (f.action == 0x12) {
        f.pose = f.facing < 0 ? kPoseAppearLeft : kPoseAppearRight;
    } else if (f.action == 0x16) {
        const bool early = t >= 0x0B;
        if (f.facing < 0) {
            f.pose = early ? kPoseStrikeLeft0 : kPoseStrikeLeft1;
        } else {
            f.pose = early ? kPoseStrikeRight0 : kPoseStrikeRight1;
        }
    } else if (f.action == kKnock || f.action == kStomp) {
        const bool early = f.action == kKnock ? t >= 0x0A : t >= 4;
        if (f.facing < 0) {
            f.pose = early ? kPoseKnockLeft0 : kPoseKnockLeft1;
        } else {
            f.pose = early ? kPoseKnockRight0 : kPoseKnockRight1;
        }
    } else if (f.action == kStuck) {
        // $27C1. The picture stays. The arms-out jump is replaced by the legs.
        if (f.pose == kPoseArms) {
            f.pose = kPoseLegs;
        }
    } else if (f.action == kCarry) {
        // $2962. $AA still set keeps the picture already showing. Cleared,
        // the picture becomes $0A, or $0C when it was already $0A.
        if (f.carryMark == 0) {
            f.pose = f.pose == kPoseClimb0 ? kPoseClimb1 : kPoseClimb0;
        }
    } else if (f.action == 0x14 || f.action == 0x15) {
        if (here.ladder) {
            f.pose = (f.step & 1) ? kPoseClimb1 : kPoseClimb0;
        } else {
            const int base = f.facing < 0 ? 2 : 4;
            f.pose = base + (f.step & 1);
        }
        f.step += 1;
    } else if (here.ladder) {
        f.pose = (f.step & 1) ? kPoseClimb1 : kPoseClimb0;
    } else {
        f.pose = f.facing < 0 ? 1 : 0;
    }
}

bool UseAlt(uint8_t flag) {
    // $0C09. A zero flag, or a noise byte at $C0 or above, picks a side at random.
    // Otherwise bit 7 selects the alternate spawn.
    if (flag != 0 && Noise() < 0xC0) {
        return (flag & 0x80) != 0;
    }
    return (Noise() & 1) != 0;
}

bool AltSpawn(int kind) {
    const int room = g.roomId;
    // Yamo reads $1333 first. Zero, or a noise byte at $C0 or above, is a coin
    // toss. Otherwise the sign of the Ninja's $136F byte picks the spawn.
    if (kind == 2) {
        if (kYamoFlag[room] == 0 || Noise() >= 0xC0) {
            return (Noise() & 1) != 0;
        }
        return (kNinjaFlag[room] & 0x80) != 0;
    }
    return UseAlt(kNinjaFlag[room]);
}

void Wake(Fighter& f) {
    // $10BA. Both spawns.
    PlayEffect(0x82);
    const int room = g.roomId;
    const bool alt = AltSpawn(f.kind);
    const int xpos = alt ? kAltX[room] : kMainX[room];
    const int ypos = alt ? kAltY[room] : kMainY[room];
    f.x = xpos * 2 - 24;
    f.y = ypos - 21;
    f.facing = 1;
    f.pose = 0;
    f.step = 0;
    f.carryDelay = 0;
    f.carryMark = 0;
    f.repeat = 6;
    f.prevAction = 0;
    for (int gen = 0; gen < kStepGens; ++gen) {
        for (int d = 0; d < 4; ++d) {
            f.hist[gen][d] = 0;
        }
    }
    f.health = 0x24;
    f.on = true;
    if (f.kind == 1) {
        // $0CEA. State $10 is the materialize, and $D1 stops at $14.
        f.action = 0x10;
        f.actionTimer = 0x0A;
        f.health = 0x14;
        f.pose = kPoseSpawn;
    } else {
        // $0E17. He stands for $10 frames, facing Bruce, with $D1 stopped at 7.
        f.action = 0;
        f.actionTimer = 0x10;
        f.health = 7;
        f.facing = f.x >= g.x ? -1 : 1;
        f.pose = f.facing < 0 ? 1 : 0;
    }
}

int RoomDelay(int kind) {
    const int room = g.roomId;
    if (room < 0 || room >= 20) {
        return 0;
    }
    return kind == 1 ? kNinjaDelay[room] : kYamoDelay[room];
}

void Bury(Fighter& f) {
    f.on = false;
    f.action = 0;
    f.actionTimer = 0;
    // $3001 jumps to $0D42, which reloads this fighter's wait.
    f.delay = RoomDelay(f.kind);
}

void Spawn(Fighter& f) {
    if (f.on || f.frames == nullptr || f.delay > 0) {
        return;
    }
    Wake(f);
    f.delay = RoomDelay(f.kind);
}

struct Who {
    int kind = 0;
    Fighter* fighter = nullptr;
    int x = 0;
    int y = 0;
    int facing = 1;
    int action = 0;
    int timer = 0;
    int pose = 0;
    int health = 0;
    const uint8_t* frames = nullptr;
};

Who ReadBruce() {
    Who w;
    w.kind = 0;
    w.x = g.x;
    w.y = g.y;
    w.facing = g.facing;
    w.action = g.action;
    w.timer = g.actionTimer;
    w.pose = g.pose;
    w.health = g.health;
    w.frames = g.frames.data();
    return w;
}

Who ReadFighter(Fighter& f) {
    Who w;
    w.kind = f.kind;
    w.fighter = &f;
    w.x = f.x;
    w.y = f.y;
    w.facing = f.facing;
    w.action = f.action;
    w.timer = f.actionTimer;
    w.pose = f.pose;
    w.health = f.health;
    w.frames = f.frames;
    return w;
}

bool Pixel(const Who& w, int plane, int x, int y) {
    if (!w.frames || x < 0 || y < 0 || x >= kSpriteCols || y >= kSpriteRows) {
        return false;
    }
    const uint8_t* frame = w.frames + static_cast<size_t>(w.pose * 2 + plane) * kFrameBytes;
    return (frame[y * kSpriteRowBytes + x / 8] & static_cast<uint8_t>(0x80 >> (x & 7))) != 0;
}

// $25FD slides a left-facing mode-1 body. Bruce's copy of these libraries is
// mode 0, so PlaneShift leaves them put. The Ninja's extended kick and late
// knockdown, and three of Yamo's pictures, are mode 1.
int PictureShift(int kind, int facing, int pose, int plane) {
    const int shift = PlaneShift(pose, plane);
    if (facing > 0) {
        return shift;
    }
    if (kind == 1 && plane == 0 && (pose == kPoseFlyLeftExtended || pose == kPoseKnockLeft1)) {
        return -16;
    }
    if (kind == 2 && plane == 0 &&
        (pose == kPoseStrikeLeft1 || pose == kPoseKnockLeft0 || pose == kPoseKnockLeft1)) {
        return -16;
    }
    return shift;
}

bool Covered(const Who& w, int sx, int sy) {
    for (int plane = 0; plane < 2; ++plane) {
        const int x = sx - (w.x + PictureShift(w.kind, w.facing, w.pose, plane));
        const int y = sy - w.y;
        if (Pixel(w, plane, x, y)) {
            return true;
        }
    }
    return false;
}

bool Touches(const Who& a, const Who& b) {
    for (int plane = 0; plane < 2; ++plane) {
        const int ox = a.x + PictureShift(a.kind, a.facing, a.pose, plane);
        for (int y = 0; y < kSpriteRows; ++y) {
            for (int x = 0; x < kSpriteCols; ++x) {
                if (!Pixel(a, plane, x, y)) {
                    continue;
                }
                if (Covered(b, ox + x, a.y + y)) {
                    return true;
                }
            }
        }
    }
    return false;
}

// $2C54. Positive means the victim is in front of the attacker. A negative
// distance, or one past $7F, is behind him or too far for this blow.
bool InFront(const Who& attacker, const Who& victim, int reach) {
    int d = XposOf(attacker.x) - XposOf(victim.x);
    if (attacker.facing > 0) {
        d = -d;
    }
    return d >= reach && d < 0x80;
}

bool Open(int action) {
    return action != kStuck && action != kDuck && action != kFlinch && action != kKnock &&
           action != kZap && action != kStomp;
}

// Writes the hurt back. True when Bruce has just lost a fall.
bool Hurt(Who& victim, const Who& attacker, int blow) {
    // $29E2 and $29F7. The trainer skips the ORA into Bruce's hit byte.
    // The swing is already chosen, so the Ninja and Yamo still attack.
    if (victim.kind == 0 && gConfig.enemiesCantHitYou && attacker.kind != 0) {
        return false;
    }
    // $2A91 is the strike. $2E29 is a punch, a kick, or a landing.
    PlayEffect(blow == 2 ? 0x87 : 0x86);
    victim.facing = attacker.facing > 0 ? -1 : 1;
    int state = kFlinch;
    int timer = 3;
    if (blow == 16 && gConfig.fixBugsInOriginal) {
        // One pass of $2BF9. Fifty points, then subtract 5. Below zero pays
        // the knockout once and stays on the knockdown.
        if (attacker.kind == 0) {
            AddScoreByte(2, 0x32);
        }
        const int before = victim.health;
        victim.health -= 5;
        if (victim.health < 0) {
            state = kKnock;
            timer = 0x0C;
            if (attacker.kind == 0 && before >= 0 && victim.kind != 0) {
                AddKnockout(victim.kind);
            }
        } else {
            state = kStomp;
            timer = 8;
        }
    } else if (blow == 16) {
        // $2BF9. A landing blow keeps subtracting 5 from the health byte until
        // that byte is positive again, and each negative result pays again.
        victim.health = ApplyStomp(victim.health, victim.kind, attacker.kind == 0);
        state = kStomp;
        timer = 8;
    } else {
        int damage = 2;
        int scoreByte = 0;
        if (blow == 4) {
            damage = 3;
            timer = 4;
            scoreByte = 0x64;
        } else if (blow == 8) {
            damage = 7;
            state = kKnock;
            timer = 0x0C;
            scoreByte = 0x4B;
        }
        const int before = victim.health;
        // Bit 7 of $BC. The blow is paid before the health test, and the
        // knockout is a separate add once that test goes negative.
        if (attacker.kind == 0 && scoreByte != 0) {
            AddScoreByte(2, scoreByte);
        }
        victim.health -= damage;
        // $2C24. Health below 0 is a knockdown, whatever the blow was. The life
        // or the removal waits until that timer reaches 0. The landing blow
        // does not stay here: its byte wraps back into 0..127.
        if (victim.health < 0) {
            state = kKnock;
            timer = 0x0C;
        }
        if (attacker.kind == 0 && before >= 0 && victim.health < 0 && victim.kind != 0) {
            AddKnockout(victim.kind);
        }
    }
    victim.action = state;
    victim.timer = timer;
    if (victim.kind == 0) {
        g.health = victim.health;
        g.facing = victim.facing;
        g.action = state;
        g.actionTimer = timer;
        g.hold = 0;
        return false;
    }
    victim.fighter->health = victim.health;
    victim.fighter->facing = victim.facing;
    victim.fighter->action = state;
    victim.fighter->actionTimer = timer;
    return false;
}

int BlowOf(const Who& attacker, const Who& victim) {
    if (attacker.action == 0x16 && attacker.timer < 8 && InFront(attacker, victim, 4)) {
        return 2;
    }
    if (attacker.action == kPunch && InFront(attacker, victim, 6)) {
        const int dy = YposOf(attacker.y) - YposOf(victim.y);
        if (dy < 0x16 && dy > -0x16) {
            return 4;
        }
    }
    if ((attacker.action == kFlyLeft || attacker.action == kFlyRight) && attacker.timer < 0x0A &&
        InFront(attacker, victim, 6)) {
        return 8;
    }
    if (attacker.action == kFall && victim.action != kFlyLeft && victim.action != kFlyRight &&
        victim.action != kPunch) {
        const int drop = YposOf(victim.y) - YposOf(attacker.y);
        if (drop >= 0x0A) {
            return 16;
        }
    }
    return 0;
}

bool Resolve() {
    Who body[3];
    int n = 0;
    body[n++] = ReadBruce();
    if (ninja.on) {
        body[n++] = ReadFighter(ninja);
    }
    if (yamo.on) {
        body[n++] = ReadFighter(yamo);
    }
    for (int v = 0; v < n; ++v) {
        if (!Open(body[v].action)) {
            continue;
        }
        for (int a = 0; a < n; ++a) {
            if (a == v || !Open(body[a].action)) {
                continue;
            }
            const int blow = BlowOf(body[a], body[v]);
            if (blow == 0 || !Touches(body[a], body[v])) {
                continue;
            }
            if (Hurt(body[v], body[a], blow)) {
                return true;
            }
            // The victim is no longer open, so a second blow waits.
            break;
        }
    }
    return false;
}

// $1C9D. State 0, or a state that just changed, reloads the burst count.
void NoteState(Fighter& f) {
    if (f.action == 0 || f.action != f.prevAction) {
        f.repeat = 6;
    } else if (f.repeat > 0) {
        f.repeat -= 1;
    }
    f.prevAction = f.action;
}

// True when he is wedged in a wall with no steps left to undo. $24DB then
// re-enters the room on Bruce's spawn.
bool StepFighter(Fighter& f) {
    if (!f.on) {
        Spawn(f);
        return false;
    }
    if (f.health < 0 && !(f.action == kKnock && f.actionTimer > 0)) {
        Bury(f);
        return false;
    }
    // enemy_turn runs before release_hold. State 0 with a finished stand
    // becomes the chase, and NoteState has to see that new state.
    if (f.action == 0 && f.actionTimer == 0) {
        const Contact ground = At(f);
        if (ground.ground || ground.ladder) {
            f.action = 0x14;
            f.actionTimer = 0x14;
        }
    }
    RememberSteps(f);
    NoteState(f);
    // The overlap from the picture already showing. push_out reads that,
    // and it runs instead of the chase, the strike, or the ladder ride.
    const Contact shown = At(f);
    if (f.action == kStuck && f.actionTimer > 0) {
        if (!shown.wall) {
            f.actionTimer = 0;
        } else if (!OlderSteps(f)) {
            return true;
        } else {
            PushOut(f, shown);
        }
        Choose(f);
        return false;
    }
    if (shown.wall) {
        PushOut(f, shown);
        Choose(f);
        return false;
    }
    if (f.action == kStuck) {
        f.action = 0;
        f.actionTimer = 0;
        Choose(f);
        return false;
    }
    if (f.action == kCarry) {
        CarryFighter(f);
        Choose(f);
        return false;
    }
    Act(f);
    Choose(f);
    return false;
}

bool DrawnPixel(const uint8_t* frames, int pose, int plane, int x, int y) {
    const uint8_t* frame = frames + static_cast<size_t>(pose * 2 + plane) * kFrameBytes;
    return (frame[y * kSpriteRowBytes + x / 8] & static_cast<uint8_t>(0x80 >> (x & 7))) != 0;
}

void DrawOne(const Fighter& f) {
    if (!f.on || f.frames == nullptr) {
        return;
    }
    for (int plane = 0; plane < 2; ++plane) {
        const int ink = f.kind == 1 ? NinjaInk(f.pose, plane) : 0;
        for (int y = 0; y < kSpriteRows; ++y) {
            for (int x = 0; x < kSpriteCols; ++x) {
                if (!DrawnPixel(f.frames, f.pose, plane, x, y)) {
                    continue;
                }
                const int px = f.x + PictureShift(f.kind, f.facing, f.pose, plane) + x;
                const int py = f.y + y;
                if (px >= 0 && py >= 0 && px < kWidth && py < kHeight) {
                    g.pixels[static_cast<size_t>(py * kWidth + px)] = kPalette[ink];
                }
            }
        }
    }
}

}  // namespace

bool LoadFighters() {
    const size_t bytes = static_cast<size_t>(kFrames) * 2 * kFrameBytes;
    ninjaBytes = LoadFile(L"ninja.bin", bytes);
    yamoBytes = LoadFile(L"yamo.bin", bytes);
    if (ninjaBytes.empty() || yamoBytes.empty()) {
        return false;
    }
    ninja.kind = 1;
    ninja.frames = ninjaBytes.data();
    yamo.kind = 2;
    yamo.frames = yamoBytes.data();
    return true;
}

Fighter gSavedNinja;
Fighter gSavedYamo;
bool gFightersHeld = false;

void StandFighters(int ninjaX, int ninjaY, int ninjaFacing, int yamoX, int yamoY, int yamoFacing) {
    if (!gFightersHeld) {
        gSavedNinja = ninja;
        gSavedYamo = yamo;
        gFightersHeld = true;
    }
    const auto stand = [](Fighter& fighter, int x, int y, int facing) {
        fighter.on = true;
        fighter.x = x;
        fighter.y = y;
        fighter.facing = facing < 0 ? -1 : 1;
        fighter.pose = facing < 0 ? 1 : 0;
        fighter.action = 0;
        fighter.actionTimer = 0;
    };
    stand(ninja, ninjaX, ninjaY, ninjaFacing);
    stand(yamo, yamoX, yamoY, yamoFacing);
}

void RestoreFighters() {
    if (!gFightersHeld) {
        return;
    }
    ninja = gSavedNinja;
    yamo = gSavedYamo;
    gFightersHeld = false;
}

void ResetFighters() {
    // $2FC6 reloads both waits when the room opens. 0 spawns on the first tick.
    ninja.on = false;
    ninja.action = 0;
    ninja.actionTimer = 0;
    ninja.delay = RoomDelay(1);
    ninja.carryDelay = 0;
    ninja.carryMark = 0;
    ninja.repeat = 6;
    ninja.prevAction = 0;
    yamo.on = false;
    yamo.action = 0;
    yamo.actionTimer = 0;
    yamo.delay = RoomDelay(2);
    yamo.carryDelay = 0;
    yamo.carryMark = 0;
    yamo.repeat = 6;
    yamo.prevAction = 0;
}

bool TickFighters() {
    if (ninja.frames == nullptr) {
        return false;
    }
    // $2E39 before $1C23. The catch is state $17, and a blow after that can
    // replace it before anyone moves.
    if (yamo.on) {
        CatchSurface(yamo);
    }
    if (ninja.on) {
        CatchSurface(ninja);
    }
    // A life lost while resolving the previous pictures must not also step them.
    if (Resolve()) {
        return true;
    }
    // $0DA4 counts both waits down every tick, including while they are on screen.
    if (ninja.delay > 0) {
        ninja.delay -= 1;
    }
    if (yamo.delay > 0) {
        yamo.delay -= 1;
    }
    // $24DB. A fighter still inside a wall, with no steps left to walk back,
    // re-enters this room on Bruce's spawn.
    if (StepFighter(yamo) || StepFighter(ninja)) {
        if (EnterRoom(g.roomId)) {
            PlaceBruce();
        }
        return true;
    }
    return false;
}

void DrawFighters() {
    DrawOne(yamo);
    DrawOne(ninja);
}
