#pragma once

#include <cstdint>
#include <vector>

constexpr int kWidth = 320;
constexpr int kHeight = 200;
constexpr int kSpriteRows = 21;
constexpr int kSpriteCols = 24;
constexpr int kFrameBytes = kSpriteRows * 3;
constexpr int kFrames = 7;

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
    bool jumpHeld = false;  // up is still down, so landing does not jump again
};

extern Game g;
