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

// Paint the room, Bruce's current pose, and the score line into g.pixels.
// Call after Tick has chosen g.pose and g.x / g.y.
void Compose();

// Paint title.bin. It is the multicolor bitmap at $2000. Every cell uses the
// same four colors from the title snapshot. Light blue along the outer edge
// is the border color, so it is drawn as the grey field.
void ComposeTitle(const uint8_t* bitmap);

// Room 1, with the Ninja and Yamo standing on its two spawn points.
// The live picture is put back. False when that room cannot be read.
bool ComposeWelcomeRoom(std::vector<uint32_t>& dest);
