#pragma once

// Result of the original overlap test at one sprite position.
// wall: $1CC7 push-out on property 0 (nibbles 3, 7, and 11-15).
// ground: $B6 bit 0 (nibbles 1, 4, 5, 6, 7, 9, and 13). The roof dots are 5.
// floor: that ground came from something other than a ladder, so a climb
// down stops there. ladder: he is touching tiles $20, $21, or $2C.
// Statue ink sets neither. The nibble table is in DESIGN.md.
struct Contact {
    bool wall = false;
    bool ground = false;
    bool floor = false;
    bool ladder = false;
};

// Test the picture he would actually be showing at (sx, sy):
// the jump, sideways jump, kick, or punch already chosen while that move runs,
// both walk strides while a direction is held,
// otherwise the stand pose for g.facing.
Contact Probe(int sx, int sy);

// The same test, using only the bottom of the sprite (rows 16–20).
Contact ProbeFeet(int sx, int sy);

// Wall flag for one pose, ignoring what the rest of the walk cycle does.
// PlaceBruce and the end-of-frame shove use this on the picture just chosen.
bool PoseWall(int pose, int sx, int sy);
