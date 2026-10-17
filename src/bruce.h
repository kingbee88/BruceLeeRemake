#pragma once

#include <cstdint>

// Pose numbers index bruce.bin. See DESIGN.md for the bytes.
// 0 stand right, 1 stand left, 2/3 walk left, 4/5 walk right.
// 6 legs together (the fall, and the first and last jump counts).
// 7 jump tuck, 8 the jump with the arms out.
// 9/10 sideways jump left, 11/12 sideways jump right.
// 13 punch left, 14 punch right.
// 15/16 flying kick left, 17/18 flying kick right.
// 19/20 the two climb pictures. Up and down on a ladder alternate them.
// 21/22 duck left and duck right. Down on a floor.
// Each direction is its own drawing. Mirroring would put the solid pixels
// on the wrong side of a wall.

// Original state numbers stored in g.action.
constexpr int kClimbUp = 1;
constexpr int kClimbDown = 2;
constexpr int kWalkLeft = 3;
constexpr int kWalkRight = 4;
constexpr int kJump = 5;
constexpr int kSideLeft = 6;
constexpr int kSideRight = 7;
constexpr int kFlyLeft = 0x0C;
constexpr int kFlyRight = 0x0D;
constexpr int kFall = 0x0E;
constexpr int kPunch = 0x11;
constexpr int kDuck = 0x1A;

constexpr int kPoseLegs = 6;
constexpr int kPoseArms = 8;
constexpr int kPosePunchLeft = 13;
constexpr int kPosePunchRight = 14;
constexpr int kPoseFlyLeftExtended = 16;
constexpr int kPoseClimb0 = 19;
constexpr int kPoseClimb1 = 20;
constexpr int kPoseDuckLeft = 21;
constexpr int kPoseDuckRight = 22;

// The stand picture for g.facing.
int StandPose();

// The current stride. The two frames alternate every logic frame.
int WalkPose();

// Screen pixels added to g.x when this plane is drawn or tested.
// Punch left, and the black plane of the extended flying kick left, are
// shifted 16 pixels left. That is the original $9E adjustment of -8.
int PlaneShift(int pose, int plane);

// Pointer to one plane of a pose. Plane 0 is the yellow body, plane 1 is
// the black detail. Each plane is 21 rows of 4 bytes.
const uint8_t* PoseBytes(int pose, int plane);

// Stand him where this room starts. Facing is to the right. Lifts him if
// that stand picture is already inside a wall.
void PlaceBruce();

// One original logic frame. The window waits 60ms between calls, which is
// three PAL screens, the delay the game counts down in $48.
void Tick();
