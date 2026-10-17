#pragma once

// Result of the original overlap test at one sprite position.
// wall: $B6 bit 3. Nibbles 3, 7, and 11-15 on property 0. $1CC7 pushes him out.
// ground: $B6 bit 0. Nibbles 1, 4, 5, 6, 7, 9, and 13. The roof dots are 5.
// ladder: $B6 bit 2. Those same solid nibbles on any other property, including
// the ladder rails and the black line of a property-6 roof. Up and down climb.
// floor: bit 0 from a nibble that is not also bit 2.
// Either ground or ladder stops a fall. Statue ink sets neither.
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

// Rows above the feet. $A7 is this result: the scan saved before the bottom
// six sprite rows. A fall nudges sideways when this still has ground.
Contact ProbeUpper(int sx, int sy);

// Wall flag for one pose, ignoring what the rest of the walk cycle does.
// PlaceBruce and the end-of-frame shove use this on the picture just chosen.
bool PoseWall(int pose, int sx, int sy);
