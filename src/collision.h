#pragma once

// $B6 after the scan at $537E and the pack at $5431.
// wall is bit 3, the solid flag $1CC7 pushes back out.
// ground is bits 0 and 2, the don't-fall mask $05.
struct Contact {
    bool wall = false;
    bool ground = false;
};

Contact Probe(int sx, int sy);
bool PoseWall(int pose, int sx, int sy);
