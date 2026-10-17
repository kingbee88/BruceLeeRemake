#pragma once

// One sample of every connected XInput pad, combined. Directions are the
// D-pad or the left stick. fire is the A button.
struct PadInput {
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
    bool fire = false;
};

PadInput ReadPad();
