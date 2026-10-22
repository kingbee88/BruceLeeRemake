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
constexpr int kCarry = 0x17;  // caught on a scrolling floor, tile $62
constexpr int kStuck = 0x18;  // $1CC7 reversed a step that still overlaps a wall
constexpr int kFlinch = 0x0F; // punched. Slides back in the stand picture.
constexpr int kPunch = 0x11;
constexpr int kKnock = 0x13;  // kicked. Libraries $38-$3F, then a fall if $D1 is negative.
constexpr int kStomp = 0x1B;  // someone fell on him. Same pictures as the knock.
constexpr int kZap = 0x19;    // beam or hot floor, library 8, then a life
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
// Libraries $2A, $2C, $2E, $30-$37, and $38-$3F. Spawn, Yamo's strike, knockdown.
constexpr int kPoseSpawn = 23;
constexpr int kPoseAppearLeft = 24;
constexpr int kPoseAppearRight = 25;
constexpr int kPoseStrikeLeft0 = 26;
constexpr int kPoseStrikeLeft1 = 27;
constexpr int kPoseStrikeRight0 = 28;
constexpr int kPoseStrikeRight1 = 29;
constexpr int kPoseKnockRight0 = 30;
constexpr int kPoseKnockRight1 = 31;
constexpr int kPoseKnockLeft0 = 32;
constexpr int kPoseKnockLeft1 = 33;

// While this is set, PoseBytes and the overlap test read this actor instead
// of Bruce. Clear it before the next Bruce probe.
void SetActorView(const uint8_t* frames, int action, int facing, int pose);
void ClearActorView();
int ViewAction();
int ViewFacing();
int ViewPose();

// $1826. One fall. A count that is still positive shows the PLAYER page
// and returns to the same room. A count that goes negative is game over.
void BruceFell();

// True from BruceFell until ResumeAfterFall. between_rooms is that page.
bool BetweenLives();

// $1828. The fall that wrapped $28 negative. Cleared once the screen is up.
bool OutOfLives();
void ClearOutOfLives();

// The PLAYER page has finished. Same room, health reloaded in PlaceBruce.
void ResumeAfterFall();

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
