#pragma once

// The effect scripts and the options tune are the original bytes in
// data/sound.bin. Effects are the Y values play_sound takes, $80-$8E.
// The options page runs the tune. Play runs the three effect voices.
// A logic frame is three PAL frames. The chip is clocked in real time.

// False only when Sound is on and data\sound.bin cannot be read.
// A missing audio device leaves the game silent and still returns true.
bool StartAudio();
void StopAudio();

void PlayEffect(int id);
void SilenceSound();
void StartMenuMusic();
void StopMenuMusic();
