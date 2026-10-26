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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// Menu command ids. Grayed commands are drawn with a shortcut and do nothing.
enum AppCommand : UINT {
    ID_RESTART = 40001,
    ID_SAVE,
    ID_LOAD,
    ID_QUICKSAVE,
    ID_QUICKLOAD,
    ID_EXIT,
    ID_MANUAL,
    ID_ABOUT,
    ID_SETTINGS,
    ID_KEYS,
    ID_CONTROLLER,
    ID_SCREENSHOT,
};

// The Settings window asks the main window to apply these.
constexpr UINT WM_APP_FULLSCREEN = WM_APP + 1;
constexpr UINT WM_APP_SOUND = WM_APP + 2;
constexpr UINT WM_APP_RESET_TOP = WM_APP + 3;

HMENU CreateAppMenu();
HACCEL CreateAppAccelerators();

void ApplyAppTheme(HWND hwnd);
void ToggleSettings(HWND owner);
void CloseSettings();
bool SettingsIsOpen();
bool SettingsContains(HWND hwnd);
bool UiOwnsKeyboard();
void RefreshSettingsDialog();
bool SettingsDialogMessage(MSG* msg);

void OpenManual(HWND owner);
void ShowAbout(HWND owner);
// Welcome wizard. False means that loop was quit, and the game window
// should stay hidden.
bool ShowIntroWizard(HWND owner);
void ShowKeyboardShortcuts(HWND owner);
void ShowControllerMapping(HWND owner);

bool OnMenuMeasure(LPARAM lp);
bool OnMenuDraw(LPARAM lp);
bool PaintDarkMenuBar(HWND hwnd, LPARAM lp);
void OnMenuSelect(HWND hwnd, WPARAM wp, LPARAM lp);
void HideGearTip();
void ReleaseUi();
