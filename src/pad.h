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

#include <string>
#include <vector>

// The controller chosen in Settings. When that choice is not connected, an
// Xbox-style pad is used if one is, and otherwise the first controller
// Windows reports. Directions are the D-pad, the left stick, or a plain
// stick's axes. fire is the A button. On a stick whose buttons have no
// names, each of those buttons punches. menu is the menu button, Start on
// the Xbox layout. view is the View button, Back on that layout. y is the Y
// button. While the game is paused, holding it restarts. Every field stays
// clear unless this process owns the foreground window, so a pad does not
// drive the game behind another program.
struct PadInput {
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
    bool fire = false;
    bool menu = false;
    bool view = false;
    bool y = false;
};

// id is what config.ini stores. name is the Settings label. Two pads with
// the same hardware name get a number on the later ones.
struct GamepadChoice {
    std::wstring id;
    std::wstring name;
};

// items is every connected game controller. Xbox-style pads come first, then
// other controllers Windows.Gaming.Input reports, then controllers that only
// appear in the Windows Game Controllers list. The first Xbox-style pad is
// the primary. active is the one ReadPad samples, or -1 when items is empty.
struct GamepadList {
    std::vector<GamepadChoice> items;
    int active = -1;
};

// The next ReadPad polls the device. Further calls in the same logic frame
// reuse that sample, so a disconnected slot is not polled twice.
void BeginPadFrame();
PadInput ReadPad();

// Match the saved controller id. When it is connected, ReadPad samples it.
// Otherwise ReadPad samples the first connected controller, and the saved id
// becomes that controller. With none connected, the saved id is left unchanged.
GamepadList SyncGamepads();

// Gamepad names come from the Windows Runtime. Construct this on the main
// thread before anything else initializes COM, and keep it until shutdown.
struct PadSession {
    PadSession();
    ~PadSession();
    PadSession(const PadSession&) = delete;
    PadSession& operator=(const PadSession&) = delete;
};
