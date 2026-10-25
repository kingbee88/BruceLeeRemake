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

// Settings from config.ini, with the command line applied on top.
struct Config {
    // True keeps FALLS on the full starting count for the whole game.
    bool infiniteLives = false;
    // True: the Ninja and Yamo still attack, and those blows never land.
    bool enemiesCantHitYou = false;
    // Off keeps the original landing loop. On pays 50 once, and one knockout.
    bool fixBugsInOriginal = false;
    int startRoom = 0;
    bool showTitleScreen = true;
    // Borderless window the size of the monitor. A double-click toggles it.
    // The maximize button turns it on.
    bool fullScreen = false;
    bool showCrtFilter = false;
    // Parts of the television filter. They apply while showCrtFilter is on.
    bool crtCurve = true;
    bool crtScanlines = true;
    bool crtBlur = true;
    bool crtVignette = true;
    // False leaves the rooms and the options page silent.
    bool sound = true;
    // True opens the welcome wizard before the game window. The wizard
    // writes false when it closes.
    bool firstTimeRun = true;
    // False leaves the game running when another program is in front.
    // Command line only. SaveConfig does not write it.
    bool pauseOnFocusLoss = true;
};

extern Config gConfig;

// Read config.ini beside the exe. If that file is not there, read config.ini
// one folder up, then two folders up. The file that was opened is the one
// SaveConfig writes. Then apply Name=Value words from the command line.
void LoadConfig(const wchar_t* commandLine);

// Start room clamped into 0 .. 19. 0 is the opening room.
int ClampedStartRoom();

// Write the live settings to the config.ini LoadConfig opened. Comments and
// blank lines stay. If no file was opened, write config.ini beside the exe.
void SaveConfig();
