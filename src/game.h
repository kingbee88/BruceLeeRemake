// Bruce Lee, a Windows port.
// Copyright (C) 2026 Jacob Marner
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// No copyright is claimed over the original game, which is copyright 1984 Datasoft, Inc.

#pragma once

#include <cstdint>
#include <vector>

constexpr int kWidth = 320;
constexpr int kHeight = 200;
constexpr int kSpriteRows = 21;
constexpr int kSpriteCols = 32;
constexpr int kSpriteRowBytes = kSpriteCols / 8;
constexpr int kFrameBytes = kSpriteRows * kSpriteRowBytes;
// 0-22 are the poses DESIGN.md lists. 23-33 are libraries $2A-$3F: the
// spawn, the strike, and the knockdown. Bruce uses those when he is hit.
constexpr int kFrames = 34;

// Colodore C64 colors, packed as 0xAARRGGBB. Index 0 is black. See DESIGN.md.
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
    int score = 0;         // $41..$43, a 24-bit count. The line shows the low 6 digits.
    int lives = 4;         // $28. The status line shows this as FALLS. One byte.
    int lifeStock = 4;     // clear_work_ram's 4, or 9 for a human opponent.
    int health = 0x24;     // $D1. A hit subtracts from this. Below 0 is a fall.
    int roomId = 0;        // $29. 0 is the opening room. StartRoom uses this same number.
    int charsetId = 0;     // 0 is $A000, 1 is $A800, 2 is $B000.
    // Four bands of background, $D022, $D023. The low nibble is the VIC color.
    // splits are raster lines. $FD ends the list. Screen y 0 is raster 50.
    uint8_t bands[12] = {0x0B, 0x01, 0x0E, 0x0B, 0x02, 0x01, 0x0C, 0x02, 0x01, 0x06, 0x0E, 0x01};
    uint8_t splits[4] = {0x78, 0x97, 0xFD, 0x78};
};

extern Game g;

// $42CA. F5 on the options page. Clear is the computer. Set is a human
// opponent on Yamo, actor 1. The menu copy stays set when an idle Yamo is
// handed back to the computer for the rest of that game.
extern bool gHumanOpponent;
// Set for the rest of a game when Yamo's stick stays neutral long enough.
// The options page still shows OPPONENT.
extern bool gOpponentYielded;

// $3175. Add amount to one score byte, then $312C. Slot 0 is $41, the high
// byte, and slot 2 is $43. The sum wraps at 24 bits. When every score byte
// is at least the mark in $4392, $28 gains one fall and the mark grows by
// the step in $438E. The mark starts at 40000 and the step is 30000.
// The best score, $4388, moves up when this total passes it.
void AddScoreByte(int slot, int amount);

// $2C2C. X is the victim. Kind 1 is the green fighter, Yamo in the manual:
// $01 in the middle byte and $C2 in the low byte, 450. Any other fighter is
// $C8 in the low byte, 200. That fighter is the ninja, the one with the stick.
void AddKnockout(int victimKind);

// $2BF9 for a landing blow. Health is one byte. Each pass adds 50 when the
// attacker is Bruce, then subtracts 5. A negative byte pays the knockout
// and the pass runs again, so the byte comes back in 0..127. scored is
// bit 7 of $BC: the attacker was Bruce.
int ApplyStomp(int health, int victimKind, bool scored);

// $3155 and $2FE5. One more fall. The counter is a single byte.
// InfiniteLives leaves the count on lifeStock.
void GrantLife();

// InfiniteLives pins FALLS to lifeStock. Otherwise this does nothing.
void SyncLives();

// clear_work_ram's score and the extra-fall mark. The top score stays.
void ResetScore();

// $4388. Compared as a 24-bit total. The line shows its low 6 digits.
// TopScore in the settings file keeps it for the next time the program starts.
int TopScore();

// The top score becomes the score on screen. That score is 0 at the start
// of a game and after game over.
void ResetTopScore();

// $1AED. Two screen characters. 0..99 are digits. From 100 the high
// nibble is A-F, and 160 paints as 00.
void FormatFalls(int lives, char out[3]);

// $0805. init_hardware stores $11 and timed_caption leaves $10, so the
// word is showing. between_lives runs that init again. $1A42, the pause
// entry, uses the same pair of instructions and the word stays up.
void ResetPlayerCaption();

// $1A96, called from frame at $1BEB. $10 prints the word and 0 blanks it,
// then reloads $20. Sixteen logic frames on, sixteen off. A negative
// $0105 skips the call, so a pause holds the count.
void TickPlayerCaption();

// $43BD in $01-$10. The cells are row 1, columns 4-6.
bool PlayerCaptionOn();
