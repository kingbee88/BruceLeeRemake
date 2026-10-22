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

// The other two fighters. Actor 1 is the Ninja (active flag $81, sprite
// colors green and black). Actor 2 is Yamo (active flag $41). $2719 forces
// his sprite color to $F0, so he is drawn black. Both use the same pose
// numbers as Bruce, including the spawn, strike, and knockdown pictures.

// Read data/ninja.bin and data/yamo.bin. False when either file is missing.
bool LoadFighters();

// Drop both fighters and arm an immediate spawn. PlaceBruce and a room
// change call this.
void ResetFighters();

// One logic frame for both fighters, then the sprite hits. True when that
// hit spent one of Bruce's falls. The room is already the one he respawned in.
bool TickFighters();

// Paint whichever of them is in the room. Bruce is drawn after this, so he
// stays in front. That is sprites 0 and 1 above sprites 2-4.
void DrawFighters();
