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

// Stored in Gamepad and Gamepad2 when that player has chosen no controller.
// An empty value still means the default pad for that player.
inline constexpr wchar_t kGamepadNone[] = L"none";

// items is every connected game controller. Xbox-style pads come first, then
// other controllers Windows.Gaming.Input reports, then controllers that only
// appear in the Windows Game Controllers list. active is player 1's pad in
// that list, and active2 is player 2's. -1 means that player has no pad.
struct GamepadList {
    std::vector<GamepadChoice> items;
    int active = -1;
    int active2 = -1;
};

// The next ReadPad and ReadPad2 poll their devices. Further calls in the
// same logic frame reuse that sample, so a disconnected slot is not polled twice.
void BeginPadFrame();
// Player 1. Arrows and Right Ctrl are read separately.
PadInput ReadPad();
// Player 2. W A S D and Left Ctrl are read separately.
PadInput ReadPad2();

// Match each saved controller id. When it is connected, that player samples
// it. A DirectInput id and a Windows.Gaming.Input id for the same pad are
// one controller. The first list in a session often has only the DirectInput
// name, so a saved id that is missing from that list is left unchanged.
// Once a saved controller has been found, and it is later absent, that
// player takes the first connected controller the other player is not
// using, or none when every connected pad is taken. An empty id is the
// default: player 1 gets the first pad, player 2 the second. The word none
// stays unassigned.
GamepadList SyncGamepads();

// Gamepad names come from the Windows Runtime. Construct this on the main
// thread before anything else initializes COM, and keep it until shutdown.
struct PadSession {
    PadSession();
    ~PadSession();
    PadSession(const PadSession&) = delete;
    PadSession& operator=(const PadSession&) = delete;
};
