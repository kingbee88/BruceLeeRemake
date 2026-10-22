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
