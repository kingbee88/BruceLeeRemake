#pragma once

#include <cstdint>
#include <vector>

constexpr int kWidth = 320;
constexpr int kHeight = 200;
constexpr int kSpriteRows = 21;
constexpr int kSpriteCols = 32;
constexpr int kSpriteRowBytes = kSpriteCols / 8;
constexpr int kFrameBytes = kSpriteRows * kSpriteRowBytes;
constexpr int kFrames = 23;

// Pepto C64 colors, packed as 0xAARRGGBB. Index 0 is black. See DESIGN.md.
extern const uint32_t kPalette[16];

// The play state shared by every source file.
// charset, room, color, and frames are the four data files, loaded as-is.
// DESIGN.md describes each byte. x and y are Bruce's top-left in screen pixels.
struct Game {
    std::vector<uint8_t> charset;
    std::vector<uint8_t> room;
    std::vector<uint8_t> color;
    std::vector<uint8_t> frames;
    std::vector<uint32_t> pixels;
    int x = 0;
    int y = 0;
    int facing = 1;        // -1 left, +1 right. Original $A4 is 0 left, 1 right.
    int pose = 0;          // which bruce.bin pose Compose draws
    int tick = 0;          // walk frames; the two strides alternate each logic frame
    int action = 0;        // original state in $BF. 0 means none
    int actionTimer = 0;   // $CE. Counts down through g.action. 0 means the script is idle
    int hold = 0;          // $0106. Logic frames to skip before the next step
    int repeat = 6;        // $CB. Counts down while the same state continues
    int prevState = 0;     // $C2. State at the start of the previous logic frame
    int fallSlide = 0;     // $B0. One sideways nudge on the second falling frame
    int score = 0;         // $41..$43, a 24-bit count. A lamp adds 125.
};

extern Game g;
