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

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "bruce.h"

#include "collision.h"
#include "config.h"
#include "draw.h"
#include "fighter.h"
#include "game.h"
#include "pad.h"
#include "room.h"
#include "sound.h"
#include "ui.h"

int StandPose() {
    return g.facing < 0 ? 1 : 0;
}

int WalkPose() {
    const int step = g.tick & 1;
    return g.facing < 0 ? (2 + step) : (4 + step);
}

int PlaneShift(int pose, int plane) {
    // Mode 1 pictures are 32 screen pixels wide. $25FD slides a left-facing
    // mode-1 body left by 8 VIC units, and $264B does the same for detail
    // libraries $05 (punch left), $19 (flying kick left, leg extended), and
    // $43 (duck right). 8 VIC units are 16 screen pixels. The torso stays
    // over the stand picture. The fist, the kicking leg, or the duck's
    // black plane hangs off that side.
    if (pose == kPosePunchLeft) {
        return -16;
    }
    if (pose == kPoseFlyLeftExtended && plane == 1) {
        return -16;
    }
    if (pose == kPoseDuckRight && plane == 1) {
        return -16;
    }
    // $3F is the late left knockdown's detail. $264B slides it with the
    // punch, the extended kick, and the duck.
    if (pose == kPoseKnockLeft1 && plane == 1) {
        return -16;
    }
    return 0;
}

namespace {

// Screen limits from the xpos/ypos checks. g.x = xpos * 2 - 24 and
// g.y = ypos - 21, with the visible screen starting at sprite coordinate 24, 50.
constexpr int kMinX = 2;     // xpos $0D. A step from here reaches x = 0.
constexpr int kMaxX = 302;   // xpos $A3. A step from 300 reaches here.
constexpr int kMinY = 25;    // ypos $2E
constexpr int kMaxY = 178;   // ypos $C7

int HoldFor(int state) {
    // $4A8C. Climb pauses three logic frames. Duck pauses one. The rest run
    // every logic frame.
    if (state == kClimbUp || state == kClimbDown) {
        return 3;
    }
    if (state == kDuck) {
        return 1;
    }
    return 0;
}

void SetState(int state, int timer) {
    g.action = state;
    g.actionTimer = timer;
    g.hold = HoldFor(state);
}

// Steps since the last check at $2E39. move_left increments $4F5D,
// move_right increments $4F60, and move_up increments $4F57. $30D4
// reads those when it decides an exit is open.
bool wentLeft = false;
bool wentRight = false;
bool wentUp = false;

// $4F57 up, $4F5A down, $4F5D left, $4F60 right, then the older copies.
// release_hold shifts a nonzero current set one generation toward $4FD8.
// $1CC7 reads the generation at $4F63. Ten generations fit before the spill
// that the shift writes and the reversal clears. One sideways count is one
// xpos, two screen pixels.
constexpr int kStepUp = 0;
constexpr int kStepDown = 1;
constexpr int kStepLeft = 2;
constexpr int kStepRight = 3;
constexpr int kStepGens = 11;
int hist[kStepGens][4] = {};

void ClearSteps() {
    for (int gen = 0; gen < kStepGens; ++gen) {
        for (int d = 0; d < 4; ++d) {
            hist[gen][d] = 0;
        }
    }
}

void RememberSteps() {
    bool any = false;
    for (int d = 0; d < 4; ++d) {
        if (hist[0][d] != 0) {
            any = true;
        }
    }
    if (any) {
        for (int gen = kStepGens - 1; gen > 0; --gen) {
            for (int d = 0; d < 4; ++d) {
                hist[gen][d] = hist[gen - 1][d];
            }
        }
    }
    for (int d = 0; d < 4; ++d) {
        hist[0][d] = 0;
    }
}

// $24C6. The bytes from $4F63 through $4FD8. The current generation is not
// part of the test.
bool OlderSteps() {
    for (int gen = 1; gen < kStepGens; ++gen) {
        for (int d = 0; d < 4; ++d) {
            if (hist[gen][d] != 0) {
                return true;
            }
        }
    }
    return false;
}

void StandStill() {
    g.action = 0;
    g.actionTimer = 0;
    g.hold = 0;
    g.repeat = 6;
    g.prevState = 0;
    g.fallSlide = 0;
    g.tick = 0;
    g.pose = StandPose();
}

void Land() {
    SetState(0, 0);
}

// One xpos step, two screen pixels. The screen edge refuses it. A wall does
// not: $1CC7 walks the step back on the next frame. The kick and the fall's
// nudge still stop on a wall here. State $17 uses MoveSideRaw instead.
bool TryStep(int dir) {
    if (dir < 0) {
        if (g.x < kMinX || Probe(g.x - 2, g.y).wall) {
            return false;
        }
        g.x -= 2;
        hist[0][kStepLeft] += 1;
        wentLeft = true;
    } else {
        if (g.x >= kMaxX || Probe(g.x + 2, g.y).wall) {
            return false;
        }
        g.x += 2;
        hist[0][kStepRight] += 1;
        wentRight = true;
    }
    return true;
}

void Rise(int pixels) {
    for (int i = 0; i < pixels; ++i) {
        if (g.y < kMinY || Probe(g.x, g.y - 1).wall) {
            return;
        }
        g.y -= 1;
        hist[0][kStepUp] += 1;
        wentUp = true;
    }
}

void Drop(int pixels) {
    for (int i = 0; i < pixels && g.y < kMaxY; ++i) {
        g.y += 1;
        hist[0][kStepDown] += 1;
    }
}

// move_up at $1DF2. The screen edge refuses it. A wall does not.
void MoveUpRaw(int pixels) {
    for (int i = 0; i < pixels; ++i) {
        if (g.y < kMinY) {
            return;
        }
        g.y -= 1;
        hist[0][kStepUp] += 1;
        wentUp = true;
    }
}

// move_down at $1E39. Same edge rule, and no wall test.
void MoveDownRaw(int pixels) {
    for (int i = 0; i < pixels && g.y < kMaxY; ++i) {
        g.y += 1;
        hist[0][kStepDown] += 1;
    }
}

// move_left at $1E86 and move_right at $1EC4. One xpos, and facing follows.
void MoveSideRaw(int dir) {
    if (dir < 0) {
        if (g.x < kMinX) {
            return;
        }
        g.x -= 2;
        hist[0][kStepLeft] += 1;
        wentLeft = true;
    } else {
        if (g.x >= kMaxX) {
            return;
        }
        g.x += 2;
        hist[0][kStepRight] += 1;
        wentRight = true;
    }
    g.facing = dir;
}

// After $1CC7 has consumed $4F63, copy $4F6F..$4FD8 down onto $4F57 and
// clear the tail from $4FC3. That drops the generation just undone and the
// empty current one, and keeps the older copies.
void SlideAfterPush() {
    int next[kStepGens][4] = {};
    for (int gen = 0; gen <= 8; ++gen) {
        for (int d = 0; d < 4; ++d) {
            next[gen][d] = hist[gen + 2][d];
        }
    }
    for (int gen = 0; gen < kStepGens; ++gen) {
        for (int d = 0; d < 4; ++d) {
            hist[gen][d] = next[gen][d];
        }
    }
}

// $1CC7. Walk $4F63 back: up by moving down, down by moving up, left by
// moving right, right by moving left. $85 stays set when that used no down
// or sideways step, and the loop then tries the next generation. $86 skips
// the facing nudge on the first pass. State $18, timer $40.
void PushOut(const Contact& here) {
    int tries = 8;
    for (;;) {
        const int up = hist[1][kStepUp];
        const int down = hist[1][kStepDown];
        const int left = hist[1][kStepLeft];
        const int right = hist[1][kStepRight];
        const bool sideways = down || left || right;
        MoveDownRaw(up);
        MoveUpRaw(down);
        for (int i = 0; i < left; ++i) {
            MoveSideRaw(1);
        }
        for (int i = 0; i < right; ++i) {
            MoveSideRaw(-1);
        }
        SlideAfterPush();
        g.hold = 0;
        tries -= 1;
        if (tries < 0 || here.ladder) {
            break;
        }
        if (!sideways && g.action != kDuck) {
            RememberSteps();
            continue;
        }
        if (tries != 7) {
            MoveSideRaw(g.facing < 0 ? 1 : -1);
        }
        break;
    }
    SetState(kStuck, 0x40);
}

// State $05 at $2903, read after keep_state has decremented the timer.
// $0F is library 8. $0C-$0E are $1E. $02-$0B are $20. 1 is $1E. 0 is 8.
int JumpPose() {
    const int t = g.actionTimer;
    if (t >= 0x0F || t <= 0) {
        return kPoseLegs;
    }
    if (t >= 0x0C || t == 1) {
        return 7;
    }
    return kPoseArms;
}

void ChoosePose(const Contact& here) {
    // `here` is the overlap from before this frame's step, which is the hit
    // choose_pose reads. A walk that leaves a ladder still shows the climb
    // picture for that frame.
    const bool climbNow = g.action == kClimbUp || g.action == kClimbDown
        || ((g.action == kWalkLeft || g.action == kWalkRight) && here.ladder)
        || (g.action == 0 && here.ladder);
    if (g.action != kWalkLeft && g.action != kWalkRight) {
        g.tick = 0;
    }

    if (g.action == kPunch) {
        g.pose = g.facing < 0 ? kPosePunchLeft : kPosePunchRight;
    } else if (g.action == kFlyLeft || g.action == kFlyRight) {
        const bool tucked = g.actionTimer >= 0x0A;
        if (g.action == kFlyLeft) {
            g.pose = tucked ? 15 : 16;
        } else {
            g.pose = tucked ? 17 : 18;
        }
    } else if (g.action == kSideLeft || g.action == kSideRight) {
        const bool tucked = g.actionTimer >= 0x0C || g.actionTimer < 3;
        if (g.action == kSideLeft) {
            g.pose = tucked ? 9 : 10;
        } else {
            g.pose = tucked ? 11 : 12;
        }
    } else if (g.action == kJump) {
        g.pose = JumpPose();
    } else if (g.action == kDuck) {
        g.pose = g.facing < 0 ? kPoseDuckLeft : kPoseDuckRight;
    } else if (g.action == kFall) {
        // Open air is library $44, the arms-out jump. Overlapping a wall
        // (hit bit 3) is library 8, the legs together.
        g.pose = here.wall ? kPoseLegs : kPoseArms;
    } else if (climbNow) {
        // States 1 and 2, and a walk that overlaps a ladder, flip $0A / $0C.
        // Standing on a ladder keeps the picture already showing.
        const bool stepping = g.action == kClimbUp || g.action == kClimbDown
            || g.action == kWalkLeft || g.action == kWalkRight;
        if (stepping) {
            g.pose = g.pose == kPoseClimb0 ? kPoseClimb1 : kPoseClimb0;
        } else if (g.pose != kPoseClimb0 && g.pose != kPoseClimb1) {
            g.pose = kPoseClimb0;
        }
        // The walk libraries are not showing, so the next stride on the
        // floor starts at the first picture.
        g.tick = 0;
    } else if (g.action == kWalkLeft || g.action == kWalkRight) {
        // $28D4 shows library $0E first, then flips every logic frame.
        g.pose = WalkPose();
        g.tick += 1;
    } else if (g.action == kStuck) {
        // $27C1 keeps the picture already showing. Library $44, the arms-out
        // jump, is the one it replaces with library 8.
        if (g.pose == kPoseArms) {
            g.pose = kPoseLegs;
        }
    } else {
        g.pose = StandPose();
    }
}

// The timer in g.actionTimer is the value before this frame's decrement,
// which is Y in move_for_state.
void Script(const Contact& here) {
    const int t = g.actionTimer;

    if (g.action == kJump) {
        if (here.ladder) {
            Land();
            return;
        }
        if (t < 0x0D && t >= 2) {
            if (t >= 8) {
                Rise(t == 0x0C ? 4 : 2);
            } else if (here.ground) {
                Land();
                return;
            } else {
                Drop(2);
            }
        }
        g.actionTimer -= 1;
        return;
    }

    if (g.action == kSideLeft || g.action == kSideRight) {
        // States 6 and 7 at $20D2 and $2120. A ladder lands immediately.
        // Ground lands only while the timer is below 7. The steps themselves
        // never read the wall bit. The two sideways steps come before the
        // drop, so a lamp that the drop clears is not a wall on the next test.
        if (here.ladder) {
            Land();
            return;
        }
        const int dir = g.action == kSideLeft ? -1 : 1;
        if (t > 0x0A) {
            MoveUpRaw(2);
        } else if (t == 0x0A) {
            MoveUpRaw(1);
        } else if (t < 7) {
            if (here.ground) {
                Land();
                return;
            }
            MoveSideRaw(dir);
            MoveSideRaw(dir);
            MoveDownRaw(t == 6 ? 1 : 2);
            g.actionTimer -= 1;
            return;
        }
        MoveSideRaw(dir);
        MoveSideRaw(dir);
        g.actionTimer -= 1;
        return;
    }

    if (g.action == kFlyLeft || g.action == kFlyRight) {
        if (here.ladder || here.wall) {
            Land();
            return;
        }
        // $2058 and $209D. The frame whose timer is still $0C, after a ladder
        // or a wall has cancelled the kick.
        if (t == 0x0C) {
            PlayEffect(0x84);
        }
        const int dir = g.action == kFlyLeft ? -1 : 1;
        if (t >= 0x0A) {
            Rise(1);
            TryStep(dir);
        } else if (t >= 3) {
            TryStep(dir);
        } else if (here.ground) {
            Land();
            return;
        } else {
            Drop(1);
            TryStep(dir);
        }
        g.actionTimer -= 1;
        return;
    }

    if (g.action == kPunch) {
        if (here.ladder || here.wall) {
            Land();
            return;
        }
        // The frame whose timer is 1 ends the picture and leaves a recovery
        // of 3 ($06 shifted down once by $10D0 while the long timer is clear).
        if (t == 1) {
            g.action = 0;
            g.actionTimer = 3;
            g.hold = 0;
            return;
        }
        g.actionTimer -= 1;
        return;
    }

    // State 0 with a timer still running is the punch recovery. Nothing moves.
    if (g.actionTimer > 0) {
        g.actionTimer -= 1;
    }
}

void StartFly(int dir) {
    g.facing = dir;
    SetState(dir < 0 ? kFlyLeft : kFlyRight, 0x0C);
}

void Walk(int dir, bool fire) {
    const int walk = dir < 0 ? kWalkLeft : kWalkRight;
    // The edge is checked before any step, so a kick cannot start there either.
    if ((dir < 0 && g.x < kMinX) || (dir > 0 && g.x >= kMaxX)) {
        Land();
        return;
    }
    // $1E63 and $1EA4. One blip once the edge test has passed, before the
    // steps. A kick that replaces a step has already sounded.
    PlayEffect(0x80);
    if (g.prevState != walk) {
        g.tick = 0;
    }
    // $1E70. Two steps, or three once $CB has reached 0. The routine falls
    // through into the last step. A kick replaces whichever step sees a walk
    // already in $C2 and $CB below 4. Fire is $9B == 0. The kick does not
    // move until the next logic frame. A step into the screen edge clears
    // the state. That step returns, so a later one in the burst can still
    // become the kick. A wall does not refuse the step. The next frame's
    // $1CC7 walks it back if he is still inside.
    auto one = [&]() -> bool {
        if (g.prevState == walk && g.repeat < 4 && fire) {
            StartFly(dir);
            return true;
        }
        if ((dir < 0 && g.x < kMinX) || (dir > 0 && g.x >= kMaxX)) {
            Land();
            return false;
        }
        MoveSideRaw(dir);
        SetState(walk, 0);
        return false;
    };
    if (g.repeat == 0 && one()) {
        return;
    }
    if (one()) {
        return;
    }
    one();
}

void Climb(int dir) {
    // Two explicit steps plus the fall-through, so three pixels. The extra
    // step when $CB is 0 makes four. Hold is 3. Changing into the climb
    // reloads $CB, so that first burst is three; later bursts count down
    // and the one that reaches 0 is four. The ladder bit is the overlap from
    // the start of the frame, so every pixel of this burst is taken. Only
    // the screen edge refuses one, and a refused pixel clears the state the
    // way stop_move does.
    const int n = g.repeat == 0 ? 4 : 3;
    for (int i = 0; i < n; ++i) {
        if (dir < 0) {
            if (g.y < kMinY) {
                Land();
                continue;
            }
            g.y -= 1;
            hist[0][kStepUp] += 1;
            wentUp = true;
            SetState(kClimbUp, 0);
        } else {
            if (g.y >= kMaxY) {
                Land();
                continue;
            }
            g.y += 1;
            hist[0][kStepDown] += 1;
            SetState(kClimbDown, 0);
        }
    }
}

void Duck(int facing) {
    // The diagonal paths refuse the turn at the far edge: facing left at
    // xpos >= $A2, facing right at xpos < $10.
    if (facing < 0 && g.x >= 300) {
        Land();
        return;
    }
    if (facing > 0 && g.x < 8) {
        Land();
        return;
    }
    g.facing = facing;
    SetState(kDuck, 0);
}

void StartFall() {
    // The first airborne frame drops one pixel and arms $B0 with 2.
    g.fallSlide = 2;
    Drop(1);
    // State $18 leaves the legs picture up. The fall switches to the arms
    // picture, whose hands are the only pixels in the room 4 lip. $1CC7
    // then puts that one pixel back and the two pictures trade forever.
    // Drop through a clip that only the arms picture touches. A wall or a
    // floor the legs picture meets still stops the extra pixels.
    if (g.prevState == kStuck) {
        for (int i = 0; i < kSpriteRows; ++i) {
            const Contact arms = ProbePose(kPoseArms, g.x, g.y);
            const Contact legs = ProbePose(kPoseLegs, g.x, g.y);
            if (!arms.wall || legs.wall || legs.ground) {
                break;
            }
            const int y = g.y;
            Drop(1);
            if (g.y == y) {
                break;
            }
        }
    }
    SetState(kFall, 0);
}

void ContinueFall(const Contact& here) {
    const Contact upper = ProbeUpper(g.x, g.y);
    if (upper.ground) {
        // Torso still on a ledge: one nudge in the facing direction, then
        // the same two-pixel drop as every later falling frame.
        g.fallSlide >>= 1;
        if (g.fallSlide != 0) {
            TryStep(g.facing);
        }
        Drop(2);
        SetState(kFall, 0);
    } else if (here.ground || here.ladder) {
        Land();
    } else {
        Drop(2);
        SetState(kFall, 0);
    }
}

void OnGround(int stick, bool fire, const Contact& here) {
    if (stick == 0x0E) {
        // Up. Too close to ypos $2E clears the state. Bit 2 climbs. Bit 3
        // blocks the standing jump. The climb is three pixels, or four once
        // the repeat count has run out. $1DDC plays before that ladder test,
        // so the standing jump sounds as well. Below ypos $2E it does not.
        if (g.y < kMinY) {
            Land();
        } else {
            PlayEffect(0x80);
            if (here.ladder) {
                Climb(-1);
            } else if (here.wall) {
                Land();
            } else {
                SetState(kJump, 0x0F);
            }
        }
    } else if (stick == 0x0D) {
        if (here.ladder) {
            // $1E29. ypos $C7 refuses the step and does not blip.
            if (g.y < kMaxY) {
                PlayEffect(0x80);
            }
            Climb(1);
        } else if (here.ground) {
            Duck(g.facing);
        }
    } else if (stick == 0x0B) {
        Walk(-1, fire);
    } else if (stick == 0x07) {
        Walk(1, fire);
    } else if (stick == 0x0A || stick == 0x06) {
        // Up with left ($0A) or right ($06). A ladder cancels it.
        // $1F46 and $1F5C blip when the jump is taken.
        if (here.ladder) {
            Land();
        } else {
            PlayEffect(0x80);
            const int dir = stick == 0x0A ? -1 : 1;
            g.facing = dir;
            SetState(dir < 0 ? kSideLeft : kSideRight, 0x0F);
        }
    } else if (stick == 0x09 || stick == 0x05) {
        if (here.ladder) {
            Land();
        } else if (here.ground) {
            Duck(stick == 0x09 ? -1 : 1);
        }
    } else if (stick == 0x0F && fire && !here.ladder) {
        SetState(kPunch, 4);
    } else {
        Land();
    }
}

}  // namespace

namespace {

const uint8_t* viewFrames = nullptr;
int viewAction = -1;
int viewFacing = 1;
int viewPose = 0;

}  // namespace

void SetActorView(const uint8_t* frames, int action, int facing, int pose) {
    viewFrames = frames;
    viewAction = action;
    viewFacing = facing;
    viewPose = pose;
}

void ClearActorView() {
    viewFrames = nullptr;
    viewAction = -1;
}

int ViewAction() {
    return viewFrames ? viewAction : g.action;
}

int ViewFacing() {
    return viewFrames ? viewFacing : g.facing;
}

int ViewPose() {
    return viewFrames ? viewPose : g.pose;
}

const uint8_t* PoseBytes(int pose, int plane) {
    const uint8_t* base = viewFrames ? viewFrames : g.frames.data();
    return base + static_cast<size_t>(pose * 2 + plane) * kFrameBytes;
}

namespace {

// $AD. State $17 waits this many frames between stick steps.
int carryDelay = 0;

void RespawnHere() {
    BruceFell();
}

int KnockPicture(bool early) {
    if (g.facing > 0) {
        return early ? kPoseKnockRight0 : kPoseKnockRight1;
    }
    return early ? kPoseKnockLeft0 : kPoseKnockLeft1;
}

// $242E and $2458. Two steps away from the way he faces, then the facing is
// put back, so the picture still looks at the fighter who hit him.
void SlideBack() {
    const int face = g.facing;
    const int back = face < 0 ? 1 : -1;
    MoveSideRaw(back);
    MoveSideRaw(back);
    g.facing = face;
    if (g.actionTimer > 0) {
        g.actionTimer -= 1;
    }
}

// States $0F, $13, and $1B. The stick is not read. $13 with $D1 negative
// spends the life on the frame the timer reaches 0.
void HitReact() {
    if (g.action == kFlinch) {
        if (g.actionTimer <= 0) {
            Land();
            g.pose = StandPose();
            return;
        }
        SlideBack();
        g.action = kFlinch;
        g.pose = StandPose();
        return;
    }
    if (g.action == kKnock) {
        if (g.actionTimer <= 0) {
            if (g.health < 0) {
                BruceFell();
                return;
            }
            Land();
            g.pose = StandPose();
            return;
        }
        const Contact here = Probe(g.x, g.y);
        if (!here.ground && !here.ladder) {
            Drop(1);
        }
        const bool early = g.actionTimer >= 0x0A;
        SlideBack();
        g.action = kKnock;
        g.pose = KnockPicture(early);
        return;
    }
    if (g.action == kStomp) {
        if (g.actionTimer <= 0) {
            if (g.health < 0) {
                BruceFell();
                return;
            }
            Land();
            g.pose = StandPose();
            return;
        }
        const bool early = g.actionTimer >= 4;
        g.actionTimer -= 1;
        g.pose = KnockPicture(early);
    }
}

// $2187. A pure direction steps two pixels. Diagonals do not. move_up,
// move_down, move_left, and move_right ignore walls; $1CC7 undoes a step
// that is still inside one on the next frame. $60 == 1 then shoves two
// pixels with the scrolling floor. The climb picture alternates on his
// own step, which is $AA shifted to 0 before choose_pose.
void CarryMove(int stick) {
    bool stepped = false;
    if (carryDelay > 0) {
        carryDelay -= 1;
    } else if (stick == 0x0E) {
        MoveUpRaw(2);
        stepped = true;
    } else if (stick == 0x0D) {
        MoveDownRaw(2);
        stepped = true;
    } else if (stick == 0x0B) {
        MoveSideRaw(-1);
        MoveSideRaw(-1);
        stepped = true;
    } else if (stick == 0x07) {
        MoveSideRaw(1);
        MoveSideRaw(1);
        stepped = true;
    }
    if (stepped) {
        carryDelay = 4;
        g.pose = g.pose == kPoseClimb0 ? kPoseClimb1 : kPoseClimb0;
        // $21EB. His own step, not the shove from the moving floor.
        PlayEffect(0x80);
    }
    const int shove = SurfaceShove();
    if (shove < 0) {
        MoveUpRaw(2);
    } else if (shove > 0) {
        MoveDownRaw(2);
    }
}

void TakeLamp(int col, int tileRow) {
    const int tile = TileAt(col, tileRow);
    if (tile != 0x13 && tile != 0x14 && tile != 0x31) {
        return;
    }
    int mount = 0;
    if (!ClaimLamp(col, tileRow, mount)) {
        return;
    }
    WriteTile(col, tileRow, mount);
    // $2F2D. $7D added to the low score byte. The digits show 125.
    AddScoreByte(2, 0x7D);
    PlayEffect(0x8E);
}

// $40BC. Three counters, one per lamp shape. At zero the counter reloads
// to 1..15 logic frames and two rows of that shape's shadow glyph are
// replaced. The new byte keeps the mount and swaps one dot through the
// four multicolor pairs, so every lamp of that shape flickers together.
uint16_t lampNoise = 0xA5;
int lamp13 = 1;
int lamp31 = 1;
int lamp14 = 1;

uint8_t LampByte() {
    lampNoise = static_cast<uint16_t>(lampNoise * 251 + 17);
    return static_cast<uint8_t>(lampNoise ^ (lampNoise >> 8));
}

int LampDelay() {
    int n = 0;
    while (n == 0) {
        n = LampByte() & 0x0F;
    }
    return n;
}

void PokeGlyph(int glyph, uint8_t value) {
    const size_t i = static_cast<size_t>(glyph) * 8 + 2;
    g.charset[i] = value;
    g.charset[i + 1] = value;
}

void AnimateLamps() {
    // Charset $A000 keeps the mount and moves one dot. Charset $A800 stores
    // the mask on its own. Charset $B000, room 18, is not written here.
    if (--lamp13 == 0) {
        lamp13 = LampDelay();
        const uint8_t bits = static_cast<uint8_t>(LampByte() & 0x30);
        if (g.charsetId == 0) {
            const uint8_t row = static_cast<uint8_t>(bits | 0xCC);
            PokeGlyph(0x93, row);
            PokeGlyph(0x86, static_cast<uint8_t>(row & 0xB8));
        } else if (g.charsetId == 1) {
            PokeGlyph(0x93, bits);
        }
    }
    if (--lamp31 == 0) {
        lamp31 = LampDelay();
        const uint8_t bits = static_cast<uint8_t>(LampByte() & 0x30);
        if (g.charsetId == 0) {
            PokeGlyph(0xB1, static_cast<uint8_t>(bits | 0xCC));
        } else if (g.charsetId == 1) {
            PokeGlyph(0xB1, bits);
        }
    }
    if (--lamp14 == 0) {
        lamp14 = LampDelay();
        const uint8_t bits = static_cast<uint8_t>(LampByte() & 0x0C);
        if (g.charsetId == 0) {
            const uint8_t row = static_cast<uint8_t>(bits | 0x33);
            PokeGlyph(0x94, row);
            PokeGlyph(0xE7, static_cast<uint8_t>(row & 0x2E));
        } else if (g.charsetId == 1) {
            PokeGlyph(0x94, bits);
        }
    }
}

// $2E39. The cells from his column across two more, on his tile row and the
// row below, are checked. Tiles 2-5 are the exits and are tested either way.
// A lamp needs bit 1. Room 13 adds 4 to ypos before the row is chosen.
// True means an exit fired and the caller should not move him this frame.
bool ScanSpecials() {
    const int xpos = (g.x + 24) / 2;
    const int ypos = g.y + 21;
    if (xpos < 0x0C) {
        return false;
    }
    int rowY = ypos;
    if (g.roomId == 0x0D) {
        rowY += 4;
    }
    if (rowY < 0x0C) {
        return false;
    }
    const int col0 = (xpos - 0x0C) >> 2;
    const int row0 = (rowY - 0x0C) >> 4;
    const Contact pose = ProbePose(g.pose, g.x, g.y);
    const bool prize = pose.bit1;
    const bool falling = g.action == kFall;
    for (int dr = 0; dr < 2; ++dr) {
        for (int dc = 0; dc < 3; ++dc) {
            const int col = col0 + dc;
            const int row = row0 + dr;
            const int tile = TileAt(col, row);
            if (tile < 0) {
                continue;
            }
            if (tile >= 2 && tile <= 5) {
                if (TakeExit(tile, xpos, ypos, wentLeft, wentRight, wentUp, falling)) {
                    return true;
                }
                continue;
            }
            TouchRoomTile(tile, col, row, prize, pose.ladder);
            if (prize) {
                TakeLamp(col, row);
            }
        }
    }
    return false;
}

}  // namespace

void PlaceBruce() {
    // wake_actor, with $0104 clear, copies $4BAE and $4BC2. Facing starts
    // at 1. The lift is only there for a stand picture that is already
    // inside a wall.
    RoomSpawn(g.roomId, g.x, g.y);
    g.facing = 1;
    g.health = 0x24;
    StandStill();
    ResetFighters();
    ClearSteps();
    wentLeft = false;
    wentRight = false;
    wentUp = false;
    for (int n = 0; n < 48 && PoseWall(g.pose, g.x, g.y); ++n) {
        g.y -= 1;
    }
}

// between_lives shows the PLAYER page before enter_room_at. The room stays
// as it was until that page's countdown reaches 0.
bool awaitingLife = false;

bool outOfLives = false;

void BruceFell() {
    // $1818. silence_sid, before the life is spent.
    SilenceSound();
    // Unlimited lives is LDA $28 in place of this DEC, and the same byte
    // in place of both INCs. The count stays on the stock the game started with.
    if (gConfig.infiniteLives) {
        g.lives = g.lifeStock;
        awaitingLife = true;
        return;
    }
    g.lives -= 1;
    // BMI game_over. Zero is still a life, so FALLS 00 plays the room again.
    // The next fall wraps the byte negative and leaves the game.
    if (g.lives < 0) {
        outOfLives = true;
        return;
    }
    awaitingLife = true;
}

bool BetweenLives() {
    return awaitingLife;
}

bool OutOfLives() {
    return outOfLives;
}

void ClearOutOfLives() {
    outOfLives = false;
    awaitingLife = false;
}

void ResumeAfterFall() {
    awaitingLife = false;
    if (EnterRoom(g.roomId)) {
        PlaceBruce();
    }
    Compose();
}

void Tick() {
    // $17C4 runs on every logic frame, including a hold, and the raster
    // count in $87 does too. The value is applied where the stick is built.
    const bool treasure = g.roomId == 19;
    const int treasureStick = treasure ? Room19Stick() : 0;

    const PadInput pad = ReadPad();
    const bool keys = !UiOwnsKeyboard();
    const bool left = (keys && (GetAsyncKeyState(VK_LEFT) & 0x8000) != 0) || pad.left;
    const bool right = (keys && (GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0) || pad.right;
    const bool up = (keys && (GetAsyncKeyState(VK_UP) & 0x8000) != 0) || pad.up;
    const bool down = (keys && (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0) || pad.down;
    const bool fire = (keys && (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0) || pad.fire;

    // $40BC runs before the overlap, and a held pose still reaches it.
    // The lamp glyphs flicker in every room. The charset selects the bytes.
    AnimateLamps();

    // $1BFE flips $7B and $3A6F runs the current room's hook before the
    // special scan, so a door can be open on the same frame it is tested.
    if (AdvanceRoom()) {
        ClearSteps();
        wentLeft = false;
        wentRight = false;
        wentUp = false;
        StandStill();
        ResetFighters();
        Compose();
        return;
    }

    // $2E10 clears state $17, the timer, and the pose hold before the cells
    // are read, so the tile has to catch him again. A shock skips the scan,
    // which is the $19 test. $AD is left as it was.
    if (g.action == kCarry) {
        g.action = 0;
        g.actionTimer = 0;
        g.hold = 0;
    }
    if (g.action != kZap && ScanSpecials()) {
        ClearSteps();
        wentLeft = false;
        wentRight = false;
        wentUp = false;
        StandStill();
        ResetFighters();
        Compose();
        return;
    }

    // Hits use the pictures already on screen. The fighters then step, and a
    // life lost here has already put Bruce back on the spawn.
    if (TickFighters()) {
        Compose();
        return;
    }
    if (g.action == kFlinch || g.action == kKnock || g.action == kStomp) {
        wentLeft = false;
        wentRight = false;
        wentUp = false;
        HitReact();
        Compose();
        return;
    }

    int stick = 0x0F;
    if (up && !down) {
        stick &= ~0x01;
    }
    if (down && !up) {
        stick &= ~0x02;
    }
    if (left && !right) {
        stick &= ~0x04;
    }
    if (right && !left) {
        stick &= ~0x08;
    }
    // Room 19. $98 becomes $0E while $88 is below 2, so he jumps or climbs
    // with no say in it, and $0F after that. $9B, fire, is left as pressed.
    if (treasure) {
        stick = treasureStick;
    }

    // $0106. The picture stays, and this frame does not read the stick.
    // A beam still shows the shock pose while the hold runs out.
    if (g.hold > 0) {
        g.hold -= 1;
        if (g.action == kZap) {
            g.pose = kPoseLegs;
        }
        Compose();
        return;
    }

    // State $19. Y is the timer before the decrement. 1 spends the life.
    if (g.action == kZap) {
        if (g.actionTimer <= 1) {
            RespawnHere();
        } else {
            g.actionTimer -= 1;
            g.pose = kPoseLegs;
        }
        Compose();
        return;
    }

    wentLeft = false;
    wentRight = false;
    wentUp = false;

    // $CB counts down only on a frame that actually runs, and only while the
    // state is the same one $C2 remembered. A fresh state reloads 6. Walking
    // and climbing take two pixels until that count reaches 0, then three.
    // State $17 passes through here too: release_hold updates $CB before the
    // wall test, and the wall test runs before the mover.
    const int state = g.action;
    if (state == 0 || state != g.prevState) {
        g.repeat = 6;
    } else if (g.repeat > 0) {
        g.repeat -= 1;
    }
    g.prevState = state;

    RememberSteps();
    // The picture chosen last frame. overlap_actors tests that one pose.
    // A walk used to test both strides so a step could be refused early.
    const Contact shown = ProbePose(g.pose, g.x, g.y);
    const Contact here = Probe(g.x, g.y);
    // $24B7. The frame after a reversal that is clear of the wall drops the
    // $40 timer and does not read the stick. While he is still inside and
    // $24C6 finds no older steps, $24DB respawns him.
    if (state == kStuck && g.actionTimer > 0 && !shown.wall) {
        g.actionTimer = 0;
        ChoosePose(here);
        Compose();
        return;
    }
    if (shown.wall) {
        if (state == kStuck && g.actionTimer > 0 && !OlderSteps()) {
            RespawnHere();
        } else {
            PushOut(shown);
        }
        ChoosePose(ProbePose(g.pose, g.x, g.y));
        Compose();
        return;
    }

    // State $17. The picture flips inside CarryMove, on his own step only.
    if (g.action == kCarry) {
        CarryMove(stick);
        Compose();
        return;
    }

    const bool supported = here.ground || here.ladder;
    if (g.actionTimer > 0) {
        Script(here);
    } else if (state == kFall) {
        ContinueFall(here);
    } else if (supported) {
        OnGround(stick, fire, here);
    } else {
        StartFall();
    }

    ChoosePose(here);
    // $1CC7 runs at the start of the next frame, from the picture chosen
    // here. Moving him back now would hide that overlap from the lamp test.
    Compose();
}
