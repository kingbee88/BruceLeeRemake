#pragma once

#include <cstdint>

// Pose numbers index bruce.bin. See DESIGN.md for the bytes.
// 0 stand right, 1 stand left, 2/3 walk left, 4/5 walk right, 6 in the air.
// Each direction is its own drawing. Mirroring would put the solid pixels
// on the wrong side of a wall.

// The stand picture for g.facing.
int StandPose();

// The current stride. g.tick / 6 alternates the two frames for g.facing.
int WalkPose();

// Pointer to one plane of a pose. Plane 0 is the yellow body, plane 1 is
// the black detail. Each plane is 21 rows of 3 bytes.
const uint8_t* PoseBytes(int pose, int plane);

// Stand him on the floor just to the right of the lamp. Lifts him if that
// stand picture is already inside a wall.
void PlaceBruce();

// One frame: read the arrows, step, jump or fall, pick the pose, shove him
// out of a wall if the new picture overlaps one, then paint.
void Tick();
