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

// The other two fighters. Actor 1 is Yamo, the green fighter (active flag
// $81, sprite colors green and black). His knockout is 450. Actor 2 is the
// ninja (active flag $41). $2719 forces his sprite color to $F0, so he is
// drawn black, and his knockout is 200. The variables and the files
// ninja.bin and yamo.bin are named the other way around. Both use the same
// pose numbers as Bruce, including the spawn, strike, and knockdown pictures.

// Read data/ninja.bin and data/yamo.bin. False when either file is missing.
bool LoadFighters();

// Drop both fighters and arm an immediate spawn. PlaceBruce and a room
// change call this.
void ResetFighters();

// Stand both of them for one picture, then RestoreFighters puts them back.
// Coordinates are screen pixels, the same as g.x and g.y. Facing is -1 or +1.
void StandFighters(int ninjaX, int ninjaY, int ninjaFacing, int yamoX, int yamoY, int yamoFacing);
void RestoreFighters();

// One logic frame for both fighters, then the sprite hits. True when that
// hit spent one of Bruce's falls. The room is already the one he respawned in.
bool TickFighters();

// A new game. Yamo is human again when the options page says OPPONENT,
// and the idle countdown starts over.
void ResetOpponentIdle();

// Paint whichever of them is in the room. Bruce is drawn after this, so he
// stays in front. That is sprites 0 and 1 above sprites 2-4.
void DrawFighters();
