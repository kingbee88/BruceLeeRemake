#pragma once

// The three multicolor colors for this scanline: background, $D022, $D023.
// Screen y 0 is raster 50. The colors change at raster $78 and $97.
void BandAt(int py, int& bg, int& c01, int& c10);

// Palette index of the room pixel at (x, y). -1 means "draw the background":
// the cell is empty ($DC), the dot is the background bit pair, or (x, y)
// is off the 320 by 200 screen.
int PixelColor(int x, int y);
