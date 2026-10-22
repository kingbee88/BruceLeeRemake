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

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <xinput.h>

#include "pad.h"

#pragma comment(lib, "xinput.lib")

namespace {

// XInputGetState on an empty slot can stall. A slot that just answered
// "not connected" is left alone for about a second of logic frames.
int idle[4] = {};

void Take(PadInput& pad, const XINPUT_GAMEPAD& gamepad) {
    const WORD buttons = gamepad.wButtons;
    if ((buttons & XINPUT_GAMEPAD_DPAD_LEFT) != 0) {
        pad.left = true;
    }
    if ((buttons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0) {
        pad.right = true;
    }
    if ((buttons & XINPUT_GAMEPAD_DPAD_UP) != 0) {
        pad.up = true;
    }
    if ((buttons & XINPUT_GAMEPAD_DPAD_DOWN) != 0) {
        pad.down = true;
    }
    if ((buttons & XINPUT_GAMEPAD_A) != 0) {
        pad.fire = true;
    }

    // The stick is a direction once it leaves the dead zone XInput defines
    // for the left thumb. Past that, each axis is on or off, same as a hat.
    const SHORT x = gamepad.sThumbLX;
    const SHORT y = gamepad.sThumbLY;
    if (x < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
        pad.left = true;
    } else if (x > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
        pad.right = true;
    }
    if (y > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
        pad.up = true;
    } else if (y < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
        pad.down = true;
    }
}

}  // namespace

PadInput ReadPad() {
    PadInput pad;
    for (DWORD i = 0; i < 4; ++i) {
        if (idle[i] > 0) {
            idle[i] -= 1;
            continue;
        }
        XINPUT_STATE state{};
        if (XInputGetState(i, &state) != ERROR_SUCCESS) {
            idle[i] = 16;
            continue;
        }
        Take(pad, state.Gamepad);
    }
    return pad;
}
