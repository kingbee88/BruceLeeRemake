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
