#pragma once

#include <cstdint>
#include <vector>

constexpr int kWidth = 320;
constexpr int kHeight = 200;
constexpr int kSpriteRows = 21;
constexpr int kSpriteCols = 24;
constexpr int kFrameBytes = kSpriteRows * 3;
constexpr int kFrames = 7;

// Pepto. Stored as 0xAARRGGBB, which is BGRA in memory on this machine.
extern const uint32_t kPalette[16];

struct Game {
    std::vector<uint8_t> charset;
    std::vector<uint8_t> room;
    std::vector<uint8_t> color;
    std::vector<uint8_t> frames;
    std::vector<uint32_t> pixels;
    int x = 0;
    int y = 0;
    int vy = 0;
    int facing = 1;
    int pose = 0;
    int tick = 0;
    bool jumpHeld = false;
};

extern Game g;
