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

#include <string>

// Settings from config.ini, or from config.local.ini when that file is
// present, with the command line applied on top.
struct Config {
    // True keeps FALLS on the full starting count for the whole game.
    bool infiniteLives = false;
    // True: the Ninja and Yamo still attack. A blow from either one never
    // lands on Bruce.
    bool enemiesCantHitBruce = false;
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
    // Player 1's controller id from Settings. Empty uses the first gamepad
    // found. The word none is no controller. Nothing is read when that pad
    // is not connected and no other pad is free.
    std::wstring gamepad;
    // Player 2's controller, Yamo when the options page is set to Opponent.
    // Empty uses the second gamepad. The word none is no controller.
    std::wstring gamepad2;
    // $4388. The best score, shown as TOP. Written with the other settings
    // when the program closes. 0 until a score passes it.
    int topScore = 0;
    // True opens the welcome wizard before the game window. The wizard
    // writes false when it closes.
    bool firstTimeRun = true;
    // False leaves the game running when another program is in front.
    // Command line only. SaveConfig does not write it.
    bool pauseOnFocusLoss = true;
};

extern Config gConfig;

// Look beside the exe, then one folder up, then two folders up. In each
// folder, config.local.ini is read when it is there, and otherwise
// config.ini. A closer folder still wins. The file that was opened is the
// one SaveConfig writes. Then apply Name=Value words from the command line.
void LoadConfig(const wchar_t* commandLine);

// Start room clamped into 0 .. 19. 0 is the opening room.
int ClampedStartRoom();

// Write the live settings to the file LoadConfig opened. Comments and
// blank lines stay. If no file was opened, write config.ini beside the exe.
void SaveConfig();

// Folder of the config file LoadConfig opened, with no trailing slash.
// That file is config.local.ini when it was the one read, and otherwise
// config.ini. When no file was opened, this is the folder beside the exe,
// where config.ini would be written.
std::wstring ConfigDirectory();
