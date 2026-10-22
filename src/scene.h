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

// The three multicolor colors for this scanline: background, $D022, $D023.
// Screen y 0 is raster 50. The splits and the colors come from the room.
void BandAt(int py, int& bg, int& c01, int& c10);

// Palette index of the room pixel at (x, y). -1 means "draw the background":
// the cell is empty ($DC), the dot is the background bit pair, or (x, y)
// is off the 320 by 200 screen.
int PixelColor(int x, int y);
