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

// The options page and the "PLAYER 1" page that follow the title picture.
// Both are drawn from data/front.bin, copied out of the menu snapshot.

// Read data/front.bin. False when the file is missing or short.
bool LoadFront();

// Blue options page. twoPlayers is $42C9 (F3). humanOpponent is $42CA (F5).
// Both start clear: 1 PLAYER and COMPUTER are the highlighted words.
void ComposeMenu(bool twoPlayers, bool humanOpponent);

// Blue page from between_rooms. player 0 is PLAYER 1, player 1 is PLAYER 2.
void ComposePlayerUp(int player);

// $188E. Blue screen, the GAME OVER glyphs, PLAYER 1, and the big score.
// score is the 24-bit total. The low 6 decimal digits are drawn.
void ComposeGameOver(int score);

// A full-width blue band across the middle of the playfield. PAUSED is
// drawn in it with the PLAYER 1 glyphs. The band covers kPausedBandTop
// for kPausedBandHeight rows of g.pixels. restartProgress is 0..1 while
// Y is held to restart, and negative when that bar is hidden.
constexpr int kPausedBandTop = 76;
constexpr int kPausedBandHeight = 48;
void DrawPausedBanner(float restartProgress);

// A camera in the upper right of the 320 by 200 picture, just under the
// score bar so FALLS stays visible. Each source dot is drawn two pixels
// wide, and the edge is red. DrawCameraFlash paints it for ageMs in
// 0 .. kCameraFlashMs. The picture underneath is the caller's to restore.
constexpr int kCameraFlashX = 268;
constexpr int kCameraFlashY = 26;
constexpr int kCameraFlashW = 48;
constexpr int kCameraFlashH = 36;
constexpr int kCameraFlashMs = 700;
void DrawCameraFlash(int ageMs);
