#pragma once

#include <cstdint>
#include <vector>

constexpr int kWidth = 320;
constexpr int kHeight = 200;
constexpr int kSpriteRows = 21;
constexpr int kSpriteCols = 32;
constexpr int kSpriteRowBytes = kSpriteCols / 8;
constexpr int kFrameBytes = kSpriteRows * kSpriteRowBytes;
constexpr int kFrames = 21;

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
    int vy = 0;             // vertical step this frame; negative is up
    int facing = 1;         // -1 left, +1 right
    int pose = 0;           // which bruce.bin pose Compose draws
    int tick = 0;           // frames a direction has been held; 0 means standing
    int jumpTimer = 0;  // state $05, counts down from 15; 0 means he is not in that jump
    int action = 0;     // 0 none, 6/7 sideways jump, $0C/$0D flying kick, $11 punch
    int actionTimer = 0;  // counts down across g.action; the picture follows it
    int climbHold = 0;    // frames to wait before the next ladder step
    int climbStep = 0;    // 0 or 1, which of the two climb pictures is showing
    bool jumpHeld = false;    // up is still down, so landing does not jump again
    bool attackHeld = false;  // space is still down, so the attack does not repeat
};

extern Game g;
