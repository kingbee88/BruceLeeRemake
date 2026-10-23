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

// One sample of every connected XInput pad, combined. Directions are the
// D-pad or the left stick. fire is the A button. menu is the menu button,
// Start on the Xbox layout. view is the View button, Back on that layout.
// y is the Y button. While the game is paused, holding it restarts.
// Every field stays clear unless this process
// owns the foreground window, so a pad does not drive the game behind
// another program.
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

// The next ReadPad polls the devices. Further calls in the same logic
// frame reuse that sample, so the idle slots are counted once.
void BeginPadFrame();
PadInput ReadPad();
