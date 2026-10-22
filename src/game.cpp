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

#include "game.h"

#include "config.h"

#include <cstdint>

// Colodore palette
const uint32_t kPalette[16] = {
    0xFF000000u, 0xFFFFFFFFu, 0xFF96282Eu, 0xFF5BD6CEu,
    0xFF9F2DADu, 0xFF41B936u, 0xFF2724C4u, 0xFFEFF347u,
    0xFF9F4815u, 0xFF5E3500u, 0xFFDA5F66u, 0xFF474747u,
    0xFF787878u, 0xFF91FF84u, 0xFF6864FFu, 0xFFAEAEAEu,
};

Game g;

namespace {

// $4392. clear_work_ram copies $438B, which is 00 9C 40.
int lifeMark = 0x009C40;
// $438E. Added into the mark after each extra fall. 00 75 30.
constexpr int kLifeStep = 0x007530;
// $4388. Stays 0 until the score passes it. A new process starts it at 0.
int topScore = 0;

bool MarkReached(int score, int mark) {
    for (int shift = 16; shift >= 0; shift -= 8) {
        const int have = (score >> shift) & 0xFF;
        const int need = (mark >> shift) & 0xFF;
        if (have < need) {
            return false;
        }
    }
    return true;
}

}  // namespace

void GrantLife() {
    // The trainer's LDA patch covers this INC and the one in room 16.
    if (gConfig.infiniteLives) {
        g.lives = g.lifeStock;
        return;
    }
    g.lives = (g.lives + 1) & 0xFF;
}

void SyncLives() {
    if (gConfig.infiniteLives) {
        g.lives = g.lifeStock;
    }
}

void ResetScore() {
    g.score = 0;
    lifeMark = 0x009C40;
}

int TopScore() {
    return topScore;
}

void FormatFalls(int lives, char out[3]) {
    int value = lives & 0xFF;
    int tens = 0;
    while (value >= 10) {
        value -= 10;
        tens += 1;
    }
    const int packed = static_cast<int>(static_cast<uint8_t>(tens << 4)) | value;
    const int hi = (packed >> 4) & 0x0F;
    const int lo = packed & 0x0F;
    auto glyph = [](int nibble) -> char {
        if (nibble < 10) {
            return static_cast<char>('0' + nibble);
        }
        return static_cast<char>('A' + (nibble - 10));
    };
    out[0] = glyph(hi);
    out[1] = glyph(lo);
    out[2] = '\0';
}

void AddScoreByte(int slot, int amount) {
    if (slot < 0 || slot > 2) {
        return;
    }
    const int add = (amount & 0xFF) << ((2 - slot) * 8);
    g.score = (g.score + add) & 0xFFFFFF;
    // $313A. High byte first. A lower byte that is still under its pair
    // waits, even when the total is already past the mark.
    if (MarkReached(g.score, lifeMark)) {
        lifeMark = (lifeMark + kLifeStep) & 0xFFFFFF;
        GrantLife();
    }
    if (topScore < g.score) {
        topScore = g.score;
    }
}

void AddKnockout(int victimKind) {
    if (victimKind == 1) {
        AddScoreByte(1, 0x01);
        AddScoreByte(2, 0xC2);
        return;
    }
    if (victimKind != 0) {
        AddScoreByte(2, 0xC8);
    }
}

// Intentional bug, kept so a killing landing scores the way the original
// does. force_knockdown at $2C51 jumps back to this test at $2BF9 while the
// landing bit is still set, so 50 and the knockout repeat until the health
// byte is positive again. FixBugsInOriginal pays that landing once instead.
int ApplyStomp(int health, int victimKind, bool scored) {
    int h = health & 0xFF;
    for (;;) {
        if (scored) {
            AddScoreByte(2, 0x32);
        }
        h = (h - 5) & 0xFF;
        if ((h & 0x80) == 0) {
            return h;
        }
        if (scored) {
            AddKnockout(victimKind);
        }
    }
}
