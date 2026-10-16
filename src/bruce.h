#pragma once

#include <cstdint>

// Poses follow the animation selector at $A780.
// 0 right stand (base 2), 1 left stand (base 0),
// 2/3 left walk ($0E, $10), 4/5 right walk ($12, $14), 6 air (base 8).
int StandPose();
int WalkPose();
const uint8_t* PoseBytes(int pose, int plane);

void PlaceBruce();
void Tick();
