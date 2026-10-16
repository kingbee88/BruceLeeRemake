#pragma once

// Raster bands for room 0. Screen y 0 is raster 50. Splits are $78 and $97.
// Each band is background, $D022, $D023. Color RAM supplies the fourth color.
void BandAt(int py, int& bg, int& c01, int& c10);

// One screen pixel per multicolor dot. -1 is the band background.
int PixelColor(int x, int y);
