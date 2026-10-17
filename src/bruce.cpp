#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "bruce.h"

#include "collision.h"
#include "draw.h"
#include "game.h"
#include "room.h"

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

// One xpos step, two screen pixels. A wall or the screen edge refuses it.
// The original moves anyway and $1CC7 pushes the next frame back out; refusing
// the step leaves him where that push would put him.
bool TryStep(int dir) {
    if (dir < 0) {
        if (g.x < kMinX || Probe(g.x - 2, g.y).wall) {
            return false;
        }
        g.x -= 2;
        wentLeft = true;
    } else {
        if (g.x >= kMaxX || Probe(g.x + 2, g.y).wall) {
            return false;
        }
        g.x += 2;
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
        wentUp = true;
    }
}

void Drop(int pixels) {
    for (int i = 0; i < pixels && g.y < kMaxY; ++i) {
        g.y += 1;
    }
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
        if (here.ladder) {
            Land();
            return;
        }
        const int dir = g.action == kSideLeft ? -1 : 1;
        if (t > 0x0A) {
            Rise(2);
        } else if (t == 0x0A) {
            Rise(1);
        } else if (t < 7) {
            if (here.ground) {
                Land();
                return;
            }
            TryStep(dir);
            TryStep(dir);
            Drop(t == 6 ? 1 : 2);
            g.actionTimer -= 1;
            return;
        }
        TryStep(dir);
        TryStep(dir);
        g.actionTimer -= 1;
        return;
    }

    if (g.action == kFlyLeft || g.action == kFlyRight) {
        if (here.ladder || here.wall) {
            Land();
            return;
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
    if (g.prevState != walk) {
        g.tick = 0;
    }
    // $1E70. Two steps, or three once $CB has reached 0. The routine falls
    // through into the last step. A kick replaces whichever step sees a walk
    // already in $C2 and $CB below 4. Fire is $9B == 0. The kick does not
    // move until the next logic frame. A step into the screen edge clears
    // the state. A wall does not: the step is refused and the walk stays,
    // which is where $1CC7 would have pushed him back to.
    auto one = [&]() -> bool {
        if (g.prevState == walk && g.repeat < 4 && fire) {
            StartFly(dir);
            return true;
        }
        if ((dir < 0 && g.x < kMinX) || (dir > 0 && g.x >= kMaxX)) {
            Land();
            return false;
        }
        g.action = walk;
        const int next = g.x + dir * 2;
        if (!Probe(next, g.y).wall) {
            g.x = next;
            if (dir < 0) {
                wentLeft = true;
            } else {
                wentRight = true;
            }
        }
        g.facing = dir;
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
            wentUp = true;
            SetState(kClimbUp, 0);
        } else {
            if (g.y >= kMaxY) {
                Land();
                continue;
            }
            g.y += 1;
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
        // the repeat count has run out.
        if (g.y < kMinY) {
            Land();
        } else if (here.ladder) {
            Climb(-1);
        } else if (here.wall) {
            Land();
        } else {
            SetState(kJump, 0x0F);
        }
    } else if (stick == 0x0D) {
        if (here.ladder) {
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
        if (here.ladder) {
            Land();
        } else {
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

const uint8_t* PoseBytes(int pose, int plane) {
    return g.frames.data() + static_cast<size_t>(pose * 2 + plane) * kFrameBytes;
}

namespace {

void TakeLamp(int col, int tileRow) {
    const int tile = TileAt(col, tileRow);
    if (tile != 0x13 && tile != 0x14 && tile != 0x31) {
        return;
    }
    int mount = 0;
    if (!ClaimLamp(col, tileRow, mount)) {
        return;
    }
    ReplaceLamp(col, tileRow, mount);
    if (g.score <= 999999 - 125) {
        g.score += 125;
    }
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
    const bool prize = ProbePose(g.pose, g.x, g.y).bit1;
    const bool falling = g.action == kFall;
    for (int dr = 0; dr < 2; ++dr) {
        for (int dc = 0; dc < 3; ++dc) {
            const int col = col0 + dc;
            const int row = row0 + dr;
            const int tile = TileAt(col, row);
            if (tile >= 2 && tile <= 5) {
                if (TakeExit(tile, xpos, ypos, wentLeft, wentRight, wentUp, falling)) {
                    return true;
                }
            }
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
    StandStill();
    wentLeft = false;
    wentRight = false;
    wentUp = false;
    for (int n = 0; n < 48 && PoseWall(g.pose, g.x, g.y); ++n) {
        g.y -= 1;
    }
}

void Tick() {
    const bool left = (GetAsyncKeyState(VK_LEFT) & 0x8000) != 0;
    const bool right = (GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0;
    const bool up = (GetAsyncKeyState(VK_UP) & 0x8000) != 0;
    const bool down = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0;
    const bool fire = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

    // $40BC runs before the overlap, and a held pose still reaches it.
    AnimateLamps();

    // scan_specials runs before the hold check, so a pose that is sitting
    // still can still claim a lamp or take an exit. The step flags are the
    // ones left by the previous frame. A hold keeps them, the way $1C62
    // returns before $1C8F clears the counters.
    if (ScanSpecials()) {
        wentLeft = false;
        wentRight = false;
        wentUp = false;
        StandStill();
        Compose();
        return;
    }

    // $0106. The picture stays, and this frame does not read the stick.
    if (g.hold > 0) {
        g.hold -= 1;
        Compose();
        return;
    }
    wentLeft = false;
    wentRight = false;
    wentUp = false;

    // $CB counts down only on a frame that actually runs, and only while the
    // state is the same one $C2 remembered. A fresh state reloads 6. Walking
    // and climbing take two pixels until that count reaches 0, then three.
    const int state = g.action;
    if (state == 0 || state != g.prevState) {
        g.repeat = 6;
    } else if (g.repeat > 0) {
        g.repeat -= 1;
    }
    g.prevState = state;

    // Joystick nibble, active low, as $1689 stores it in $98. Fire is $9B
    // cleared. Opposite directions cancel, which a stick cannot do.
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

    const Contact here = Probe(g.x, g.y);
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
        if (g.x < start) {
            wentLeft = true;
        } else if (g.x > start) {
            wentRight = true;
        }
    }
    Compose();
}
