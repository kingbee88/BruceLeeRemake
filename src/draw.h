#pragma once

#include <cstdint>

// Paint the room, Bruce's current pose, and the score line into g.pixels.
// Call after Tick has chosen g.pose and g.x / g.y.
void Compose();

// Paint title.bin. It is the multicolor bitmap at $2000. Every cell uses the
// same four colors from the title snapshot. Light blue along the outer edge
// is the border color, so it is drawn as the grey field.
void ComposeTitle(const uint8_t* bitmap);
