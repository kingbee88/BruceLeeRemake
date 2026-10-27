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

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <winreg.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <wincodec.h>

#include "ui.h"

#include "config.h"
#include "crt.h"
#include "draw.h"
#include "game.h"
#include "load.h"
#include "pad.h"
#include "room.h"

#include <cmath>
#include <cstdint>
#include <vector>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace {

constexpr int IDC_LIVES = 201;
constexpr int IDC_ENEMIES = 202;
constexpr int IDC_ROOM = 203;
constexpr int IDC_TITLE = 204;
constexpr int IDC_FIX = 205;
constexpr int IDC_FULL = 206;
constexpr int IDC_CRT = 207;
constexpr int IDC_SOUND = 208;
constexpr int IDC_HINT = 209;
constexpr int IDC_ROOM_LABEL = 210;
constexpr int IDC_CRT_CURVE = 211;
constexpr int IDC_CRT_SCANLINES = 212;
constexpr int IDC_CRT_BLUR = 213;
constexpr int IDC_CRT_VIGNETTE = 214;
constexpr int IDC_GAMEPAD = 215;
constexpr int IDC_GAMEPAD_LABEL = 216;
constexpr int IDC_RESET_TOP = 217;
constexpr int IDC_GAMEPAD2 = 218;
constexpr int IDC_GAMEPAD2_LABEL = 219;
constexpr UINT_PTR kGamepadTimer = 1;
constexpr wchar_t kNoGamepad[] = L"<no gamepad found>";
constexpr int IDC_WIZ_MODERN = 220;
constexpr int IDC_WIZ_AUTHENTIC = 221;
constexpr int IDC_WIZ_MODERN_PIC = 222;
constexpr int IDC_WIZ_AUTHENTIC_PIC = 223;
constexpr int IDC_WIZ_NOTE = 224;
constexpr int IDC_WIZ_ADVICE = 225;
// Which welcome-wizard experience is selected. Owner-drawn buttons do not
// keep a check of their own.
bool gWizardModern = true;
// Room for the focus ring to the left of the checkbox. The ring is clipped if it
// shares the checkbox's left edge.
constexpr int kFocusPad = 4;

constexpr wchar_t kAboutText[] =
    L"Ported to Windows by Jacob Marner.\n"
    L"The Windows port is copyright Jacob Marner.\n"
    L"This port is free software under the GNU General Public License, "
    L"version 3 or any later version.\n"
    L"\n"
    L"No copyright is claimed over the original game. That Commodore 64 game "
    L"is copyright 1984 Datasoft, Inc. It was made by Ron J. Fortier, with "
    L"graphics by Kelly Day and music by John A. Fitzpatrick.\n"
    L"\n"
    L"This Windows port was made as a homage to that game.";

HWND gSettings = nullptr;
HWND gChkLives = nullptr;
HWND gChkEnemies = nullptr;
HWND gRoomEdit = nullptr;
HWND gChkTitle = nullptr;
HWND gChkFix = nullptr;
HWND gChkFull = nullptr;
HWND gChkCrt = nullptr;
HWND gChkCurve = nullptr;
HWND gChkScanlines = nullptr;
HWND gChkBlur = nullptr;
HWND gChkVignette = nullptr;
HWND gChkSound = nullptr;
HWND gPadLabel = nullptr;
HWND gPadCombo = nullptr;
HWND gPad2Label = nullptr;
HWND gPad2Combo = nullptr;
HWND gResetTop = nullptr;
int gResetTopW = 0;
int gResetTopH = 0;
std::vector<GamepadChoice> gPadItems;
// Nested CRT rows sit under Show CRT Filter, and only while that box is on.
int gCrtNestTop = 0;
int gCrtNestX = 0;
int gCrtNestW = 0;
int gCrtRowH = 0;
int gSoundX = 0;
int gSoundW = 0;
int gSettingsPad = 0;
int gSettingsGroup = 0;
int gGamepadLabelW = 0;
int gGamepadGap = 0;
int gGamepadComboW = 0;
int gGamepadDropH = 0;
int gGamepadVisualH = 0;
int gGamepadMoveH = 0;
HFONT gUiFont = nullptr;
HFONT gGearFont = nullptr;
HWND gTip = nullptr;
bool gRefreshing = false;
bool gRelayout = false;
HBITMAP gAboutBitmap = nullptr;
int gBuiltFont = 0;
bool gBuiltDark = false;
bool gSettingsReady = false;
// Where Settings last sat, in screen coordinates. Kept for this session only.
bool gSettingsPlaced = false;
int gSettingsX = 0;
int gSettingsY = 0;

struct Theme {
    bool dark = false;
    bool highContrast = false;
    COLORREF window = RGB(255, 255, 255);
    COLORREF text = RGB(0, 0, 0);
    COLORREF muted = RGB(96, 96, 96);
    COLORREF editBg = RGB(255, 255, 255);
    COLORREF editText = RGB(0, 0, 0);
    COLORREF boxBorder = RGB(100, 100, 100);
    COLORREF boxFill = RGB(255, 255, 255);
    COLORREF boxChecked = RGB(0, 120, 212);
    COLORREF checkMark = RGB(255, 255, 255);
    COLORREF buttonBg = RGB(225, 225, 225);
    COLORREF buttonText = RGB(0, 0, 0);
    HBRUSH windowBrush = nullptr;
    HBRUSH editBrush = nullptr;
    HBRUSH buttonBrush = nullptr;
};

Theme gTheme;

void ClearThemeBrushes() {
    if (gTheme.windowBrush) {
        DeleteObject(gTheme.windowBrush);
        gTheme.windowBrush = nullptr;
    }
    if (gTheme.editBrush) {
        DeleteObject(gTheme.editBrush);
        gTheme.editBrush = nullptr;
    }
    if (gTheme.buttonBrush) {
        DeleteObject(gTheme.buttonBrush);
        gTheme.buttonBrush = nullptr;
    }
}

void EnsureUiClasses() {
    static bool ready = false;
    if (ready) {
        return;
    }
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_UPDOWN_CLASS | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);
    ready = true;
}

LOGFONTW MessageFont() {
    NONCLIENTMETRICS metrics{};
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
        return metrics.lfMessageFont;
    }
    LOGFONTW font{};
    font.lfHeight = -15;
    font.lfWeight = FW_NORMAL;
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfQuality = CLEARTYPE_QUALITY;
    lstrcpyW(font.lfFaceName, L"Segoe UI");
    return font;
}

HFONT UiFont() {
    const LOGFONTW want = MessageFont();
    if (gUiFont) {
        LOGFONTW have{};
        if (GetObjectW(gUiFont, sizeof(have), &have) == sizeof(have) &&
            have.lfHeight == want.lfHeight && have.lfWeight == want.lfWeight &&
            lstrcmpW(have.lfFaceName, want.lfFaceName) == 0) {
            return gUiFont;
        }
        DeleteObject(gUiFont);
        gUiFont = nullptr;
    }
    gUiFont = CreateFontIndirectW(&want);
    if (!gUiFont) {
        gUiFont = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }
    return gUiFont;
}

int FontHeight() {
    HDC dc = GetDC(nullptr);
    const HFONT old = static_cast<HFONT>(SelectObject(dc, UiFont()));
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, old);
    ReleaseDC(nullptr, dc);
    return tm.tmHeight > 0 ? tm.tmHeight : 16;
}

COLORREF AccentColor() {
    DWORD accent = 0x00D47800;
    DWORD size = sizeof(accent);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", L"AccentColor",
                     RRF_RT_REG_DWORD, nullptr, &accent, &size) == ERROR_SUCCESS) {
        accent &= 0x00FFFFFF;
    }
    return static_cast<COLORREF>(accent);
}

COLORREF MarkOn(COLORREF fill) {
    const int lum = GetRValue(fill) * 30 + GetGValue(fill) * 59 + GetBValue(fill) * 11;
    return lum > 14000 ? RGB(0, 0, 0) : RGB(255, 255, 255);
}

bool AppsUseDarkMode() {
    DWORD light = 1;
    DWORD size = sizeof(light);
    if (RegGetValueW(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size) != ERROR_SUCCESS) {
        return false;
    }
    return light == 0;
}

void RebuildTheme() {
    HIGHCONTRASTW contrast{};
    contrast.cbSize = sizeof(contrast);
    SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
    const bool high = (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
    const bool dark = !high && AppsUseDarkMode();
    ClearThemeBrushes();
    gTheme.dark = dark;
    gTheme.highContrast = high;
    if (high) {
        gTheme.window = GetSysColor(COLOR_WINDOW);
        gTheme.text = GetSysColor(COLOR_WINDOWTEXT);
        gTheme.muted = gTheme.text;
        gTheme.editBg = gTheme.window;
        gTheme.editText = gTheme.text;
        gTheme.boxBorder = gTheme.text;
        gTheme.boxFill = gTheme.window;
        gTheme.boxChecked = GetSysColor(COLOR_HIGHLIGHT);
        gTheme.checkMark = GetSysColor(COLOR_HIGHLIGHTTEXT);
        gTheme.buttonBg = GetSysColor(COLOR_BTNFACE);
        gTheme.buttonText = GetSysColor(COLOR_BTNTEXT);
    } else if (dark) {
        gTheme.window = RGB(32, 32, 32);
        gTheme.text = RGB(255, 255, 255);
        gTheme.muted = RGB(196, 196, 196);
        gTheme.editBg = RGB(45, 45, 45);
        gTheme.editText = RGB(255, 255, 255);
        gTheme.boxBorder = RGB(166, 166, 166);
        gTheme.boxFill = RGB(32, 32, 32);
        gTheme.boxChecked = AccentColor();
        gTheme.checkMark = MarkOn(gTheme.boxChecked);
        gTheme.buttonBg = RGB(58, 58, 58);
        gTheme.buttonText = RGB(255, 255, 255);
    } else {
        gTheme.window = GetSysColor(COLOR_WINDOW);
        gTheme.text = GetSysColor(COLOR_WINDOWTEXT);
        gTheme.muted = RGB(80, 80, 80);
        gTheme.editBg = RGB(255, 255, 255);
        gTheme.editText = gTheme.text;
        gTheme.boxBorder = RGB(110, 110, 110);
        gTheme.boxFill = RGB(255, 255, 255);
        gTheme.boxChecked = AccentColor();
        gTheme.checkMark = MarkOn(gTheme.boxChecked);
        gTheme.buttonBg = GetSysColor(COLOR_BTNFACE);
        gTheme.buttonText = GetSysColor(COLOR_BTNTEXT);
    }
    gTheme.windowBrush = CreateSolidBrush(gTheme.window);
    gTheme.editBrush = CreateSolidBrush(gTheme.editBg);
    gTheme.buttonBrush = CreateSolidBrush(gTheme.buttonBg);
}

void ApplyFrameTheme(HWND hwnd) {
    const BOOL dark = gTheme.dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
    DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
}

void UseDarkAppMode(bool allow) {
    HMODULE themeLib = GetModuleHandleW(L"uxtheme.dll");
    if (!themeLib) {
        themeLib = LoadLibraryW(L"uxtheme.dll");
    }
    if (!themeLib) {
        return;
    }
    using SetMode = int(WINAPI*)(int);
    auto setMode = reinterpret_cast<SetMode>(GetProcAddress(themeLib, MAKEINTRESOURCEA(135)));
    auto refresh = reinterpret_cast<void(WINAPI*)()>(GetProcAddress(themeLib, MAKEINTRESOURCEA(104)));
    auto flush = reinterpret_cast<void(WINAPI*)()>(GetProcAddress(themeLib, MAKEINTRESOURCEA(136)));
    if (setMode) {
        // AllowDark follows the Windows app theme. Default leaves high contrast alone.
        setMode(allow ? 1 : 0);
    }
    if (refresh) {
        refresh();
    }
    if (flush) {
        flush();
    }
}

void AllowDarkWindow(HWND hwnd, bool allow) {
    HMODULE themeLib = GetModuleHandleW(L"uxtheme.dll");
    if (!themeLib || !hwnd) {
        return;
    }
    using Allow = BOOL(WINAPI*)(HWND, BOOL);
    auto allowWindow = reinterpret_cast<Allow>(GetProcAddress(themeLib, MAKEINTRESOURCEA(133)));
    if (allowWindow) {
        allowWindow(hwnd, allow ? TRUE : FALSE);
    }
}

void DrawFocusRing(HDC dc, const RECT& rc) {
    if (rc.right <= rc.left + 2 || rc.bottom <= rc.top + 2) {
        return;
    }
    const COLORREF color = gTheme.highContrast ? gTheme.text
                                                : (gTheme.dark ? RGB(230, 230, 230) : RGB(0, 0, 0));
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    const HGDIOBJ oldPen = SelectObject(dc, pen);
    const HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    // Right and bottom are exclusive, so every edge stays inside the control.
    Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

int TextWidth(HDC dc, const wchar_t* text) {
    SIZE size{};
    GetTextExtentPoint32W(dc, text, lstrlenW(text), &size);
    return size.cx;
}

int WrapHeight(HDC dc, const wchar_t* text, int width) {
    RECT rc{0, 0, width, 0};
    DrawTextW(dc, text, -1, &rc, DT_CALCRECT | DT_WORDBREAK | DT_LEFT | DT_NOPREFIX);
    return rc.bottom > 0 ? rc.bottom : FontHeight();
}

void DrawTick(HDC dc, const RECT& box, COLORREF color) {
    const int w = box.right - box.left;
    const int h = box.bottom - box.top;
    const int thick = w >= 22 ? 3 : 2;
    HPEN pen = CreatePen(PS_SOLID, thick, color);
    const HGDIOBJ old = SelectObject(dc, pen);
    const int x0 = box.left + w * 22 / 100;
    const int y0 = box.top + h * 52 / 100;
    const int x1 = box.left + w * 42 / 100;
    const int y1 = box.top + h * 72 / 100;
    const int x2 = box.left + w * 78 / 100;
    const int y2 = box.top + h * 30 / 100;
    MoveToEx(dc, x0, y0, nullptr);
    LineTo(dc, x1, y1);
    LineTo(dc, x2, y2);
    SelectObject(dc, old);
    DeleteObject(pen);
}

bool CheckValue(int id);

void DrawChoiceCaption(HDC dc, const RECT& rc, const RECT& boxRc, int box, HWND item, UINT itemState) {
    wchar_t label[128]{};
    const int count = GetWindowTextW(item, label, 128);
    RECT text = rc;
    text.left = boxRc.right + (box >= 18 ? 10 : 8);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, gTheme.text);
    const HFONT oldFont = static_cast<HFONT>(SelectObject(dc, UiFont()));
    DrawTextW(dc, label, count, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if ((itemState & ODS_FOCUS) != 0) {
        SIZE size{};
        GetTextExtentPoint32W(dc, label, count, &size);
        int right = text.left + static_cast<int>(size.cx) + 6;
        if (right > rc.right - 1) {
            right = rc.right - 1;
        }
        RECT focus{rc.left + 1, rc.top + 1, right, rc.bottom - 1};
        DrawFocusRing(dc, focus);
    }
    SelectObject(dc, oldFont);
}

int ChoiceBox(const RECT& rc, RECT& boxRc) {
    const int fontH = FontHeight();
    int box = fontH;
    if (box < 14) {
        box = 14;
    }
    const int limit = rc.bottom - rc.top - 4;
    if (box > limit && limit > 8) {
        box = limit;
    }
    const int top = rc.top + (rc.bottom - rc.top - box) / 2;
    boxRc = RECT{rc.left + kFocusPad, top, rc.left + kFocusPad + box, top + box};
    return box;
}

void PaintCheck(const DRAWITEMSTRUCT* draw) {
    RECT rc = draw->rcItem;
    FillRect(draw->hDC, &rc, gTheme.windowBrush);
    RECT boxRc{};
    const int box = ChoiceBox(rc, boxRc);
    const bool on = CheckValue(static_cast<int>(GetDlgCtrlID(draw->hwndItem)));
    HBRUSH fill = CreateSolidBrush(on ? gTheme.boxChecked : gTheme.boxFill);
    HPEN pen = CreatePen(PS_SOLID, 1, on ? gTheme.boxChecked : gTheme.boxBorder);
    const HGDIOBJ oldBrush = SelectObject(draw->hDC, fill);
    const HGDIOBJ oldPen = SelectObject(draw->hDC, pen);
    const int radius = box >= 16 ? 4 : 3;
    RoundRect(draw->hDC, boxRc.left, boxRc.top, boxRc.right, boxRc.bottom, radius, radius);
    SelectObject(draw->hDC, oldBrush);
    SelectObject(draw->hDC, oldPen);
    DeleteObject(fill);
    DeleteObject(pen);
    if (on) {
        DrawTick(draw->hDC, boxRc, gTheme.checkMark);
    }
    DrawChoiceCaption(draw->hDC, rc, boxRc, box, draw->hwndItem, draw->itemState);
}

void PaintRadio(const DRAWITEMSTRUCT* draw) {
    RECT rc = draw->rcItem;
    FillRect(draw->hDC, &rc, gTheme.windowBrush);
    RECT boxRc{};
    const int box = ChoiceBox(rc, boxRc);
    const int id = static_cast<int>(GetDlgCtrlID(draw->hwndItem));
    const bool on = id == IDC_WIZ_MODERN ? gWizardModern : !gWizardModern;
    HBRUSH fill = CreateSolidBrush(on ? gTheme.boxChecked : gTheme.boxFill);
    HPEN pen = CreatePen(PS_SOLID, 1, on ? gTheme.boxChecked : gTheme.boxBorder);
    const HGDIOBJ oldBrush = SelectObject(draw->hDC, fill);
    const HGDIOBJ oldPen = SelectObject(draw->hDC, pen);
    Ellipse(draw->hDC, boxRc.left, boxRc.top, boxRc.right, boxRc.bottom);
    SelectObject(draw->hDC, oldBrush);
    SelectObject(draw->hDC, oldPen);
    DeleteObject(fill);
    DeleteObject(pen);
    if (on) {
        const int inset = box >= 16 ? 4 : 3;
        HBRUSH mark = CreateSolidBrush(gTheme.checkMark);
        const HGDIOBJ oldMark = SelectObject(draw->hDC, mark);
        const HGDIOBJ oldDot = SelectObject(draw->hDC, GetStockObject(NULL_PEN));
        Ellipse(draw->hDC, boxRc.left + inset, boxRc.top + inset, boxRc.right - inset,
                boxRc.bottom - inset);
        SelectObject(draw->hDC, oldDot);
        SelectObject(draw->hDC, oldMark);
        DeleteObject(mark);
    }
    DrawChoiceCaption(draw->hDC, rc, boxRc, box, draw->hwndItem, draw->itemState);
}

void PaintPush(const DRAWITEMSTRUCT* draw) {
    RECT rc = draw->rcItem;
    const bool down = (draw->itemState & ODS_SELECTED) != 0;
    HBRUSH fill = CreateSolidBrush(down ? gTheme.boxChecked : gTheme.buttonBg);
    HPEN pen = CreatePen(PS_SOLID, 1, gTheme.boxChecked);
    const HGDIOBJ oldBrush = SelectObject(draw->hDC, fill);
    const HGDIOBJ oldPen = SelectObject(draw->hDC, pen);
    RoundRect(draw->hDC, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
    SelectObject(draw->hDC, oldBrush);
    SelectObject(draw->hDC, oldPen);
    DeleteObject(fill);
    DeleteObject(pen);
    wchar_t label[64]{};
    const int count = GetWindowTextW(draw->hwndItem, label, 64);
    SetBkMode(draw->hDC, TRANSPARENT);
    SetTextColor(draw->hDC, down ? gTheme.checkMark : gTheme.buttonText);
    const HFONT oldFont = static_cast<HFONT>(SelectObject(draw->hDC, UiFont()));
    DrawTextW(draw->hDC, label, count, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(draw->hDC, oldFont);
    if ((draw->itemState & ODS_FOCUS) != 0) {
        RECT focus = rc;
        InflateRect(&focus, -3, -3);
        DrawFocusRing(draw->hDC, focus);
    }
}

LRESULT OnCtlColor(WPARAM wp, LPARAM lp, bool edit) {
    HDC dc = reinterpret_cast<HDC>(wp);
    SetBkMode(dc, TRANSPARENT);
    if (edit) {
        SetTextColor(dc, gTheme.editText);
        SetBkColor(dc, gTheme.editBg);
        return reinterpret_cast<LRESULT>(gTheme.editBrush);
    }
    const int id = GetDlgCtrlID(reinterpret_cast<HWND>(lp));
    SetTextColor(dc, (id == IDC_HINT || id == IDC_WIZ_NOTE) ? gTheme.muted : gTheme.text);
    SetBkColor(dc, gTheme.window);
    return reinterpret_cast<LRESULT>(gTheme.windowBrush);
}

void ThemeInput(HWND ctrl, bool edit) {
    if (gTheme.highContrast) {
        return;
    }
    SetWindowTheme(ctrl, gTheme.dark ? (edit ? L"DarkMode_CFD" : L"DarkMode_Explorer") : L"Explorer",
                   nullptr);
}

LRESULT CALLBACK CheckEraseProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_ERASEBKGND) {
        return 1;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

HFONT GearFont() {
    if (!gGearFont) {
        gGearFont = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
    }
    return gGearFont;
}

void PlaceOnOwner(HWND hwnd, HWND owner) {
    RECT wr{};
    GetWindowRect(hwnd, &wr);
    RECT ownerRc{};
    GetWindowRect(owner, &ownerRc);
    const int ww = wr.right - wr.left;
    const int wh = wr.bottom - wr.top;
    int x = ownerRc.left + ((ownerRc.right - ownerRc.left) - ww) / 2;
    int y = ownerRc.top + ((ownerRc.bottom - ownerRc.top) - wh) / 2;
    HMONITOR monitor = MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)) {
        if (x < info.rcWork.left) {
            x = info.rcWork.left;
        }
        if (y < info.rcWork.top) {
            y = info.rcWork.top;
        }
        if (x + ww > info.rcWork.right) {
            x = info.rcWork.right - ww;
        }
        if (y + wh > info.rcWork.bottom) {
            y = info.rcWork.bottom - wh;
        }
    }
    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void RememberSettingsPlace(HWND hwnd) {
    RECT rc{};
    if (!GetWindowRect(hwnd, &rc)) {
        return;
    }
    gSettingsX = rc.left;
    gSettingsY = rc.top;
    gSettingsPlaced = true;
}

void PlaceSettings(HWND hwnd, HWND owner) {
    if (gSettingsPlaced) {
        SetWindowPos(hwnd, nullptr, gSettingsX, gSettingsY, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    } else {
        PlaceOnOwner(hwnd, owner);
    }
    RememberSettingsPlace(hwnd);
}

bool CheckValue(int id) {
    switch (id) {
    case IDC_LIVES:
        return gConfig.infiniteLives;
    case IDC_ENEMIES:
        return gConfig.enemiesCantHitBruce;
    case IDC_TITLE:
        return gConfig.showTitleScreen;
    case IDC_FIX:
        return gConfig.fixBugsInOriginal;
    case IDC_FULL:
        return gConfig.fullScreen;
    case IDC_CRT:
        return gConfig.showCrtFilter;
    case IDC_CRT_CURVE:
        return gConfig.crtCurve;
    case IDC_CRT_SCANLINES:
        return gConfig.crtScanlines;
    case IDC_CRT_BLUR:
        return gConfig.crtBlur;
    case IDC_CRT_VIGNETTE:
        return gConfig.crtVignette;
    case IDC_SOUND:
        return gConfig.sound;
    default:
        return false;
    }
}

void InvalidateCheck(HWND ctrl) {
    if (ctrl) {
        InvalidateRect(ctrl, nullptr, FALSE);
    }
}

void SetRoomText() {
    wchar_t buf[8]{};
    swprintf_s(buf, L"%d", ClampedStartRoom());
    SetWindowTextW(gRoomEdit, buf);
}

int gSettingsClientW = 0;
int gSettingsClientH = 0;

HWND MakeCheck(HWND parent, HINSTANCE inst, int id, const wchar_t* label, int x, int y, int w, int h) {
    HWND ctrl = CreateWindowExW(0, L"BUTTON", label,
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                inst, nullptr);
    SetWindowSubclass(ctrl, CheckEraseProc, 1, 0);
    return ctrl;
}

void DestroySettingsChildren(HWND hwnd) {
    gRefreshing = true;
    while (HWND child = FindWindowExW(hwnd, nullptr, nullptr, nullptr)) {
        DestroyWindow(child);
    }
    gChkLives = nullptr;
    gChkEnemies = nullptr;
    gRoomEdit = nullptr;
    gChkTitle = nullptr;
    gChkFix = nullptr;
    gChkFull = nullptr;
    gChkCrt = nullptr;
    gChkCurve = nullptr;
    gChkScanlines = nullptr;
    gChkBlur = nullptr;
    gChkVignette = nullptr;
    gChkSound = nullptr;
    gPadLabel = nullptr;
    gPadCombo = nullptr;
    gPad2Label = nullptr;
    gPad2Combo = nullptr;
    gResetTop = nullptr;
    gPadItems.clear();
    gRefreshing = false;
}

void FitSettingsWindow(HWND hwnd);
void LayoutCrtOptions(HWND settings);

void BuildSettings(HWND hwnd) {
    const HINSTANCE inst = GetModuleHandleW(nullptr);
    HDC dc = GetDC(hwnd);
    const HFONT old = static_cast<HFONT>(SelectObject(dc, UiFont()));
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    const int fontH = tm.tmHeight > 0 ? tm.tmHeight : 16;
    const int pad = fontH;
    const int rowH = fontH + fontH / 2 + 4;
    const int group = fontH / 2 + 4;
    int box = fontH;
    if (box < 14) {
        box = 14;
    }
    const int afterBox = kFocusPad + box + (box >= 18 ? 10 : 8);

    const wchar_t* checks[] = {
        L"Infinite Lives",
        L"Enemies cannot hit Bruce",
        L"Show Title Screen at Start",
        L"Fix Bugs in Original",
        L"Enable Full Screen",
        L"Show CRT Filter",
        L"Enable Sound",
    };
    int labelW = 0;
    for (const wchar_t* label : checks) {
        const int width = TextWidth(dc, label);
        if (width > labelW) {
            labelW = width;
        }
    }
    const int roomLabelW = TextWidth(dc, L"Start Room");
    const int editW = TextWidth(dc, L"00") + GetSystemMetrics(SM_CXVSCROLL) + fontH;
    const wchar_t* hintText =
        L"Start room applies on Game Over or Restart. The title screen applies at startup and on Game Over.";
    const int hintNatural = TextWidth(dc, hintText);
    const int em = TextWidth(dc, L"M");
    int inner = afterBox + labelW;
    const wchar_t* crtParts[] = {L"Curve", L"Scanlines", L"Blur", L"Vignette"};
    for (const wchar_t* label : crtParts) {
        const int width = fontH + afterBox + TextWidth(dc, label);
        if (width > inner) {
            inner = width;
        }
    }
    const int roomRow = kFocusPad + roomLabelW + fontH / 2 + editW;
    if (roomRow > inner) {
        inner = roomRow;
    }
    // One line when it fits. A long translation of the note wraps to two.
    int hintBudget = em * 48;
    if (hintBudget < hintNatural / 2 + fontH * 4) {
        hintBudget = hintNatural / 2 + fontH * 4;
    }
    const int hintWant = hintNatural <= hintBudget ? hintNatural : hintBudget;
    if (afterBox + hintWant > inner) {
        inner = afterBox + hintWant;
    }
    const GamepadList gamepads = SyncGamepads();
    int gamepadLabelW = TextWidth(dc, L"Player 1");
    const int player2LabelW = TextWidth(dc, L"Player 2");
    if (player2LabelW > gamepadLabelW) {
        gamepadLabelW = player2LabelW;
    }
    int gamepadNameW = TextWidth(dc, kNoGamepad);
    const int typicalPad = TextWidth(dc, L"Xbox Wireless Controller");
    if (typicalPad > gamepadNameW) {
        gamepadNameW = typicalPad;
    }
    for (const GamepadChoice& choice : gamepads.items) {
        const int width = TextWidth(dc, choice.name.c_str());
        if (width > gamepadNameW) {
            gamepadNameW = width;
        }
    }
    const int gamepadChrome = GetSystemMetrics(SM_CXVSCROLL) + fontH / 2;
    const int gamepadRow = kFocusPad + gamepadLabelW + fontH / 2 + gamepadNameW + gamepadChrome;
    if (gamepadRow > inner) {
        inner = gamepadRow;
    }
    int resetH = fontH + fontH / 2;
    if (resetH < 28) {
        resetH = 28;
    }
    int resetW = TextWidth(dc, L"Reset Top Score") + em * 2;
    if (resetW < em * 8) {
        resetW = em * 8;
    }
    if (kFocusPad + resetW > inner) {
        inner = kFocusPad + resetW;
    }
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (GetMonitorInfoW(monitor, &monitorInfo)) {
        int maxInner = (monitorInfo.rcWork.right - monitorInfo.rcWork.left) - pad * 2 -
                       GetSystemMetrics(SM_CXFRAME) * 2 - 8;
        if (maxInner < afterBox + labelW) {
            maxInner = afterBox + labelW;
        }
        if (inner > maxInner) {
            inner = maxInner;
        }
    }
    if (resetW > inner - kFocusPad) {
        resetW = inner - kFocusPad;
    }
    if (resetW < 48) {
        resetW = 48;
    }
    gResetTopW = resetW;
    gResetTopH = resetH;
    const int hintW = inner - afterBox;
    const int hintH = WrapHeight(dc, hintText, hintW > 20 ? hintW : 20);
    SelectObject(dc, old);
    ReleaseDC(hwnd, dc);

    gSettingsGroup = group;
    gGamepadLabelW = gamepadLabelW;
    gGamepadGap = fontH / 2;
    gGamepadComboW = inner - kFocusPad - gGamepadLabelW - gGamepadGap;
    if (gGamepadComboW < 48) {
        gGamepadComboW = 48;
    }
    gGamepadDropH = rowH * 8;
    if (gGamepadDropH < 160) {
        gGamepadDropH = 160;
    }

    const int editH = fontH + fontH / 2;
    int y = pad;
    const int x = pad;
    gChkLives = MakeCheck(hwnd, inst, IDC_LIVES, checks[0], x, y, inner, rowH);
    y += rowH;
    gChkEnemies = MakeCheck(hwnd, inst, IDC_ENEMIES, checks[1], x, y, inner, rowH);
    y += rowH + group;

    const int labelY = y + (rowH - fontH) / 2;
    HWND roomLabel = CreateWindowExW(0, L"STATIC", L"Start Room", WS_CHILD | WS_VISIBLE,
                                     x + kFocusPad, labelY, roomLabelW + 4, fontH, hwnd,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ROOM_LABEL)),
                                     inst, nullptr);
    SendMessageW(roomLabel, WM_SETFONT, reinterpret_cast<WPARAM>(UiFont()), FALSE);
    const int editY = y + (rowH - editH) / 2;
    gRoomEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL,
                                x + kFocusPad + roomLabelW + fontH / 2, editY, editW, editH, hwnd,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ROOM)), inst, nullptr);
    SendMessageW(gRoomEdit, WM_SETFONT, reinterpret_cast<WPARAM>(UiFont()), FALSE);
    ThemeInput(gRoomEdit, true);
    HWND spin = CreateWindowExW(0, UPDOWN_CLASSW, nullptr,
                                WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                                    UDS_ARROWKEYS | UDS_NOTHOUSANDS,
                                0, 0, 0, 0, hwnd, nullptr, inst, nullptr);
    SendMessageW(spin, UDM_SETBUDDY, reinterpret_cast<WPARAM>(gRoomEdit), 0);
    SendMessageW(spin, UDM_SETRANGE32, 0, kRoomCount - 1);
    ThemeInput(spin, false);
    y += rowH;

    gChkTitle = MakeCheck(hwnd, inst, IDC_TITLE, checks[2], x, y, inner, rowH);
    y += rowH;
    HWND hint = CreateWindowExW(0, L"STATIC", hintText, WS_CHILD | WS_VISIBLE,
                                x + afterBox, y, hintW, hintH + 2, hwnd,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_HINT)), inst, nullptr);
    SendMessageW(hint, WM_SETFONT, reinterpret_cast<WPARAM>(UiFont()), FALSE);
    y += hintH + 2 + group;

    gChkFix = MakeCheck(hwnd, inst, IDC_FIX, checks[3], x, y, inner, rowH);
    y += rowH;
    gChkFull = MakeCheck(hwnd, inst, IDC_FULL, checks[4], x, y, inner, rowH);
    y += rowH;
    gChkCrt = MakeCheck(hwnd, inst, IDC_CRT, checks[5], x, y, inner, rowH);
    y += rowH;
    // The part boxes line up under the CRT label, one step in from that box.
    gCrtRowH = rowH;
    gCrtNestX = x + fontH;
    gCrtNestW = inner - fontH;
    if (gCrtNestW < afterBox) {
        gCrtNestW = inner;
        gCrtNestX = x;
    }
    gCrtNestTop = y;
    gSoundX = x;
    gSoundW = inner;
    gSettingsPad = pad;
    gChkCurve = MakeCheck(hwnd, inst, IDC_CRT_CURVE, crtParts[0], gCrtNestX, y, gCrtNestW, rowH);
    y += rowH;
    gChkScanlines = MakeCheck(hwnd, inst, IDC_CRT_SCANLINES, crtParts[1], gCrtNestX, y, gCrtNestW, rowH);
    y += rowH;
    gChkBlur = MakeCheck(hwnd, inst, IDC_CRT_BLUR, crtParts[2], gCrtNestX, y, gCrtNestW, rowH);
    y += rowH;
    gChkVignette = MakeCheck(hwnd, inst, IDC_CRT_VIGNETTE, crtParts[3], gCrtNestX, y, gCrtNestW, rowH);
    y += rowH;
    gChkSound = MakeCheck(hwnd, inst, IDC_SOUND, checks[6], x, y, inner, rowH);
    y += rowH + gSettingsGroup;
    gPadLabel = CreateWindowExW(0, L"STATIC", L"Player 1", WS_CHILD | WS_VISIBLE,
                                x + kFocusPad, y, gGamepadLabelW + 4, fontH, hwnd,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_GAMEPAD_LABEL)),
                                inst, nullptr);
    SendMessageW(gPadLabel, WM_SETFONT, reinterpret_cast<WPARAM>(UiFont()), FALSE);
    gPadCombo = CreateWindowExW(
        0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_CLIPSIBLINGS | CBS_DROPDOWNLIST |
            CBS_HASSTRINGS | CBS_NOINTEGRALHEIGHT,
        x + kFocusPad + gGamepadLabelW + gGamepadGap, y, gGamepadComboW, gGamepadDropH, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_GAMEPAD)), inst, nullptr);
    if (gPadCombo) {
        SendMessageW(gPadCombo, WM_SETFONT, reinterpret_cast<WPARAM>(UiFont()), FALSE);
        SendMessageW(gPadCombo, CB_SETMINVISIBLE, 8, 0);
        ThemeInput(gPadCombo, true);
        const int itemH =
            static_cast<int>(SendMessageW(gPadCombo, CB_GETITEMHEIGHT, static_cast<WPARAM>(-1), 0));
        int visual = itemH + GetSystemMetrics(SM_CYEDGE) * 2;
        if (visual < fontH) {
            visual = fontH + 8;
        }
        RECT probe{};
        GetWindowRect(gPadCombo, &probe);
        const int createdH = probe.bottom - probe.top;
        // comctl v6 keeps the closed control at one line and opens the list
        // in its own window. A tall child would cover the padding under it.
        if (createdH > 0 && createdH < gGamepadDropH / 2) {
            gGamepadVisualH = createdH;
            gGamepadMoveH = createdH;
        } else {
            gGamepadVisualH = visual;
            gGamepadMoveH = gGamepadDropH;
        }
    }
    gPad2Label = CreateWindowExW(0, L"STATIC", L"Player 2", WS_CHILD | WS_VISIBLE,
                                 x + kFocusPad, y, gGamepadLabelW + 4, fontH, hwnd,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_GAMEPAD2_LABEL)),
                                 inst, nullptr);
    SendMessageW(gPad2Label, WM_SETFONT, reinterpret_cast<WPARAM>(UiFont()), FALSE);
    gPad2Combo = CreateWindowExW(
        0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_CLIPSIBLINGS | CBS_DROPDOWNLIST |
            CBS_HASSTRINGS | CBS_NOINTEGRALHEIGHT,
        x + kFocusPad + gGamepadLabelW + gGamepadGap, y, gGamepadComboW, gGamepadDropH, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_GAMEPAD2)), inst, nullptr);
    if (gPad2Combo) {
        SendMessageW(gPad2Combo, WM_SETFONT, reinterpret_cast<WPARAM>(UiFont()), FALSE);
        SendMessageW(gPad2Combo, CB_SETMINVISIBLE, 8, 0);
        ThemeInput(gPad2Combo, true);
    }
    gResetTop = CreateWindowExW(0, L"BUTTON", L"Reset Top Score",
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                x + kFocusPad, y, gResetTopW, gResetTopH, hwnd,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_RESET_TOP)),
                                inst, nullptr);
    SetWindowSubclass(gResetTop, CheckEraseProc, 1, 0);
    y += rowH + pad;

    gSettingsClientW = pad + inner + pad;
    gSettingsClientH = y;
    gBuiltFont = fontH;
    gBuiltDark = gTheme.dark;
    LayoutCrtOptions(hwnd);
}

void FitSettingsWindow(HWND hwnd) {
    RECT rc{0, 0, gSettingsClientW, gSettingsClientH};
    AdjustWindowRectEx(&rc, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_TOOLWINDOW);
    SetWindowPos(hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// The four parts sit under Show CRT Filter while that box is on. Off, Sound
// moves up and the parts leave the tab order. The two gamepad rows stay under
// Sound, and Reset Top Score stays under them.
void LayoutCrtOptions(HWND settings) {
    if (!gChkSound) {
        return;
    }
    const bool show = gConfig.showCrtFilter;
    HWND parts[] = {gChkCurve, gChkScanlines, gChkBlur, gChkVignette};
    int y = gCrtNestTop;
    for (HWND ctrl : parts) {
        if (!ctrl) {
            continue;
        }
        if (show) {
            SetWindowPos(ctrl, nullptr, gCrtNestX, y, gCrtNestW, gCrtRowH,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            ShowWindow(ctrl, SW_SHOW);
            EnableWindow(ctrl, TRUE);
            y += gCrtRowH;
        } else {
            ShowWindow(ctrl, SW_HIDE);
            EnableWindow(ctrl, FALSE);
        }
    }
    SetWindowPos(gChkSound, nullptr, gSoundX, y, gSoundW, gCrtRowH,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    y += gCrtRowH;
    HWND padLabels[] = {gPadLabel, gPad2Label};
    HWND padCombos[] = {gPadCombo, gPad2Combo};
    for (int row = 0; row < 2; ++row) {
        if (!padLabels[row] || !padCombos[row]) {
            continue;
        }
        y += gSettingsGroup;
        const int fontH = gBuiltFont > 0 ? gBuiltFont : 16;
        const int labelY = y + (gCrtRowH - fontH) / 2;
        SetWindowPos(padLabels[row], nullptr, gSoundX + kFocusPad, labelY, gGamepadLabelW + 4, fontH,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        const int visual = gGamepadVisualH > 0 ? gGamepadVisualH : fontH + 8;
        int comboY = y + (gCrtRowH - visual) / 2;
        if (comboY < y) {
            comboY = y;
        }
        const int moveH = gGamepadMoveH > 0 ? gGamepadMoveH : visual;
        SetWindowPos(padCombos[row], nullptr, gSoundX + kFocusPad + gGamepadLabelW + gGamepadGap, comboY,
                     gGamepadComboW, moveH, SWP_NOZORDER | SWP_NOACTIVATE);
        int used = gCrtRowH;
        // A closed list taller than the row would cover the control under it.
        if (moveH > used && moveH < gGamepadDropH / 2) {
            used = moveH;
        }
        y += used;
    }
    if (gResetTop) {
        y += gSettingsGroup;
        SetWindowPos(gResetTop, nullptr, gSoundX + kFocusPad, y, gResetTopW, gResetTopH,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        y += gResetTopH;
    }
    y += gSettingsPad;
    gSettingsClientH = y;
    if (settings) {
        FitSettingsWindow(settings);
        InvalidateRect(settings, nullptr, TRUE);
    }
}

void ApplyRoomEdit() {
    wchar_t buf[16]{};
    GetWindowTextW(gRoomEdit, buf, 16);
    if (buf[0] == L'\0') {
        return;
    }
    wchar_t* end = nullptr;
    const long value = wcstol(buf, &end, 10);
    if (end == buf || *end != L'\0') {
        return;
    }
    gConfig.startRoom = static_cast<int>(value);
}

constexpr wchar_t kPadNone[] = L"None";

bool ComboOpen(HWND combo) {
    return combo && SendMessageW(combo, CB_GETDROPPEDSTATE, 0, 0) != 0;
}

// The row under the pointer while the list is open. CB_GETCURSEL stays on
// the controller that was current when the list opened, until it closes.
int ComboHighlight(HWND combo) {
    COMBOBOXINFO info{};
    info.cbSize = sizeof(info);
    if (combo && GetComboBoxInfo(combo, &info) && info.hwndList) {
        const LRESULT cur = SendMessageW(info.hwndList, LB_GETCURSEL, 0, 0);
        if (cur != LB_ERR) {
            return static_cast<int>(cur);
        }
    }
    if (!combo) {
        return -1;
    }
    return static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
}

int PadSelection(int active, int count) {
    if (count <= 0) {
        return 0;
    }
    if (active < 0 || active >= count) {
        return 0;
    }
    return active + 1;
}

bool OnePadComboShows(HWND combo, int active, int count) {
    if (!combo) {
        return false;
    }
    const bool wantEnabled = count > 0;
    if ((IsWindowEnabled(combo) != FALSE) != wantEnabled) {
        return false;
    }
    if (!wantEnabled) {
        return true;
    }
    return static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0)) == PadSelection(active, count);
}

bool GamepadCombosShow(const GamepadList& list) {
    if (list.items.size() != gPadItems.size()) {
        return false;
    }
    for (size_t i = 0; i < list.items.size(); ++i) {
        if (list.items[i].id != gPadItems[i].id || list.items[i].name != gPadItems[i].name) {
            return false;
        }
    }
    const int count = static_cast<int>(list.items.size());
    return OnePadComboShows(gPadCombo, list.active, count) &&
           OnePadComboShows(gPad2Combo, list.active2, count);
}

void FillPadCombo(HWND combo, int active) {
    if (!combo) {
        return;
    }
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    if (gPadItems.empty()) {
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kNoGamepad));
        SendMessageW(combo, CB_SETCURSEL, 0, 0);
        EnableWindow(combo, FALSE);
        return;
    }
    EnableWindow(combo, TRUE);
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kPadNone));
    HDC dc = GetDC(combo);
    const HFONT old = static_cast<HFONT>(SelectObject(dc, UiFont()));
    int widest = TextWidth(dc, kPadNone);
    for (const GamepadChoice& choice : gPadItems) {
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(choice.name.c_str()));
        const int width = TextWidth(dc, choice.name.c_str());
        if (width > widest) {
            widest = width;
        }
    }
    SelectObject(dc, old);
    ReleaseDC(combo, dc);
    SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(PadSelection(active, static_cast<int>(gPadItems.size()))),
                 0);
    int drop = widest + GetSystemMetrics(SM_CXVSCROLL) + 12;
    RECT rc{};
    GetWindowRect(combo, &rc);
    const int closedW = rc.right - rc.left;
    if (drop < closedW) {
        drop = closedW;
    }
    SendMessageW(combo, CB_SETDROPPEDWIDTH, static_cast<WPARAM>(drop), 0);
}

void RefreshGamepadCombo() {
    if (!gPadCombo && !gPad2Combo) {
        return;
    }
    // The dropped list highlights the row under the pointer. Rebuilding it,
    // or CB_SETCURSEL, puts that highlight back on the closed selection.
    // A pad that arrives while a list is open is picked up once it closes.
    if (ComboOpen(gPadCombo) || ComboOpen(gPad2Combo)) {
        return;
    }
    const GamepadList list = SyncGamepads();
    if (GamepadCombosShow(list)) {
        return;
    }
    gRefreshing = true;
    gPadItems = list.items;
    FillPadCombo(gPadCombo, list.active);
    FillPadCombo(gPad2Combo, list.active2);
    gRefreshing = false;
}

void OnSettingCommand(HWND hwnd, int id, int code) {
    if (gRefreshing) {
        return;
    }
    if (code == EN_CHANGE && id == IDC_ROOM) {
        ApplyRoomEdit();
        return;
    }
    if (code == EN_KILLFOCUS && id == IDC_ROOM) {
        gConfig.startRoom = ClampedStartRoom();
        gRefreshing = true;
        SetRoomText();
        gRefreshing = false;
        return;
    }
    if (id == IDC_GAMEPAD || id == IDC_GAMEPAD2) {
        // While the list is open, CBN_SELCHANGE follows the pointer and
        // CB_GETCURSEL is still the controller the box had when it opened.
        // Writing that id and refreshing snaps the highlight to the first
        // row. The click, or Enter, is CBN_SELENDOK. Arrow keys on the
        // closed box send CBN_SELCHANGE with the list shut.
        HWND combo = id == IDC_GAMEPAD ? gPadCombo : gPad2Combo;
        const bool choose = code == CBN_SELENDOK || (code == CBN_SELCHANGE && !ComboOpen(combo));
        if (choose && !gPadItems.empty()) {
            const int sel = ComboOpen(combo) ? ComboHighlight(combo)
                                             : static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
            const int player = id == IDC_GAMEPAD ? 0 : 1;
            std::wstring chosen = kGamepadNone;
            if (sel > 0 && sel <= static_cast<int>(gPadItems.size())) {
                chosen = gPadItems[static_cast<size_t>(sel - 1)].id;
            }
            std::wstring& mine = player == 0 ? gConfig.gamepad : gConfig.gamepad2;
            std::wstring& other = player == 0 ? gConfig.gamepad2 : gConfig.gamepad;
            // The pad just chosen leaves the other player. He takes the first
            // remaining controller, or none when that was the only one.
            if (chosen != kGamepadNone && other == chosen) {
                std::wstring fallback = kGamepadNone;
                for (const GamepadChoice& choice : gPadItems) {
                    if (choice.id != chosen) {
                        fallback = choice.id;
                        break;
                    }
                }
                other = fallback;
            }
            mine = chosen;
            gRefreshing = true;
            const GamepadList list = SyncGamepads();
            gPadItems = list.items;
            FillPadCombo(gPadCombo, list.active);
            FillPadCombo(gPad2Combo, list.active2);
            gRefreshing = false;
        }
        return;
    }
    if (code != BN_CLICKED) {
        return;
    }
    HWND owner = GetParent(hwnd);
    switch (id) {
    case IDC_LIVES:
        gConfig.infiniteLives = !gConfig.infiniteLives;
        InvalidateCheck(gChkLives);
        break;
    case IDC_ENEMIES:
        gConfig.enemiesCantHitBruce = !gConfig.enemiesCantHitBruce;
        InvalidateCheck(gChkEnemies);
        break;
    case IDC_TITLE:
        gConfig.showTitleScreen = !gConfig.showTitleScreen;
        InvalidateCheck(gChkTitle);
        break;
    case IDC_FIX:
        gConfig.fixBugsInOriginal = !gConfig.fixBugsInOriginal;
        InvalidateCheck(gChkFix);
        break;
    case IDC_CRT:
        // Paint reads this on the next frame. The part rows follow the box.
        gConfig.showCrtFilter = !gConfig.showCrtFilter;
        InvalidateCheck(gChkCrt);
        LayoutCrtOptions(hwnd);
        break;
    case IDC_CRT_CURVE:
        gConfig.crtCurve = !gConfig.crtCurve;
        InvalidateCheck(gChkCurve);
        break;
    case IDC_CRT_SCANLINES:
        gConfig.crtScanlines = !gConfig.crtScanlines;
        InvalidateCheck(gChkScanlines);
        break;
    case IDC_CRT_BLUR:
        gConfig.crtBlur = !gConfig.crtBlur;
        InvalidateCheck(gChkBlur);
        break;
    case IDC_CRT_VIGNETTE:
        gConfig.crtVignette = !gConfig.crtVignette;
        InvalidateCheck(gChkVignette);
        break;
    case IDC_FULL:
        SendMessageW(owner, WM_APP_FULLSCREEN, gConfig.fullScreen ? 0 : 1, 0);
        break;
    case IDC_SOUND:
        SendMessageW(owner, WM_APP_SOUND, gConfig.sound ? 0 : 1, 0);
        break;
    case IDC_RESET_TOP:
        SendMessageW(owner, WM_APP_RESET_TOP, 0, 0);
        break;
    default:
        break;
    }
}

void RelayoutSettings(HWND hwnd) {
    if (gRelayout || !IsWindow(hwnd)) {
        return;
    }
    gRelayout = true;
    const int fontH = FontHeight();
    RebuildTheme();
    if (fontH != gBuiltFont || gTheme.dark != gBuiltDark) {
        DestroySettingsChildren(hwnd);
        BuildSettings(hwnd);
        FitSettingsWindow(hwnd);
        RefreshSettingsDialog();
    }
    ApplyFrameTheme(hwnd);
    InvalidateRect(hwnd, nullptr, TRUE);
    gRelayout = false;
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND: {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        FillRect(reinterpret_cast<HDC>(wp), &rc, gTheme.windowBrush);
        return 1;
    }
    case WM_CTLCOLORSTATIC:
        return OnCtlColor(wp, lp, false);
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        return OnCtlColor(wp, lp, true);
    case WM_DRAWITEM: {
        const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lp);
        if (static_cast<int>(GetDlgCtrlID(draw->hwndItem)) == IDC_RESET_TOP) {
            PaintPush(draw);
        } else {
            PaintCheck(draw);
        }
        return TRUE;
    }
    case WM_COMMAND:
        OnSettingCommand(hwnd, LOWORD(wp), HIWORD(wp));
        return 0;
    case WM_TIMER:
        if (wp == kGamepadTimer && gSettingsReady && hwnd == gSettings) {
            RefreshGamepadCombo();
        }
        return 0;
    case WM_DEVICECHANGE:
        // A pad can arrive a moment after the notice. The timer catches that.
        if (gSettingsReady && hwnd == gSettings) {
            RefreshGamepadCombo();
        }
        return TRUE;
    case WM_SETTINGCHANGE:
    case WM_SYSCOLORCHANGE:
    case WM_THEMECHANGED:
        if (gSettingsReady) {
            RelayoutSettings(hwnd);
        }
        return 0;
    case WM_MOVE:
        if (gSettingsReady && hwnd == gSettings) {
            RememberSettingsPlace(hwnd);
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, kGamepadTimer);
        if (hwnd == gSettings) {
            gSettings = nullptr;
            gSettingsReady = false;
        }
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

void RegisterSettingsClass() {
    static bool ready = false;
    if (ready) {
        return;
    }
    WNDCLASSW wc{};
    wc.lpfnWndProc = SettingsProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    wc.lpszClassName = L"BruceLeeSettings";
    RegisterClassW(&wc);
    ready = true;
}

LRESULT CALLBACK AboutProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND: {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        FillRect(reinterpret_cast<HDC>(wp), &rc, gTheme.windowBrush);
        return 1;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        return OnCtlColor(wp, lp, false);
    case WM_DRAWITEM:
        PaintPush(reinterpret_cast<const DRAWITEMSTRUCT*>(lp));
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_SETTINGCHANGE:
    case WM_SYSCOLORCHANGE:
    case WM_THEMECHANGED:
        RebuildTheme();
        ApplyFrameTheme(hwnd);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (gAboutBitmap) {
            DeleteObject(gAboutBitmap);
            gAboutBitmap = nullptr;
        }
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

void RegisterAboutClass() {
    static bool ready = false;
    if (ready) {
        return;
    }
    WNDCLASSW wc{};
    wc.lpfnWndProc = AboutProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    wc.lpszClassName = L"BruceLeeAbout";
    RegisterClassW(&wc);
    ready = true;
}

HBITMAP ScaleBitmap(HBITMAP source, int destW, int destH) {
    BITMAP bm{};
    if (!GetObjectW(source, sizeof(bm), &bm)) {
        return nullptr;
    }
    HDC screen = GetDC(nullptr);
    HDC srcDc = CreateCompatibleDC(screen);
    HDC dstDc = CreateCompatibleDC(screen);
    HBITMAP dest = CreateCompatibleBitmap(screen, destW, destH);
    HGDIOBJ oldSrc = SelectObject(srcDc, source);
    HGDIOBJ oldDst = SelectObject(dstDc, dest);
    SetStretchBltMode(dstDc, HALFTONE);
    SetBrushOrgEx(dstDc, 0, 0, nullptr);
    StretchBlt(dstDc, 0, 0, destW, destH, srcDc, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
    SelectObject(srcDc, oldSrc);
    SelectObject(dstDc, oldDst);
    DeleteDC(srcDc);
    DeleteDC(dstDc);
    ReleaseDC(nullptr, screen);
    return dest;
}

HBITMAP BitmapFromFile(const std::wstring& path) {
    const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool balance = co == S_OK || co == S_FALSE;
    HBITMAP raw = nullptr;
    IWICImagingFactory* factory = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&factory)))) {
        IWICBitmapDecoder* decoder = nullptr;
        if (SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                         WICDecodeMetadataCacheOnLoad, &decoder))) {
            IWICBitmapFrameDecode* frame = nullptr;
            if (SUCCEEDED(decoder->GetFrame(0, &frame))) {
                IWICFormatConverter* converter = nullptr;
                if (SUCCEEDED(factory->CreateFormatConverter(&converter))) {
                    if (SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppBGR,
                                                        WICBitmapDitherTypeNone, nullptr, 0.0,
                                                        WICBitmapPaletteTypeCustom))) {
                        UINT width = 0;
                        UINT height = 0;
                        converter->GetSize(&width, &height);
                        if (width > 0 && height > 0 && width < 10000 && height < 10000) {
                            BITMAPINFO info{};
                            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                            info.bmiHeader.biWidth = static_cast<LONG>(width);
                            info.bmiHeader.biHeight = -static_cast<LONG>(height);
                            info.bmiHeader.biPlanes = 1;
                            info.bmiHeader.biBitCount = 32;
                            info.bmiHeader.biCompression = BI_RGB;
                            void* bits = nullptr;
                            HDC screen = GetDC(nullptr);
                            raw = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
                            ReleaseDC(nullptr, screen);
                            if (raw && bits) {
                                const UINT stride = width * 4;
                                if (FAILED(converter->CopyPixels(nullptr, stride, stride * height,
                                                                static_cast<BYTE*>(bits)))) {
                                    DeleteObject(raw);
                                    raw = nullptr;
                                }
                            }
                        }
                    }
                    converter->Release();
                }
                frame->Release();
            }
            decoder->Release();
        }
        factory->Release();
    }
    if (balance) {
        CoUninitialize();
    }
    return raw;
}

HBITMAP LoadBoxArt() {
    const wchar_t* names[] = {L"boxart.png", L"boxart.jpg", L"boxart.jpeg", L"boxart.bmp",
                              L"banner.png", L"banner.jpg", L"banner.jpeg", L"banner.bmp"};
    for (const wchar_t* name : names) {
        const std::wstring path = FindDataFile(name);
        if (path.empty()) {
            continue;
        }
        HBITMAP bitmap = BitmapFromFile(path);
        if (bitmap) {
            return bitmap;
        }
    }
    return nullptr;
}

void EnsureTip(HWND owner) {
    if (gTip) {
        return;
    }
    gTip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                           WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
                           CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                           owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!gTip) {
        return;
    }
    TOOLINFOW info{};
    info.cbSize = sizeof(info);
    info.uFlags = TTF_TRACK | TTF_ABSOLUTE;
    info.hwnd = owner;
    info.uId = 1;
    info.lpszText = const_cast<wchar_t*>(L"Settings (Ctrl+,)");
    SendMessageW(gTip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&info));
}

// A null name is a section heading. The text is the heading.
struct HelpLine {
    const wchar_t* name;
    const wchar_t* text;
};

constexpr int kHelpMax = 32;

// The shortcut and controller lists share one modal frame. The labels sit on a
// tall child, and that child moves up inside a viewport so the OK button stays.
struct HelpState {
    int ruleY = 0;
    int inner = 0;
    int listH = 0;
    int page = 0;
    int scroll = 0;
    HFONT bold = nullptr;
    HWND viewport = nullptr;
    HWND content = nullptr;
    HWND bar = nullptr;
};

HelpState gHelp;

void ClearHelp() {
    if (gHelp.bold) {
        DeleteObject(gHelp.bold);
    }
    gHelp = {};
}

HFONT MakeBoldFont() {
    LOGFONTW font = MessageFont();
    font.lfWeight = FW_SEMIBOLD;
    return CreateFontIndirectW(&font);
}

int WheelDelta(WPARAM wp) {
    const unsigned int high = HIWORD(wp);
    return static_cast<int>(static_cast<short>(static_cast<unsigned short>(high)));
}

void SetHelpScroll(int next) {
    int maxScroll = gHelp.listH - gHelp.page;
    if (maxScroll < 0) {
        maxScroll = 0;
    }
    if (next < 0) {
        next = 0;
    }
    if (next > maxScroll) {
        next = maxScroll;
    }
    if (next == gHelp.scroll) {
        return;
    }
    gHelp.scroll = next;
    if (gHelp.content) {
        SetWindowPos(gHelp.content, nullptr, 0, -next, gHelp.inner, gHelp.listH,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
    }
    if (gHelp.bar) {
        SetScrollPos(gHelp.bar, SB_CTL, next, TRUE);
    }
}

void OnHelpVScroll(WPARAM wp) {
    const int line = FontHeight() * 2;
    int page = gHelp.page - FontHeight();
    if (page < line) {
        page = line;
    }
    switch (LOWORD(wp)) {
    case SB_LINEUP:
        SetHelpScroll(gHelp.scroll - line);
        break;
    case SB_LINEDOWN:
        SetHelpScroll(gHelp.scroll + line);
        break;
    case SB_PAGEUP:
        SetHelpScroll(gHelp.scroll - page);
        break;
    case SB_PAGEDOWN:
        SetHelpScroll(gHelp.scroll + page);
        break;
    case SB_TOP:
        SetHelpScroll(0);
        break;
    case SB_BOTTOM:
        SetHelpScroll(gHelp.listH);
        break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION: {
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_TRACKPOS;
        GetScrollInfo(gHelp.bar, SB_CTL, &si);
        SetHelpScroll(si.nTrackPos);
        break;
    }
    default:
        break;
    }
}

void InitHelpBar() {
    if (!gHelp.bar) {
        return;
    }
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = gHelp.listH > 1 ? gHelp.listH - 1 : 0;
    si.nPage = static_cast<UINT>(gHelp.page > 0 ? gHelp.page : 1);
    si.nPos = 0;
    SetScrollInfo(gHelp.bar, SB_CTL, &si, TRUE);
}

LRESULT CALLBACK HelpViewProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, gTheme.windowBrush);
        EndPaint(hwnd, &ps);
        return 0;
    }
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

LRESULT CALLBACK HelpListProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND: {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        FillRect(reinterpret_cast<HDC>(wp), &rc, gTheme.windowBrush);
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        if (gHelp.inner > 0) {
            HPEN pen = CreatePen(PS_SOLID, 1, gTheme.boxBorder);
            const HGDIOBJ old = SelectObject(dc, pen);
            MoveToEx(dc, 0, gHelp.ruleY, nullptr);
            LineTo(dc, gHelp.inner, gHelp.ruleY);
            SelectObject(dc, old);
            DeleteObject(pen);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
        return OnCtlColor(wp, lp, false);
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

LRESULT CALLBACK HelpFrameProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND: {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        FillRect(reinterpret_cast<HDC>(wp), &rc, gTheme.windowBrush);
        return 1;
    }
    case WM_CTLCOLORBTN:
        return OnCtlColor(wp, lp, false);
    case WM_DRAWITEM:
        PaintPush(reinterpret_cast<const DRAWITEMSTRUCT*>(lp));
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_VSCROLL:
        if (gHelp.bar && reinterpret_cast<HWND>(lp) == gHelp.bar) {
            OnHelpVScroll(wp);
        }
        return 0;
    case WM_MOUSEWHEEL:
        SetHelpScroll(gHelp.scroll - WheelDelta(wp) * FontHeight() * 2 / WHEEL_DELTA);
        return 0;
    case WM_KEYDOWN:
        switch (wp) {
        case VK_UP:
            SetHelpScroll(gHelp.scroll - FontHeight() * 2);
            break;
        case VK_DOWN:
            SetHelpScroll(gHelp.scroll + FontHeight() * 2);
            break;
        case VK_PRIOR:
            SetHelpScroll(gHelp.scroll - (gHelp.page > FontHeight() ? gHelp.page - FontHeight() : FontHeight()));
            break;
        case VK_NEXT:
            SetHelpScroll(gHelp.scroll + (gHelp.page > FontHeight() ? gHelp.page - FontHeight() : FontHeight()));
            break;
        case VK_HOME:
            SetHelpScroll(0);
            break;
        case VK_END:
            SetHelpScroll(gHelp.listH);
            break;
        default:
            break;
        }
        return 0;
    case WM_SETTINGCHANGE:
    case WM_SYSCOLORCHANGE:
    case WM_THEMECHANGED:
        RebuildTheme();
        ApplyFrameTheme(hwnd);
        RedrawWindow(hwnd, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        gHelp.viewport = nullptr;
        gHelp.content = nullptr;
        gHelp.bar = nullptr;
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

void RegisterHelpClass() {
    static bool ready = false;
    if (ready) {
        return;
    }
    const HINSTANCE inst = GetModuleHandleW(nullptr);
    const HCURSOR cursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    WNDCLASSW wc{};
    wc.hInstance = inst;
    wc.hCursor = cursor;
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    wc.lpfnWndProc = HelpFrameProc;
    wc.lpszClassName = L"BruceLeeHelp";
    RegisterClassW(&wc);
    wc.lpfnWndProc = HelpViewProc;
    wc.lpszClassName = L"BruceLeeHelpView";
    RegisterClassW(&wc);
    wc.lpfnWndProc = HelpListProc;
    wc.lpszClassName = L"BruceLeeHelpList";
    RegisterClassW(&wc);
    ready = true;
}

struct HelpLayout {
    int keyW = 0;
    int gap = 0;
    int actionW = 0;
    int pad = 0;
    int fontH = 0;
    int headerH = 0;
    int ruleY = 0;
    int tops[kHelpMax]{};
    int heights[kHelpMax]{};
    int noteTop = 0;
    int noteH = 0;
    int listH = 0;
    int inner = 0;
    int bodyH = 0;
    int extraW = 0;
    int clientW = 0;
    int clientH = 0;
    int btnW = 0;
    int btnH = 0;
};

HelpLayout LayoutHelp(HWND owner, HFONT nameFont, const wchar_t* keyHeading,
                      const HelpLine* lines, int count, const wchar_t* note) {
    HelpLayout layout{};
    HDC dc = GetDC(nullptr);
    const HFONT textFont = UiFont();
    const HFONT old = static_cast<HFONT>(SelectObject(dc, textFont));
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    const int fontH = tm.tmHeight > 0 ? tm.tmHeight : 16;
    layout.fontH = fontH;
    const int em = TextWidth(dc, L"M");
    int pad = fontH;
    if (pad < 12) {
        pad = 12;
    }
    layout.pad = pad;
    int gap = fontH;
    if (gap < 16) {
        gap = 16;
    }
    layout.gap = gap;
    int rowGap = fontH / 3;
    if (rowGap < 6) {
        rowGap = 6;
    }

    const HFONT keys = nameFont ? nameFont : textFont;
    SelectObject(dc, keys);
    int keyW = 0;
    for (int i = 0; i < count; ++i) {
        if (!lines[i].name) {
            continue;
        }
        const int width = TextWidth(dc, lines[i].name);
        if (width > keyW) {
            keyW = width;
        }
    }
    SelectObject(dc, textFont);
    const int headingW = TextWidth(dc, keyHeading);
    if (headingW > keyW) {
        keyW = headingW;
    }
    keyW += 4;
    layout.keyW = keyW;

    int natural = TextWidth(dc, L"Action");
    for (int i = 0; i < count; ++i) {
        if (!lines[i].name) {
            continue;
        }
        const int width = TextWidth(dc, lines[i].text);
        if (width > natural) {
            natural = width;
        }
    }
    int cap = em * 32;
    if (cap < 280) {
        cap = 280;
    }
    int minAction = em * 16;
    if (minAction < 160) {
        minAction = 160;
    }
    int actionW = natural;
    if (actionW > cap) {
        actionW = cap;
    }
    if (actionW < minAction) {
        actionW = minAction;
    }

    HMONITOR monitor = MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    RECT work{0, 0, 1280, 800};
    if (GetMonitorInfoW(monitor, &monitorInfo)) {
        work = monitorInfo.rcWork;
    }
    const int workW = work.right - work.left;
    const int workH = work.bottom - work.top;
    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    const DWORD exStyle = WS_EX_DLGMODALFRAME;
    RECT chrome{0, 0, 0, 0};
    AdjustWindowRectEx(&chrome, style, FALSE, exStyle);
    const int chromeW = chrome.right - chrome.left;
    const int chromeH = chrome.bottom - chrome.top;

    int maxAction = workW - chromeW - pad * 2 - keyW - gap;
    if (maxAction < minAction) {
        maxAction = minAction;
    }
    if (actionW > maxAction) {
        actionW = maxAction;
    }

    int btnH = fontH + fontH / 2;
    if (btnH < 28) {
        btnH = 28;
    }
    int btnW = TextWidth(dc, L"OK") + em * 4;
    if (btnW < em * 8) {
        btnW = em * 8;
    }
    layout.btnW = btnW;
    layout.btnH = btnH;

    auto measure = [&](int width) {
        layout.actionW = width;
        layout.inner = keyW + gap + width;
        layout.ruleY = fontH + 3;
        layout.headerH = fontH + fontH / 2 + 4;
        if (layout.headerH < layout.ruleY + 8) {
            layout.headerH = layout.ruleY + 8;
        }
        int y = layout.headerH;
        for (int i = 0; i < count; ++i) {
            if (!lines[i].name && i > 0) {
                int sectionGap = fontH / 2;
                if (sectionGap < 8) {
                    sectionGap = 8;
                }
                y += sectionGap;
            }
            layout.tops[i] = y;
            int h = fontH;
            if (lines[i].name) {
                h = WrapHeight(dc, lines[i].text, width);
                if (h < fontH) {
                    h = fontH;
                }
            }
            h += 4;
            layout.heights[i] = h;
            y += h + rowGap;
        }
        if (note && note[0] != L'\0') {
            int noteGap = fontH / 2;
            if (noteGap < 8) {
                noteGap = 8;
            }
            y += noteGap;
            layout.noteTop = y;
            layout.noteH = WrapHeight(dc, note, layout.inner);
            if (layout.noteH < fontH) {
                layout.noteH = fontH;
            }
            layout.noteH += 2;
            y += layout.noteH;
        } else {
            layout.noteTop = y;
            layout.noteH = 0;
        }
        layout.listH = y + 2;
    };
    measure(actionW);

    auto clientHeight = [&](int body) {
        return pad + body + pad / 2 + btnH + pad;
    };
    int bodyH = layout.listH;
    int extraW = 0;
    if (chromeH + clientHeight(bodyH) > workH) {
        extraW = GetSystemMetrics(SM_CXVSCROLL);
        int scrolled = maxAction - extraW;
        if (scrolled < minAction) {
            scrolled = minAction;
        }
        if (actionW > scrolled) {
            actionW = scrolled;
            measure(actionW);
        }
        int maxBody = workH - chromeH - (pad + pad / 2 + btnH + pad);
        if (maxBody < fontH * 4) {
            maxBody = fontH * 4;
        }
        bodyH = layout.listH;
        if (bodyH > maxBody) {
            bodyH = maxBody;
        }
        if (layout.listH <= bodyH) {
            extraW = 0;
            bodyH = layout.listH;
        }
    }
    layout.bodyH = bodyH;
    layout.extraW = extraW;
    layout.clientW = pad * 2 + layout.inner + extraW;
    if (layout.clientW < btnW + pad * 2) {
        layout.clientW = btnW + pad * 2;
    }
    layout.clientH = clientHeight(bodyH);

    SelectObject(dc, old);
    ReleaseDC(nullptr, dc);
    return layout;
}

HWND AddLabel(HWND parent, const wchar_t* text, DWORD extra, int x, int y, int w, int h,
              HFONT font, int id) {
    HWND ctrl = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | extra, x, y, w, h, parent,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                GetModuleHandleW(nullptr), nullptr);
    if (ctrl && font) {
        SendMessageW(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
    }
    return ctrl;
}

bool HelpFrame(HWND hwnd) {
    wchar_t name[32]{};
    return GetClassNameW(hwnd, name, 32) > 0 && lstrcmpW(name, L"BruceLeeHelp") == 0;
}

// Escape and Enter close the box. The shortcut and controller lists also
// scroll with the wheel and the arrow keys. About has nothing to scroll.
// False means the message loop was quit.
bool RunModal(HWND dlg, HWND owner) {
    const bool help = HelpFrame(dlg);
    EnableWindow(owner, FALSE);
    bool quit = false;
    MSG msg{};
    for (;;) {
        if (!IsWindow(dlg)) {
            break;
        }
        const BOOL got = GetMessageW(&msg, nullptr, 0, 0);
        if (got == 0) {
            PostQuitMessage(static_cast<int>(msg.wParam));
            quit = true;
            break;
        }
        if (got == -1) {
            break;
        }
        const bool inside = msg.hwnd == dlg || IsChild(dlg, msg.hwnd);
        if (inside && msg.message == WM_KEYDOWN &&
            (msg.wParam == VK_ESCAPE || msg.wParam == VK_RETURN)) {
            DestroyWindow(dlg);
            continue;
        }
        if (help && inside && msg.message == WM_MOUSEWHEEL) {
            SendMessageW(dlg, WM_MOUSEWHEEL, msg.wParam, msg.lParam);
            continue;
        }
        if (help && inside && msg.message == WM_KEYDOWN &&
            (msg.wParam == VK_UP || msg.wParam == VK_DOWN || msg.wParam == VK_PRIOR ||
             msg.wParam == VK_NEXT || msg.wParam == VK_HOME || msg.wParam == VK_END)) {
            SendMessageW(dlg, WM_KEYDOWN, msg.wParam, msg.lParam);
            continue;
        }
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    return !quit;
}

void ShowHelp(HWND owner, const wchar_t* title, const wchar_t* keyHeading, const HelpLine* lines,
              int count, const wchar_t* note) {
    if (!owner || !lines || count < 1 || count > kHelpMax) {
        return;
    }
    EnsureUiClasses();
    RegisterHelpClass();
    RebuildTheme();
    ClearHelp();
    gHelp.bold = MakeBoldFont();
    const HelpLayout layout = LayoutHelp(owner, gHelp.bold, keyHeading, lines, count, note);
    gHelp.ruleY = layout.ruleY;
    gHelp.inner = layout.inner;
    gHelp.listH = layout.listH;
    gHelp.page = layout.bodyH;
    gHelp.scroll = 0;

    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    const DWORD exStyle = WS_EX_DLGMODALFRAME;
    RECT rc{0, 0, layout.clientW, layout.clientH};
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);
    const HINSTANCE inst = GetModuleHandleW(nullptr);
    HWND dlg = CreateWindowExW(exStyle, L"BruceLeeHelp", title, style, 0, 0, rc.right - rc.left,
                               rc.bottom - rc.top, owner, nullptr, inst, nullptr);
    if (!dlg) {
        ClearHelp();
        return;
    }
    ApplyFrameTheme(dlg);
    HWND view = CreateWindowExW(0, L"BruceLeeHelpView", nullptr,
                                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                layout.pad, layout.pad, layout.inner, layout.bodyH, dlg, nullptr, inst,
                                nullptr);
    if (!view) {
        DestroyWindow(dlg);
        ClearHelp();
        return;
    }
    gHelp.viewport = view;
    HWND content = CreateWindowExW(0, L"BruceLeeHelpList", nullptr,
                                   WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0,
                                   layout.inner, layout.listH, view, nullptr, inst, nullptr);
    if (!content) {
        DestroyWindow(dlg);
        ClearHelp();
        return;
    }
    gHelp.content = content;

    const HFONT textFont = UiFont();
    const HFONT nameFont = gHelp.bold ? gHelp.bold : textFont;
    const DWORD single = SS_LEFTNOWORDWRAP | SS_NOPREFIX;
    const DWORD wrapped = SS_LEFT | SS_NOPREFIX;
    AddLabel(content, keyHeading, single, 0, 0, layout.keyW, layout.fontH, textFont, IDC_HINT);
    AddLabel(content, L"Action", single, layout.keyW + layout.gap, 0, layout.actionW, layout.fontH,
             textFont, IDC_HINT);
    for (int i = 0; i < count; ++i) {
        const int y = layout.tops[i];
        const int h = layout.heights[i];
        if (!lines[i].name) {
            AddLabel(content, lines[i].text, single, 0, y, layout.inner, h, nameFont, 0);
            continue;
        }
        AddLabel(content, lines[i].name, single, 0, y, layout.keyW, h, nameFont, 0);
        AddLabel(content, lines[i].text, wrapped, layout.keyW + layout.gap, y, layout.actionW, h,
                 textFont, 0);
    }
    if (note && note[0] != L'\0' && layout.noteH > 0) {
        AddLabel(content, note, wrapped, 0, layout.noteTop, layout.inner, layout.noteH, textFont,
                 IDC_HINT);
    }
    if (layout.extraW > 0) {
        gHelp.bar = CreateWindowExW(0, L"SCROLLBAR", nullptr, WS_CHILD | WS_VISIBLE | SBS_VERT,
                                    layout.pad + layout.inner, layout.pad, layout.extraW, layout.bodyH,
                                    dlg, nullptr, inst, nullptr);
        InitHelpBar();
    }
    const int btnY = layout.pad + layout.bodyH + layout.pad / 2;
    HWND ok = CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                              (layout.clientW - layout.btnW) / 2, btnY, layout.btnW, layout.btnH, dlg,
                              reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), inst, nullptr);
    SetWindowSubclass(ok, CheckEraseProc, 1, 0);
    PlaceOnOwner(dlg, owner);
    ShowWindow(dlg, SW_SHOW);
    SetFocus(ok);
    RunModal(dlg, owner);
    if (!IsWindow(dlg)) {
        ClearHelp();
    }
}

}  // namespace

HMENU CreateAppMenu() {
    EnsureUiClasses();
    HMENU bar = CreateMenu();
    HMENU file = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, ID_RESTART, L"&Restart Game\tF1");
    AppendMenuW(file, MF_STRING | MF_GRAYED, ID_SAVE, L"&Save game...\tCtrl+S");
    AppendMenuW(file, MF_STRING | MF_GRAYED, ID_LOAD, L"&Load game...\tCtrl+O");
    AppendMenuW(file, MF_STRING | MF_GRAYED, ID_QUICKSAVE, L"&Quick save\tF6");
    AppendMenuW(file, MF_STRING | MF_GRAYED, ID_QUICKLOAD, L"Quick &Load\tF9");
    AppendMenuW(file, MF_STRING, ID_SCREENSHOT, L"Take S&creenshot\tF12");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, ID_EXIT, L"E&xit\tAlt+F4");
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"&File");

    HMENU about = CreatePopupMenu();
    AppendMenuW(about, MF_STRING, ID_MANUAL, L"Game &Manual\tF2");
    AppendMenuW(about, MF_STRING, ID_KEYS, L"&Keyboard shortcuts");
    AppendMenuW(about, MF_STRING, ID_CONTROLLER, L"&Controller mapping");
    AppendMenuW(about, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(about, MF_STRING, ID_ABOUT, L"&About Bruce Lee Remake...\tCtrl+F1");
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(about), L"&About");
    // Owner-drawn so the bar text can follow the dark theme. The popup menus stay attached.
    const wchar_t* captions[] = {L"&File", L"&About"};
    for (UINT index = 0; index < 2; ++index) {
        MENUITEMINFOW info{};
        info.cbSize = sizeof(info);
        info.fMask = MIIM_FTYPE | MIIM_DATA;
        info.fType = MFT_OWNERDRAW;
        info.dwItemData = reinterpret_cast<ULONG_PTR>(captions[index]);
        SetMenuItemInfoW(bar, index, TRUE, &info);
    }

    AppendMenuW(bar, MF_OWNERDRAW | MF_RIGHTJUSTIFY, ID_SETTINGS, L"Settings");
    return bar;
}

HACCEL CreateAppAccelerators() {
    ACCEL keys[] = {
        {FCONTROL | FVIRTKEY, VK_F1, ID_ABOUT},
        {FVIRTKEY, VK_F1, ID_RESTART},
        {FVIRTKEY, VK_F2, ID_MANUAL},
        {FCONTROL | FVIRTKEY, VK_OEM_COMMA, ID_SETTINGS},
        {FVIRTKEY, VK_F12, ID_SCREENSHOT},
    };
    return CreateAcceleratorTableW(keys, static_cast<int>(sizeof keys / sizeof keys[0]));
}

void RefreshSettingsDialog() {
    if (!gSettings) {
        return;
    }
    gRefreshing = true;
    gConfig.startRoom = ClampedStartRoom();
    SetRoomText();
    gRefreshing = false;
    InvalidateCheck(gChkLives);
    InvalidateCheck(gChkEnemies);
    InvalidateCheck(gChkTitle);
    InvalidateCheck(gChkFix);
    InvalidateCheck(gChkFull);
    InvalidateCheck(gChkCrt);
    InvalidateCheck(gChkCurve);
    InvalidateCheck(gChkScanlines);
    InvalidateCheck(gChkBlur);
    InvalidateCheck(gChkVignette);
    InvalidateCheck(gChkSound);
    RefreshGamepadCombo();
}

void ApplyMenuBarBrush(HWND hwnd) {
    HMENU menu = hwnd ? GetMenu(hwnd) : nullptr;
    if (!menu) {
        return;
    }
    MENUINFO info{};
    info.cbSize = sizeof(info);
    info.fMask = MIM_BACKGROUND;
    info.hbrBack = (gTheme.dark && !gTheme.highContrast) ? gTheme.windowBrush : nullptr;
    SetMenuInfo(menu, &info);
}

void ApplyAppTheme(HWND hwnd) {
    if (hwnd && GetMenu(hwnd)) {
        // Drop the brush before RebuildTheme deletes it.
        MENUINFO info{};
        info.cbSize = sizeof(info);
        info.fMask = MIM_BACKGROUND;
        info.hbrBack = nullptr;
        SetMenuInfo(GetMenu(hwnd), &info);
    }
    RebuildTheme();
    UseDarkAppMode(!gTheme.highContrast);
    if (!hwnd) {
        return;
    }
    AllowDarkWindow(hwnd, gTheme.dark && !gTheme.highContrast);
    ApplyFrameTheme(hwnd);
    ApplyMenuBarBrush(hwnd);
    if (GetMenu(hwnd)) {
        DrawMenuBar(hwnd);
    }
}

void ToggleSettings(HWND owner) {
    if (gSettings) {
        SetForegroundWindow(gSettings);
        return;
    }
    EnsureUiClasses();
    RegisterSettingsClass();
    RebuildTheme();
    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, L"BruceLeeSettings", L"Settings",
                                WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
                                0, 0, 320, 240,
                                owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwnd) {
        return;
    }
    gSettings = hwnd;
    ApplyFrameTheme(hwnd);
    BuildSettings(hwnd);
    FitSettingsWindow(hwnd);
    RefreshSettingsDialog();
    PlaceSettings(hwnd, owner);
    gSettingsReady = true;
    ShowWindow(hwnd, SW_SHOW);
    SetFocus(gChkLives);
    SetTimer(hwnd, kGamepadTimer, 250, nullptr);
}

void CloseSettings() {
    if (gSettings) {
        DestroyWindow(gSettings);
        gSettings = nullptr;
    }
}

bool SettingsIsOpen() {
    return gSettings != nullptr;
}

bool SettingsContains(HWND hwnd) {
    return gSettings && (hwnd == gSettings || IsChild(gSettings, hwnd));
}

bool UiOwnsKeyboard() {
    return gSettings && GetForegroundWindow() == gSettings;
}

bool SettingsDialogMessage(MSG* msg) {
    return gSettings && IsDialogMessageW(gSettings, msg);
}

void OpenManual(HWND owner) {
    const std::wstring path = FindDataFile(L"GameManual.pdf");
    if (path.empty()) {
        MessageBoxW(owner, L"Could not find GameManual.pdf.", L"Bruce Lee", MB_ICONERROR);
        return;
    }
    const HINSTANCE opened = ShellExecuteW(owner, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(opened) <= 32) {
        MessageBoxW(owner, L"Could not open the game manual.", L"Bruce Lee", MB_ICONERROR);
    }
}

void ShowAbout(HWND owner) {
    EnsureUiClasses();
    RegisterAboutClass();
    RebuildTheme();
    HBITMAP raw = LoadBoxArt();
    BITMAP source{};
    if (raw && !GetObjectW(raw, sizeof(source), &source)) {
        DeleteObject(raw);
        raw = nullptr;
    }

    HMONITOR monitor = MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    RECT work{0, 0, 1280, 800};
    if (GetMonitorInfoW(monitor, &info)) {
        work = info.rcWork;
    }
    const int workW = work.right - work.left;
    const int workH = work.bottom - work.top;
    const int chrome = GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYFRAME) * 2;

    HDC dc = GetDC(nullptr);
    const HFONT old = static_cast<HFONT>(SelectObject(dc, UiFont()));
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    const int fontH = tm.tmHeight > 0 ? tm.tmHeight : 16;
    const int em = TextWidth(dc, L"M");
    int pad = fontH;
    if (pad < 12) {
        pad = 12;
    }
    int textW = em * 36;
    if (textW < 320) {
        textW = 320;
    }
    if (textW > workW - pad * 2 - 32) {
        textW = workW - pad * 2 - 32;
    }
    if (textW < 200) {
        textW = 200;
    }
    RECT measure{0, 0, textW, 0};
    DrawTextW(dc, kAboutText, -1, &measure, DT_CALCRECT | DT_WORDBREAK | DT_LEFT | DT_NOPREFIX);
    const int textH = (measure.bottom > 0 ? measure.bottom : fontH * 8) + 4;
    int btnH = fontH + fontH / 2;
    if (btnH < 28) {
        btnH = 28;
    }
    int btnW = TextWidth(dc, L"OK") + em * 4;
    if (btnW < em * 8) {
        btnW = em * 8;
    }
    SelectObject(dc, old);
    ReleaseDC(nullptr, dc);

    int imageMaxW = em * 22;
    if (imageMaxW < 240) {
        imageMaxW = 240;
    }
    if (imageMaxW > textW) {
        imageMaxW = textW;
    }
    int imageMaxH = workH - chrome - textH - btnH - pad * 4 - 16;
    if (imageMaxH < fontH * 8) {
        imageMaxH = fontH * 8;
    }
    HBITMAP banner = nullptr;
    int bannerW = 0;
    int bannerH = 0;
    if (raw && source.bmWidth > 0 && source.bmHeight > 0) {
        double scale = 1.0;
        if (source.bmWidth > imageMaxW) {
            scale = static_cast<double>(imageMaxW) / static_cast<double>(source.bmWidth);
        }
        if (static_cast<double>(source.bmHeight) * scale > imageMaxH) {
            scale = static_cast<double>(imageMaxH) / static_cast<double>(source.bmHeight);
        }
        bannerW = static_cast<int>(source.bmWidth * scale + 0.5);
        bannerH = static_cast<int>(source.bmHeight * scale + 0.5);
        if (bannerW < 1) {
            bannerW = 1;
        }
        if (bannerH < 1) {
            bannerH = 1;
        }
        if (bannerW == source.bmWidth && bannerH == source.bmHeight) {
            banner = raw;
        } else {
            banner = ScaleBitmap(raw, bannerW, bannerH);
            DeleteObject(raw);
        }
        raw = nullptr;
        if (!banner) {
            bannerW = 0;
            bannerH = 0;
        }
    }

    const int contentW = textW > bannerW ? textW : bannerW;
    const int clientW = contentW + pad * 2;
    int y = pad;
    const int bannerY = y;
    if (banner) {
        y += bannerH + pad;
    }
    const int textY = y;
    y += textH + pad / 2;
    const int btnY = y;
    y += btnH + pad;

    RECT rc{0, 0, clientW, y};
    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    const DWORD ex = WS_EX_DLGMODALFRAME;
    AdjustWindowRectEx(&rc, style, FALSE, ex);
    HWND dlg = CreateWindowExW(ex, L"BruceLeeAbout", L"About Bruce Lee", style,
                               0, 0, rc.right - rc.left, rc.bottom - rc.top,
                               owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!dlg) {
        if (banner) {
            DeleteObject(banner);
        }
        return;
    }
    ApplyFrameTheme(dlg);
    const HINSTANCE inst = GetModuleHandleW(nullptr);
    if (banner) {
        gAboutBitmap = banner;
        HWND picture = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | SS_BITMAP,
                                       pad + (contentW - bannerW) / 2, bannerY, bannerW, bannerH,
                                       dlg, nullptr, inst, nullptr);
        SendMessageW(picture, STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(banner));
    }
    HWND text = CreateWindowExW(0, L"STATIC", kAboutText, WS_CHILD | WS_VISIBLE,
                                pad, textY, textW, textH, dlg, nullptr, inst, nullptr);
    SendMessageW(text, WM_SETFONT, reinterpret_cast<WPARAM>(UiFont()), FALSE);
    HWND ok = CreateWindowExW(0, L"BUTTON", L"OK",
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                              (clientW - btnW) / 2, btnY, btnW, btnH, dlg,
                              reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), inst, nullptr);
    SetWindowSubclass(ok, CheckEraseProc, 1, 0);
    PlaceOnOwner(dlg, owner);
    ShowWindow(dlg, SW_SHOW);
    SetFocus(ok);
    RunModal(dlg, owner);
}

constexpr wchar_t kWelcomeHeading[] = L"Welcome to Bruce Lee Remake.";
constexpr wchar_t kWelcomeIntro[] =
    L"How do you want your experience to look? "
    L"You can change this and more later in Settings.";
constexpr wchar_t kModernExperience[] = L"Modern experience";
constexpr wchar_t kAuthenticExperience[] = L"Authentic experience";
constexpr wchar_t kModernNote[] = L"Modern TV. Fix scoring bug in the original.";
constexpr wchar_t kAuthenticNote[] = L"Old School TV. Keep original scoring bug.";
constexpr wchar_t kTrainersHeading[] = L"Trainers";
constexpr wchar_t kPadAdvice[] =
    L"It is highly recommended to play with a gamepad. Prefer the D-pad.";

HBITMAP gWizardModernBmp = nullptr;
HBITMAP gWizardAuthenticBmp = nullptr;
HFONT gWizardBold = nullptr;
HFONT gWizardLarge = nullptr;
bool gWizardSave = true;
bool gWizardDone = false;
int gWizardShotW = 0;
int gWizardShotH = 0;

struct WizardLayout {
    int pad = 0;
    int imageGap = 0;
    int clientW = 0;
    int clientH = 0;
    int contentW = 0;
    int colW = 0;
    int imageW = 0;
    int imageH = 0;
    int radioH = 0;
    int noteH = 0;
    int noteW = 0;
    int afterBox = 0;
    bool stacked = false;
    int headingY = 0;
    int headingH = 0;
    int introY = 0;
    int introH = 0;
    int colX[2] = {};
    int colY[2] = {};
    int trainersY = 0;
    int trainersH = 0;
    int checksY = 0;
    int checksW = 0;
    int rowH = 0;
    int adviceX = 0;
    int adviceY = 0;
    int adviceW = 0;
    int adviceH = 0;
    int btnX = 0;
    int btnY = 0;
    int btnW = 0;
    int btnH = 0;
};

// Sharp picture, and one landing pays 50 once. The four television parts
// stay as they are, so Settings can turn the old picture back on.
void ApplyModernExperience() {
    gConfig.showCrtFilter = false;
    gConfig.fixBugsInOriginal = true;
}

// The old television, and the original landing score.
void ApplyAuthenticExperience() {
    gConfig.showCrtFilter = true;
    gConfig.crtCurve = true;
    gConfig.crtScanlines = true;
    gConfig.crtBlur = true;
    gConfig.crtVignette = true;
    gConfig.fixBugsInOriginal = false;
}

void SelectWizard(HWND dlg, bool modern) {
    gWizardModern = modern;
    if (modern) {
        ApplyModernExperience();
    } else {
        ApplyAuthenticExperience();
    }
    if (!dlg) {
        return;
    }
    InvalidateRect(GetDlgItem(dlg, IDC_WIZ_MODERN), nullptr, FALSE);
    InvalidateRect(GetDlgItem(dlg, IDC_WIZ_AUTHENTIC), nullptr, FALSE);
}

void FinishWizard() {
    if (gWizardDone) {
        return;
    }
    gWizardDone = true;
    if (!gWizardSave) {
        return;
    }
    if (gWizardModern) {
        ApplyModernExperience();
    } else {
        ApplyAuthenticExperience();
    }
    gConfig.firstTimeRun = false;
    SaveConfig();
}

void ReleaseWizardPictures() {
    if (gWizardModernBmp) {
        DeleteObject(gWizardModernBmp);
        gWizardModernBmp = nullptr;
    }
    if (gWizardAuthenticBmp) {
        DeleteObject(gWizardAuthenticBmp);
        gWizardAuthenticBmp = nullptr;
    }
}

HFONT MakeLargeFont() {
    LOGFONTW font = MessageFont();
    if (font.lfHeight < 0) {
        font.lfHeight = font.lfHeight * 3 / 2;
    } else if (font.lfHeight > 0) {
        font.lfHeight = font.lfHeight * 3 / 2;
    } else {
        font.lfHeight = -22;
    }
    font.lfWeight = FW_SEMIBOLD;
    return CreateFontIndirectW(&font);
}

void ReleaseWizardFont() {
    if (gWizardBold) {
        DeleteObject(gWizardBold);
        gWizardBold = nullptr;
    }
    if (gWizardLarge) {
        DeleteObject(gWizardLarge);
        gWizardLarge = nullptr;
    }
}

uint32_t PackedColor(COLORREF color) {
    return 0xFF000000u | (static_cast<uint32_t>(GetRValue(color)) << 16) |
           (static_cast<uint32_t>(GetGValue(color)) << 8) | static_cast<uint32_t>(GetBValue(color));
}

// Room 1 with the Ninja and Yamo, scaled into the thumbnail. Modern is the
// nearest-neighbor scale. Authentic is the television, drawn large enough
// that its scanlines resolve and then averaged back to this size.
std::vector<uint32_t> gWizardSource;

const float* ThumbnailLinear() {
    static float table[256];
    static bool ready = false;
    if (ready) {
        return table;
    }
    for (int channel = 0; channel < 256; ++channel) {
        const float c = static_cast<float>(channel) / 255.f;
        if (c <= 0.04045f) {
            table[channel] = c / 12.92f;
        } else {
            table[channel] = std::pow((c + 0.055f) / 1.055f, 2.4f);
        }
    }
    ready = true;
    return table;
}

int ThumbnailByte(float linear) {
    if (linear <= 0.f) {
        return 0;
    }
    if (linear >= 1.f) {
        return 255;
    }
    float encoded = linear * 12.92f;
    if (linear > 0.0031308f) {
        encoded = 1.055f * std::pow(linear, 1.f / 2.4f) - 0.055f;
    }
    int byte = static_cast<int>(encoded * 255.f + 0.5f);
    if (byte < 0) {
        return 0;
    }
    if (byte > 255) {
        return 255;
    }
    return byte;
}

// The thumbnail is about one pixel per source row, and the television draws
// a scanline on every row. Sampled at that size the lines beat against the
// pixels and turn into waves. Four samples across each source pixel let the
// lines exist, and averaging them in linear light leaves the glass without
// the waves.
void DrawAuthenticShot(const uint32_t* src, uint32_t* dest, int destW, int innerW, int innerH) {
    int scale = 1;
    while (scale < 8 && innerH * scale < kHeight * 4) {
        ++scale;
    }
    if (scale == 1) {
        std::vector<uint32_t> temp(static_cast<size_t>(innerW) * static_cast<size_t>(innerH));
        ApplyCrt(src, kWidth, kHeight, temp.data(), innerW, innerH, CrtOptions{true, true, true, true});
        for (int y = 0; y < innerH; ++y) {
            uint32_t* row = dest + static_cast<size_t>(y + 1) * static_cast<size_t>(destW) + 1;
            const uint32_t* from = temp.data() + static_cast<size_t>(y) * static_cast<size_t>(innerW);
            for (int x = 0; x < innerW; ++x) {
                row[x] = from[x];
            }
        }
        return;
    }
    const int bigW = innerW * scale;
    const int bigH = innerH * scale;
    std::vector<uint32_t> big(static_cast<size_t>(bigW) * static_cast<size_t>(bigH));
    ApplyCrt(src, kWidth, kHeight, big.data(), bigW, bigH, CrtOptions{true, true, true, true});
    const float* linear = ThumbnailLinear();
    const float samples = static_cast<float>(scale * scale);
    for (int y = 0; y < innerH; ++y) {
        uint32_t* row = dest + static_cast<size_t>(y + 1) * static_cast<size_t>(destW) + 1;
        for (int x = 0; x < innerW; ++x) {
            float red = 0.f;
            float green = 0.f;
            float blue = 0.f;
            for (int sy = 0; sy < scale; ++sy) {
                const uint32_t* from = big.data() +
                                       (static_cast<size_t>(y * scale + sy) * static_cast<size_t>(bigW) +
                                        static_cast<size_t>(x * scale));
                for (int sx = 0; sx < scale; ++sx) {
                    const uint32_t pixel = from[sx];
                    red += linear[(pixel >> 16) & 255u];
                    green += linear[(pixel >> 8) & 255u];
                    blue += linear[pixel & 255u];
                }
            }
            row[x] = 0xFF000000u | (static_cast<uint32_t>(ThumbnailByte(red / samples)) << 16) |
                     (static_cast<uint32_t>(ThumbnailByte(green / samples)) << 8) |
                     static_cast<uint32_t>(ThumbnailByte(blue / samples));
        }
    }
}

HBITMAP MakeExperienceBitmap(bool crt, int destW, int destH) {
    if (destW < 4 || destH < 4) {
        return nullptr;
    }
    const uint32_t* srcPixels = gWizardSource.size() == static_cast<size_t>(kWidth * kHeight)
                                    ? gWizardSource.data()
                                    : g.pixels.data();
    if (gWizardSource.size() != static_cast<size_t>(kWidth * kHeight) &&
        g.pixels.size() != static_cast<size_t>(kWidth * kHeight)) {
        return nullptr;
    }
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = destW;
    info.bmiHeader.biHeight = -destH;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!bitmap || !bits) {
        if (bitmap) {
            DeleteObject(bitmap);
        }
        return nullptr;
    }
    auto* dest = static_cast<uint32_t*>(bits);
    const uint32_t border = PackedColor(gTheme.boxBorder);
    const int count = destW * destH;
    for (int i = 0; i < count; ++i) {
        dest[i] = border;
    }
    const int innerW = destW - 2;
    const int innerH = destH - 2;
    const uint32_t* src = srcPixels;
    if (crt) {
        DrawAuthenticShot(src, dest, destW, innerW, innerH);
    } else {
        for (int y = 0; y < innerH; ++y) {
            const int sy = static_cast<int>((static_cast<long long>(y) * kHeight) / innerH);
            uint32_t* row = dest + static_cast<size_t>(y + 1) * static_cast<size_t>(destW) + 1;
            const uint32_t* from = src + static_cast<size_t>(sy) * static_cast<size_t>(kWidth);
            for (int x = 0; x < innerW; ++x) {
                const int sx = static_cast<int>((static_cast<long long>(x) * kWidth) / innerW);
                row[x] = from[sx];
            }
        }
    }
    return bitmap;
}

void SwapShot(HWND picture, HBITMAP& slot, HBITMAP next) {
    if (!next) {
        return;
    }
    if (!picture) {
        DeleteObject(next);
        return;
    }
    const auto previous = reinterpret_cast<HBITMAP>(
        SendMessageW(picture, STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(next)));
    if (previous && previous != next) {
        DeleteObject(previous);
    }
    slot = next;
}

void RefreshWizardShots(HWND dlg) {
    if (gWizardShotW < 4 || gWizardShotH < 4) {
        return;
    }
    SwapShot(GetDlgItem(dlg, IDC_WIZ_MODERN_PIC), gWizardModernBmp,
             MakeExperienceBitmap(false, gWizardShotW, gWizardShotH));
    SwapShot(GetDlgItem(dlg, IDC_WIZ_AUTHENTIC_PIC), gWizardAuthenticBmp,
             MakeExperienceBitmap(true, gWizardShotW, gWizardShotH));
}

// Owner-drawn buttons share their type bits with BS_AUTORADIOBUTTON, so the
// two experiences are a group only in this proc. Any arrow moves to the other.
LRESULT CALLBACK WizardRadioProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_ERASEBKGND) {
        return 1;
    }
    if (msg == WM_KEYDOWN &&
        (wp == VK_LEFT || wp == VK_RIGHT || wp == VK_UP || wp == VK_DOWN)) {
        const bool toModern = GetDlgCtrlID(hwnd) != IDC_WIZ_MODERN;
        HWND dlg = GetParent(hwnd);
        SelectWizard(dlg, toModern);
        SetFocus(GetDlgItem(dlg, toModern ? IDC_WIZ_MODERN : IDC_WIZ_AUTHENTIC));
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

int Larger(int a, int b) {
    return a > b ? a : b;
}

WizardLayout LayoutWizard(HDC dc, HFONT bold, HFONT large, int maxW, int maxH) {
    WizardLayout layout{};
    const HFONT old = static_cast<HFONT>(SelectObject(dc, UiFont()));
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    const int fontH = tm.tmHeight > 0 ? tm.tmHeight : 16;
    int pad = fontH;
    if (pad < 12) {
        pad = 12;
    }
    const int em = TextWidth(dc, L"M");
    const int rowH = fontH + fontH / 2 + 4;
    int box = fontH;
    if (box < 14) {
        box = 14;
    }
    const int afterBox = kFocusPad + box + (box >= 18 ? 10 : 8);
    const int labelNeed = afterBox + Larger(TextWidth(dc, kModernExperience), TextWidth(dc, kAuthenticExperience));
    const int noteNeed = afterBox + Larger(TextWidth(dc, kModernNote), TextWidth(dc, kAuthenticNote));
    const int checksNeed = afterBox + Larger(TextWidth(dc, L"Infinite Lives"), TextWidth(dc, L"Enemies cannot hit Bruce"));
    // A column narrower than the 320-wide picture makes the television hard to
    // see. The fit below shrinks it when the work area cannot hold two.
    int colWant = Larger(labelNeed, noteNeed);
    if (colWant < kWidth) {
        colWant = kWidth;
    }
    int cap = em * 48;
    if (cap < kWidth) {
        cap = kWidth;
    }
    if (cap < labelNeed) {
        cap = labelNeed;
    }
    if (colWant > cap) {
        colWant = cap;
    }
    SelectObject(dc, bold ? bold : UiFont());
    TEXTMETRICW boldTm{};
    GetTextMetricsW(dc, &boldTm);
    const int boldH = boldTm.tmHeight > 0 ? boldTm.tmHeight : fontH + 2;
    const int headingLine = TextWidth(dc, kWelcomeHeading);
    SelectObject(dc, UiFont());
    int btnH = fontH + fontH / 2;
    if (btnH < 28) {
        btnH = 28;
    }
    int btnW = TextWidth(dc, L"OK") + em * 4;
    if (btnW < em * 8) {
        btnW = em * 8;
    }
    const int gap = pad;
    const int imageGap = pad / 2 > 4 ? pad / 2 : 4;
    if (maxW < 80) {
        maxW = 80;
    }
    if (maxH < 80) {
        maxH = 80;
    }

    bool stacked = false;
    int colW = colWant;
    int imageW = colW;
    for (int attempt = 0; attempt < 12; ++attempt) {
        if (!stacked) {
            const int pairW = pad * 2 + colW * 2 + gap;
            if (pairW > maxW) {
                const int fit = (maxW - pad * 2 - gap) / 2;
                if (fit >= labelNeed && fit < colW) {
                    colW = fit;
                    if (imageW > colW) {
                        imageW = colW;
                    }
                    continue;
                }
                stacked = true;
                int room = maxW - pad * 2;
                if (room < labelNeed) {
                    room = labelNeed;
                }
                int comfortable = em * 42;
                if (comfortable < checksNeed) {
                    comfortable = checksNeed;
                }
                if (comfortable < labelNeed) {
                    comfortable = labelNeed;
                }
                if (room > comfortable) {
                    room = comfortable;
                }
                colW = room;
                imageW = colW;
                continue;
            }
        }

        int contentW = stacked ? colW : colW * 2 + gap;
        if (contentW < checksNeed) {
            contentW = checksNeed;
            if (stacked && colW < contentW) {
                colW = contentW;
                if (imageW > colW) {
                    imageW = colW;
                }
            }
        }
        if (imageW > colW) {
            imageW = colW;
        }
        if (imageW < 8) {
            imageW = 8;
        }
        int imageH = (imageW - 2) * kHeight / kWidth + 2;
        if (imageH < 4) {
            imageH = 4;
        }
        int noteW = colW - afterBox;
        if (noteW < 20) {
            noteW = 20;
        }
        const int noteH = Larger(WrapHeight(dc, kModernNote, noteW), WrapHeight(dc, kAuthenticNote, noteW)) + 2;

        SelectObject(dc, bold ? bold : UiFont());
        int headingH = boldH;
        if (headingLine > contentW) {
            headingH = WrapHeight(dc, kWelcomeHeading, contentW) + 2;
        }
        const int trainerH = boldH;
        SelectObject(dc, UiFont());
        int introH = WrapHeight(dc, kWelcomeIntro, contentW) + 2;
        if (introH < fontH) {
            introH = fontH;
        }

        const int optionH = rowH + noteH + imageGap + imageH;
        int y = pad;
        const int headingY = y;
        y += headingH + pad / 2;
        const int introY = y;
        y += introH + pad;
        const int optionY = y;
        int colY0 = optionY;
        int colY1 = optionY;
        if (stacked) {
            colY1 = optionY + optionH + pad;
            y = colY1 + optionH + pad;
        } else {
            y = optionY + optionH + pad;
        }
        const int trainersY = y;
        y += trainerH + 4;
        const int checksY = y;
        y += rowH * 2;
        y += pad;
        SelectObject(dc, large ? large : (bold ? bold : UiFont()));
        const int textW = TextWidth(dc, kPadAdvice);
        int adviceW = textW + pad * 2;
        int adviceH = 0;
        if (adviceW > contentW) {
            adviceW = contentW;
            int innerW = contentW - pad;
            if (innerW < 20) {
                innerW = 20;
            }
            adviceH = WrapHeight(dc, kPadAdvice, innerW) + pad;
        } else {
            TEXTMETRICW largeTm{};
            GetTextMetricsW(dc, &largeTm);
            const int largeH = largeTm.tmHeight > 0 ? largeTm.tmHeight : fontH * 3 / 2;
            adviceH = largeH + pad;
        }
        SelectObject(dc, UiFont());
        if (adviceH < fontH) {
            adviceH = fontH;
        }
        const int adviceX = pad + (contentW - adviceW) / 2;
        const int adviceY = y;
        y += adviceH + pad;
        const int btnY = y;
        y += btnH + pad;

        int clientW = pad * 2 + contentW;
        if (clientW < btnW + pad * 2) {
            clientW = btnW + pad * 2;
        }
        const int clientH = y;

        bool retry = false;
        const int minImageH = fontH * 4;
        if (clientH > maxH && imageH > minImageH && attempt < 11) {
            const int slots = stacked ? 2 : 1;
            int reduce = (clientH - maxH) / slots + 1;
            int newH = imageH - reduce;
            if (newH < minImageH) {
                newH = minImageH;
            }
            int newW = (newH - 2) * kWidth / kHeight;
            if (newW < 8) {
                newW = 8;
            }
            if (newW > colW) {
                newW = colW;
            }
            if (newW < imageW) {
                imageW = newW;
                retry = true;
            }
        }

        layout.pad = pad;
        layout.imageGap = imageGap;
        layout.clientW = clientW;
        layout.clientH = clientH;
        layout.contentW = contentW;
        layout.colW = colW;
        layout.imageW = imageW;
        layout.imageH = imageH;
        layout.radioH = rowH;
        layout.noteH = noteH;
        layout.noteW = noteW;
        layout.afterBox = afterBox;
        layout.stacked = stacked;
        layout.headingY = headingY;
        layout.headingH = headingH;
        layout.introY = introY;
        layout.introH = introH;
        layout.colX[0] = pad;
        layout.colY[0] = colY0;
        layout.colX[1] = stacked ? pad : pad + colW + gap;
        layout.colY[1] = colY1;
        layout.trainersY = trainersY;
        layout.trainersH = trainerH;
        layout.checksY = checksY;
        layout.checksW = contentW;
        layout.rowH = rowH;
        layout.adviceX = adviceX;
        layout.adviceY = adviceY;
        layout.adviceW = adviceW;
        layout.adviceH = adviceH;
        layout.btnW = btnW;
        layout.btnH = btnH;
        layout.btnX = (clientW - btnW) / 2;
        layout.btnY = btnY;
        if (!retry) {
            break;
        }
    }
    SelectObject(dc, old);
    return layout;
}

void PaintAdvice(const DRAWITEMSTRUCT* draw) {
    RECT rc = draw->rcItem;
    FillRect(draw->hDC, &rc, gTheme.windowBrush);
    HBRUSH fill = CreateSolidBrush(gTheme.boxChecked);
    HPEN pen = CreatePen(PS_SOLID, 1, gTheme.boxChecked);
    const HGDIOBJ oldBrush = SelectObject(draw->hDC, fill);
    const HGDIOBJ oldPen = SelectObject(draw->hDC, pen);
    const int height = rc.bottom - rc.top;
    const int radius = height > 4 ? height / 2 : 2;
    RoundRect(draw->hDC, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(draw->hDC, oldBrush);
    SelectObject(draw->hDC, oldPen);
    DeleteObject(fill);
    DeleteObject(pen);

    int inset = FontHeight() / 2;
    if (inset < 8) {
        inset = 8;
    }
    RECT text = rc;
    InflateRect(&text, -inset, 0);
    if (text.right < text.left) {
        text = rc;
    }
    const HFONT font = gWizardLarge ? gWizardLarge : UiFont();
    const HFONT oldFont = static_cast<HFONT>(SelectObject(draw->hDC, font));
    RECT measure = text;
    DrawTextW(draw->hDC, kPadAdvice, -1, &measure, DT_CALCRECT | DT_WORDBREAK | DT_CENTER | DT_NOPREFIX);
    const int textH = measure.bottom - measure.top;
    const int extra = (rc.bottom - rc.top) - textH;
    if (extra > 0) {
        text.top += extra / 2;
    }
    text.bottom = text.top + textH;
    SetBkMode(draw->hDC, TRANSPARENT);
    SetTextColor(draw->hDC, gTheme.checkMark);
    DrawTextW(draw->hDC, kPadAdvice, -1, &text, DT_WORDBREAK | DT_CENTER | DT_NOPREFIX);
    SelectObject(draw->hDC, oldFont);
}

LRESULT CALLBACK WizardProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND: {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        FillRect(reinterpret_cast<HDC>(wp), &rc, gTheme.windowBrush);
        return 1;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        return OnCtlColor(wp, lp, false);
    case WM_DRAWITEM: {
        const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lp);
        const int id = static_cast<int>(GetDlgCtrlID(draw->hwndItem));
        if (id == IDOK) {
            PaintPush(draw);
        } else if (id == IDC_WIZ_ADVICE) {
            PaintAdvice(draw);
        } else if (id == IDC_WIZ_MODERN || id == IDC_WIZ_AUTHENTIC) {
            PaintRadio(draw);
        } else {
            PaintCheck(draw);
        }
        return TRUE;
    }
    case WM_COMMAND: {
        const int id = LOWORD(wp);
        const int code = HIWORD(wp);
        if (code != BN_CLICKED) {
            return 0;
        }
        if (id == IDC_WIZ_MODERN || id == IDC_WIZ_MODERN_PIC) {
            SelectWizard(hwnd, true);
            SetFocus(GetDlgItem(hwnd, IDC_WIZ_MODERN));
            return 0;
        }
        if (id == IDC_WIZ_AUTHENTIC || id == IDC_WIZ_AUTHENTIC_PIC) {
            SelectWizard(hwnd, false);
            SetFocus(GetDlgItem(hwnd, IDC_WIZ_AUTHENTIC));
            return 0;
        }
        if (id == IDC_LIVES) {
            gConfig.infiniteLives = !gConfig.infiniteLives;
            InvalidateRect(reinterpret_cast<HWND>(lp), nullptr, FALSE);
            return 0;
        }
        if (id == IDC_ENEMIES) {
            gConfig.enemiesCantHitBruce = !gConfig.enemiesCantHitBruce;
            InvalidateRect(reinterpret_cast<HWND>(lp), nullptr, FALSE);
            return 0;
        }
        if (id == IDOK) {
            DestroyWindow(hwnd);
        }
        return 0;
    }
    case WM_SETTINGCHANGE:
    case WM_SYSCOLORCHANGE:
    case WM_THEMECHANGED:
        RebuildTheme();
        ApplyFrameTheme(hwnd);
        RefreshWizardShots(hwnd);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        FinishWizard();
        return 0;
    case WM_NCDESTROY:
        ReleaseWizardPictures();
        ReleaseWizardFont();
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

void RegisterWizardClass() {
    static bool ready = false;
    if (ready) {
        return;
    }
    WNDCLASSW wc{};
    wc.lpfnWndProc = WizardProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    wc.lpszClassName = L"BruceLeeWelcome";
    RegisterClassW(&wc);
    ready = true;
}

bool ShowIntroWizard(HWND owner) {
    if (!gConfig.firstTimeRun || !owner) {
        return true;
    }
    gWizardModern = true;
    gWizardSave = true;
    gWizardDone = false;
    gWizardShotW = 0;
    gWizardShotH = 0;
    ReleaseWizardPictures();
    ReleaseWizardFont();
    RegisterWizardClass();
    RebuildTheme();
    gWizardBold = MakeBoldFont();
    gWizardLarge = MakeLargeFont();

    HMONITOR monitor = MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    RECT work{0, 0, 1280, 800};
    if (GetMonitorInfoW(monitor, &info)) {
        work = info.rcWork;
    }
    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    const DWORD ex = WS_EX_DLGMODALFRAME;
    RECT frame{0, 0, 0, 0};
    AdjustWindowRectEx(&frame, style, FALSE, ex);
    const int maxW = (work.right - work.left) - (frame.right - frame.left) - 8;
    const int maxH = (work.bottom - work.top) - (frame.bottom - frame.top) - 8;

    HDC dc = GetDC(owner);
    const WizardLayout layout = LayoutWizard(dc, gWizardBold, gWizardLarge, maxW, maxH);
    ReleaseDC(owner, dc);

    gWizardShotW = layout.imageW;
    gWizardShotH = layout.imageH;
    if (!ComposeWelcomeRoom(gWizardSource)) {
        gWizardSource = g.pixels;
    }
    gWizardModernBmp = MakeExperienceBitmap(false, layout.imageW, layout.imageH);
    gWizardAuthenticBmp = MakeExperienceBitmap(true, layout.imageW, layout.imageH);

    RECT rc{0, 0, layout.clientW, layout.clientH};
    AdjustWindowRectEx(&rc, style, FALSE, ex);
    const HINSTANCE inst = GetModuleHandleW(nullptr);
    // Unowned, so a hidden game window does not keep the wizard off the
    // taskbar and behind whatever was already open.
    HWND dlg = CreateWindowExW(ex, L"BruceLeeWelcome", L"Welcome", style, 0, 0, rc.right - rc.left,
                               rc.bottom - rc.top, nullptr, nullptr, inst, nullptr);
    if (!dlg) {
        ReleaseWizardPictures();
        ReleaseWizardFont();
        return true;
    }
    ApplyFrameTheme(dlg);
    const HFONT headingFont = gWizardBold ? gWizardBold : UiFont();
    AddLabel(dlg, kWelcomeHeading, SS_NOPREFIX, layout.pad, layout.headingY, layout.contentW, layout.headingH,
             headingFont, 0);
    AddLabel(dlg, kWelcomeIntro, SS_NOPREFIX, layout.pad, layout.introY, layout.contentW, layout.introH, UiFont(),
             0);

    const wchar_t* titles[] = {kModernExperience, kAuthenticExperience};
    const wchar_t* notes[] = {kModernNote, kAuthenticNote};
    const int radioIds[] = {IDC_WIZ_MODERN, IDC_WIZ_AUTHENTIC};
    const int pictureIds[] = {IDC_WIZ_MODERN_PIC, IDC_WIZ_AUTHENTIC_PIC};
    HBITMAP pictures[] = {gWizardModernBmp, gWizardAuthenticBmp};
    HWND modernRadio = nullptr;
    for (int i = 0; i < 2; ++i) {
        HWND radio = CreateWindowExW(0, L"BUTTON", titles[i],
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                     layout.colX[i], layout.colY[i], layout.colW, layout.radioH, dlg,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(radioIds[i])), inst, nullptr);
        SetWindowSubclass(radio, WizardRadioProc, 1, 0);
        if (i == 0) {
            modernRadio = radio;
        }
        AddLabel(dlg, notes[i], SS_NOPREFIX, layout.colX[i] + layout.afterBox, layout.colY[i] + layout.radioH,
                 layout.noteW, layout.noteH, UiFont(), IDC_WIZ_NOTE);
        if (pictures[i]) {
            const int x = layout.colX[i] + (layout.colW - layout.imageW) / 2;
            const int y = layout.colY[i] + layout.radioH + layout.noteH + layout.imageGap;
            HWND picture = CreateWindowExW(0, L"STATIC", nullptr,
                                           WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_NOTIFY, x, y, layout.imageW,
                                           layout.imageH, dlg,
                                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(pictureIds[i])), inst,
                                           nullptr);
            SendMessageW(picture, STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(pictures[i]));
        }
    }
    AddLabel(dlg, kTrainersHeading, SS_NOPREFIX, layout.pad, layout.trainersY, layout.contentW, layout.trainersH,
             headingFont, 0);
    MakeCheck(dlg, inst, IDC_LIVES, L"Infinite Lives", layout.pad, layout.checksY, layout.checksW, layout.rowH);
    MakeCheck(dlg, inst, IDC_ENEMIES, L"Enemies cannot hit Bruce", layout.pad, layout.checksY + layout.rowH,
              layout.checksW, layout.rowH);
    HWND advice = CreateWindowExW(0, L"STATIC", kPadAdvice, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                                  layout.adviceX, layout.adviceY, layout.adviceW, layout.adviceH, dlg,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_WIZ_ADVICE)), inst, nullptr);
    SetWindowSubclass(advice, CheckEraseProc, 1, 0);
    HWND ok = CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                              layout.btnX, layout.btnY, layout.btnW, layout.btnH, dlg,
                              reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), inst, nullptr);
    SetWindowSubclass(ok, CheckEraseProc, 1, 0);

    SelectWizard(dlg, true);
    PlaceOnOwner(dlg, owner);
    ShowWindow(dlg, SW_SHOWNORMAL);
    SetWindowPos(dlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetWindowPos(dlg, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(dlg);
    SetFocus(modernRadio ? modernRadio : ok);
    const bool okRun = RunModal(dlg, owner);
    if (IsWindow(dlg)) {
        if (!okRun) {
            gWizardSave = false;
        }
        DestroyWindow(dlg);
    }
    return okRun;
}

void ShowKeyboardShortcuts(HWND owner) {
    static constexpr HelpLine lines[] = {
        {nullptr, L"Playing"},
        {L"Left / Right", L"Player 1 walks."},
        {L"Up", L"Player 1 jumps, or climbs a ladder. With Left or Right held, jump sideways."},
        {L"Down", L"Player 1 ducks. On a ladder, climb down."},
        {L"Right Ctrl", L"Player 1 punches while standing. Flying kick while moving."},
        {L"W A S D", L"Player 2 walks, jumps, and ducks. Yamo, when the options page is set to Opponent. W is up, A is left, S is down, and D is right."},
        {L"Left Ctrl", L"Player 2 punches while standing. Flying kick while moving."},
        {L"Space", L"Pause during play. Press again to resume."},
        {L"V",
         L"Show the credit line on the top bar: BRUCE LEE (C) 1984 DATASOFT REV.2 BY RJF. Press again for the black bar. Entering a room brings the black bar back."},
        {L"S",
         L"During play, switch the options tune off or on. Nothing changes until the next options page: it is silent, or the tune starts as soon as that page opens. Press again to switch it back."},
        {L"F1",
         L"Return to the options page. The score, the falls, and the rooms start over. The two option choices stay."},
        {nullptr, L"Title and options"},
        {L"Space or Enter", L"Leave the title picture."},
        {L"F3", L"On the options page, switch 1 player / 2 players."},
        {L"F5", L"On the options page, switch computer / opponent."},
        {L"F7", L"Leave the options page."},
        {nullptr, L"Window"},
        {L"Escape", L"Leave full screen, and close Settings if it is open."},
        {L"F2", L"Open the game manual."},
        {L"Ctrl+F1", L"About."},
        {L"Ctrl+,", L"Settings."},
        {L"F12", L"Save a picture of the window as the next free ScreenShotXX.png, in the folder the config file was read from. A camera flash shows in the upper right."},
        {L"Alt+F4", L"Exit."},
    };
    ShowHelp(owner, L"Keyboard shortcuts", L"Key", lines,
             static_cast<int>(sizeof(lines) / sizeof(lines[0])), nullptr);
}

void ShowControllerMapping(HWND owner) {
    static constexpr HelpLine lines[] = {
        {L"A",
         L"Leave the title picture. Leave the options page. Punch while standing. Flying kick while moving."},
        {L"Left stick",
         L"Left and right walk. Up jumps. Up with left or right jumps sideways. Down ducks. On a ladder, up and down climb."},
        {L"D-pad", L"Same as the left stick."},
        {L"Menu (Start)", L"Pause. Press again to resume."},
        {L"View (Back)", L"Open Settings. Press again to close it."},
        {L"Y",
         L"While paused, hold for 2 seconds to return to the options page. A bar under PAUSED fills while Y is held."},
    };
    static constexpr wchar_t note[] =
        L"An Xbox pad uses these names. Menu is the button to the right of the Xbox button, "
        L"and View is the button to the left. The other buttons are not used. A stick with "
        L"no button names, and a controller that only shows in the Windows Game Controllers "
        L"list, uses its stick or hat to move and its buttons to punch. Settings chooses "
        L"a controller for each player. Player 1 starts on the first one, and player 2 "
        L"on the second. Yamo uses player 2 when the options page is set to Opponent. "
        L"Either controller's A button leaves the title and the options page, and either "
        L"Menu, View, or Y button does what that button does above. The keyboard keeps "
        L"working. A controller is read only while this program is in front.";
    ShowHelp(owner, L"Controller mapping", L"Control", lines,
             static_cast<int>(sizeof(lines) / sizeof(lines[0])), note);
}

HFONT MenuFont() {
    static HFONT font = nullptr;
    if (font) {
        return font;
    }
    NONCLIENTMETRICS metrics{};
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
        font = CreateFontIndirectW(&metrics.lfMenuFont);
    }
    if (!font) {
        font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }
    return font;
}

bool MenuBarDark() {
    return gTheme.dark && !gTheme.highContrast;
}

bool OnMenuMeasure(LPARAM lp) {
    auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lp);
    if (measure->CtlType != ODT_MENU) {
        return false;
    }
    const UINT side = static_cast<UINT>(GetSystemMetrics(SM_CYMENU));
    const int height = side > 4 ? static_cast<int>(side) - 2 : static_cast<int>(side);
    if (measure->itemID == ID_SETTINGS) {
        measure->itemWidth = side;
        measure->itemHeight = static_cast<UINT>(height);
        return true;
    }
    const wchar_t* label = reinterpret_cast<const wchar_t*>(measure->itemData);
    if (!label || label[0] == L'\0') {
        return false;
    }
    HDC dc = GetDC(nullptr);
    const HFONT old = static_cast<HFONT>(SelectObject(dc, MenuFont()));
    RECT rc{0, 0, 0, 0};
    DrawTextW(dc, label, -1, &rc, DT_CALCRECT | DT_SINGLELINE | DT_HIDEPREFIX);
    SelectObject(dc, old);
    ReleaseDC(nullptr, dc);
    measure->itemWidth = static_cast<UINT>((rc.right > 0 ? rc.right : 24) + 16);
    measure->itemHeight = static_cast<UINT>(height);
    return true;
}

#ifndef OBJID_MENU
#define OBJID_MENU ((LONG)0xFFFFFFFD)
#endif

void MenuBarColors(bool hot, COLORREF& bg, COLORREF& fg) {
    if (MenuBarDark()) {
        bg = hot ? RGB(55, 55, 55) : gTheme.window;
        fg = RGB(255, 255, 255);
        return;
    }
    bg = GetSysColor(hot ? COLOR_HIGHLIGHT : COLOR_MENU);
    fg = GetSysColor(hot ? COLOR_HIGHLIGHTTEXT : COLOR_MENUTEXT);
}

bool LooksLikeDc(HDC dc) {
    if (!dc) {
        return false;
    }
    const DWORD kind = GetObjectType(dc);
    return kind == OBJ_DC || kind == OBJ_MEMDC;
}

RECT WindowDcRect(HWND hwnd, RECT screen) {
    RECT window{};
    GetWindowRect(hwnd, &window);
    OffsetRect(&screen, -window.left, -window.top);
    return screen;
}

bool MenuBarScreenRect(HWND hwnd, RECT& screen) {
    MENUBARINFO bar{};
    bar.cbSize = sizeof(bar);
    if (!GetMenuBarInfo(hwnd, OBJID_MENU, 0, &bar)) {
        return false;
    }
    screen = bar.rcBar;
    return screen.right > screen.left && screen.bottom > screen.top;
}

// The themed menu bar paints its background through WM_UAHDRAWMENU. Its DC is
// clipped to the bar, so fill that clip rather than a window-relative rect.
bool FillUahBar(HWND hwnd, LPARAM lp) {
    struct UahMenu {
        HMENU hmenu;
        HDC hdc;
        DWORD dwFlags;
    };
    const auto* um = reinterpret_cast<const UahMenu*>(lp);
    if (!um || !LooksLikeDc(um->hdc)) {
        return false;
    }
    RECT clip{};
    if (GetClipBox(um->hdc, &clip) == ERROR) {
        return false;
    }
    RECT target{};
    bool have = false;
    const int clipH = clip.bottom - clip.top;
    const int clipW = clip.right - clip.left;
    if (clipH > 0 && clipW > 0 && clipH <= 160 && clipW < 20000) {
        target = clip;
        have = true;
    }
    RECT screen{};
    if (MenuBarScreenRect(hwnd, screen)) {
        const RECT mapped = WindowDcRect(hwnd, screen);
        RECT inter{};
        if (IntersectRect(&inter, &mapped, &clip)) {
            target = mapped;
            have = true;
        }
    }
    if (!have) {
        return false;
    }
    HBRUSH brush = CreateSolidBrush(gTheme.window);
    FillRect(um->hdc, &target, brush);
    DeleteObject(brush);
    return true;
}

// Owner-drawn captions already paint their own cells. Darken only the rest of
// the bar, which the theme leaves white.
void FillMenuBarGap(HWND hwnd) {
    RECT screen{};
    if (!MenuBarScreenRect(hwnd, screen)) {
        return;
    }
    HDC dc = GetWindowDC(hwnd);
    if (!dc) {
        return;
    }
    const RECT bar = WindowDcRect(hwnd, screen);
    HRGN gap = CreateRectRgnIndirect(&bar);
    HMENU menu = GetMenu(hwnd);
    const int count = menu ? GetMenuItemCount(menu) : 0;
    for (int i = 0; i < count; ++i) {
        RECT itemScreen{};
        if (!GetMenuItemRect(hwnd, menu, i, &itemScreen)) {
            continue;
        }
        RECT item = WindowDcRect(hwnd, itemScreen);
        item.top = bar.top;
        item.bottom = bar.bottom;
        HRGN piece = CreateRectRgnIndirect(&item);
        CombineRgn(gap, gap, piece, RGN_DIFF);
        DeleteObject(piece);
    }
    HBRUSH brush = CreateSolidBrush(gTheme.window);
    FillRgn(dc, gap, brush);
    DeleteObject(gap);
    // The themed bar draws a light etched line just under itself, above the client.
    RECT client{};
    GetClientRect(hwnd, &client);
    MapWindowPoints(hwnd, nullptr, reinterpret_cast<LPPOINT>(&client), 2);
    const RECT clientDc = WindowDcRect(hwnd, client);
    if (clientDc.top > bar.bottom) {
        RECT seam{bar.left, bar.bottom, bar.right, clientDc.top};
        FillRect(dc, &seam, brush);
    }
    DeleteObject(brush);
    ReleaseDC(hwnd, dc);
}

bool PaintDarkMenuBar(HWND hwnd, LPARAM lp) {
    static bool painting = false;
    if (painting || !MenuBarDark() || !hwnd || !GetMenu(hwnd)) {
        return false;
    }
    painting = true;
    bool uah = false;
    if (lp) {
        uah = FillUahBar(hwnd, lp);
    }
    FillMenuBarGap(hwnd);
    painting = false;
    return uah;
}

bool OnMenuDraw(LPARAM lp) {
    const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lp);
    if (draw->CtlType != ODT_MENU) {
        return false;
    }
    const bool gear = draw->itemID == ID_SETTINGS;
    const wchar_t* label = gear ? nullptr : reinterpret_cast<const wchar_t*>(draw->itemData);
    if (!gear && (label == nullptr || label[0] == L'\0')) {
        return false;
    }
    const bool hot = (draw->itemState & (ODS_SELECTED | ODS_HOTLIGHT)) != 0;
    COLORREF bg = 0;
    COLORREF fg = 0;
    MenuBarColors(hot, bg, fg);
    HBRUSH brush = CreateSolidBrush(bg);
    FillRect(draw->hDC, &draw->rcItem, brush);
    DeleteObject(brush);
    const HFONT old = static_cast<HFONT>(SelectObject(draw->hDC, gear ? GearFont() : MenuFont()));
    SetBkMode(draw->hDC, TRANSPARENT);
    SetTextColor(draw->hDC, fg);
    RECT rc = draw->rcItem;
    const UINT format = DT_CENTER | DT_VCENTER | DT_SINGLELINE |
                        ((draw->itemState & ODS_NOACCEL) != 0 ? DT_HIDEPREFIX : 0);
    DrawTextW(draw->hDC, gear ? L"\uE713" : label, -1, &rc, format);
    SelectObject(draw->hDC, old);
    const HWND owner = WindowFromDC(draw->hDC);
    if (owner) {
        PaintDarkMenuBar(owner, 0);
    }
    return true;
}

void HideGearTip() {
    if (!gTip || !IsWindow(gTip)) {
        gTip = nullptr;
        return;
    }
    TOOLINFOW info{};
    info.cbSize = sizeof(info);
    info.hwnd = GetParent(gTip);
    info.uId = 1;
    SendMessageW(gTip, TTM_TRACKACTIVATE, FALSE, reinterpret_cast<LPARAM>(&info));
}

void OnMenuSelect(HWND hwnd, WPARAM wp, LPARAM lp) {
    const UINT flags = HIWORD(wp);
    const UINT id = LOWORD(wp);
    if (lp == 0 || flags == 0xFFFF) {
        HideGearTip();
        return;
    }
    if (id != ID_SETTINGS || (flags & MF_POPUP) != 0) {
        HideGearTip();
        return;
    }
    EnsureTip(hwnd);
    if (!gTip) {
        return;
    }
    HMENU menu = GetMenu(hwnd);
    const int count = GetMenuItemCount(menu);
    RECT rc{};
    if (count <= 0 || !GetMenuItemRect(hwnd, menu, count - 1, &rc)) {
        return;
    }
    TOOLINFOW info{};
    info.cbSize = sizeof(info);
    info.hwnd = hwnd;
    info.uId = 1;
    SendMessageW(gTip, TTM_TRACKPOSITION, 0, MAKELPARAM(rc.left, rc.bottom + 2));
    SendMessageW(gTip, TTM_TRACKACTIVATE, TRUE, reinterpret_cast<LPARAM>(&info));
}

void ReleaseUi() {
    HideGearTip();
    if (gTip && IsWindow(gTip)) {
        DestroyWindow(gTip);
    }
    gTip = nullptr;
    if (gGearFont) {
        DeleteObject(gGearFont);
        gGearFont = nullptr;
    }
    if (gUiFont) {
        DeleteObject(gUiFont);
        gUiFont = nullptr;
    }
    ClearThemeBrushes();
}
