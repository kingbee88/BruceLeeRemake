#pragma once

#include <cstdint>

// Paint the room, Bruce's current pose, and the score line into g.pixels.
// Call after Tick has chosen g.pose and g.x / g.y.
void Compose();

// Paint title.bin. A set bit is white and a clear bit is black. The file
// carries no color bytes, so this is the hires picture on its own.
void ComposeTitle(const uint8_t* bitmap);
