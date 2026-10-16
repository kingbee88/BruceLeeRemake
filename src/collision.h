#pragma once

// Result of the original overlap test at one sprite position.
// wall: the move would be pushed back out (solid patterns on property 0).
// ground: he should not fall (edge patterns, or solid patterns on property 6).
// Statue ink sets neither. The nibble table is in DESIGN.md.
struct Contact {
    bool wall = false;
    bool ground = false;
};

// Test the picture he would actually be showing at (sx, sy):
// the jump, sideways jump, kick, or punch already chosen while that move runs,
// both walk strides while a direction is held,
// otherwise the stand pose for g.facing.
Contact Probe(int sx, int sy);

// Wall flag for one pose, ignoring what the rest of the walk cycle does.
// PlaceBruce and the end-of-frame shove use this on the picture just chosen.
bool PoseWall(int pose, int sx, int sy);
