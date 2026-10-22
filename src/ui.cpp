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
#include "load.h"
#include "room.h"

#include <cstdint>

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
HWND gChkSound = nullptr;
HFONT gUiFont = nullptr;
HFONT gGearFont = nullptr;
HWND gTip = nullptr;
bool gRefreshing = false;
bool gRelayout = false;
HBITMAP gAboutBitmap = nullptr;
int gBuiltFont = 0;
bool gBuiltDark = false;
bool gSettingsReady = false;

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

void PaintCheck(const DRAWITEMSTRUCT* draw) {
    RECT rc = draw->rcItem;
    FillRect(draw->hDC, &rc, gTheme.windowBrush);
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
    RECT boxRc{rc.left + kFocusPad, top, rc.left + kFocusPad + box, top + box};
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
    wchar_t label[128]{};
    const int count = GetWindowTextW(draw->hwndItem, label, 128);
    RECT text = rc;
    text.left = boxRc.right + (box >= 18 ? 10 : 8);
    SetBkMode(draw->hDC, TRANSPARENT);
    SetTextColor(draw->hDC, gTheme.text);
    const HFONT oldFont = static_cast<HFONT>(SelectObject(draw->hDC, UiFont()));
    DrawTextW(draw->hDC, label, count, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if ((draw->itemState & ODS_FOCUS) != 0) {
        SIZE size{};
        GetTextExtentPoint32W(draw->hDC, label, count, &size);
        int right = text.left + static_cast<int>(size.cx) + 6;
        if (right > rc.right - 1) {
            right = rc.right - 1;
        }
        RECT focus{rc.left + 1, rc.top + 1, right, rc.bottom - 1};
        DrawFocusRing(draw->hDC, focus);
    }
    SelectObject(draw->hDC, oldFont);
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
    SetTextColor(dc, id == IDC_HINT ? gTheme.muted : gTheme.text);
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

bool CheckValue(int id) {
    switch (id) {
    case IDC_LIVES:
        return gConfig.infiniteLives;
    case IDC_ENEMIES:
        return gConfig.enemiesCantHitYou;
    case IDC_TITLE:
        return gConfig.showTitleScreen;
    case IDC_FIX:
        return gConfig.fixBugsInOriginal;
    case IDC_FULL:
        return gConfig.fullScreen;
    case IDC_CRT:
        return gConfig.showCrtFilter;
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
    gChkSound = nullptr;
    gRefreshing = false;
}

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
        L"Enemies cannot hit you",
        L"Show Title Screen",
        L"Fix Bugs in Original",
        L"Full Screen",
        L"Show CRT Filter",
        L"Sound",
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
    const wchar_t* hintText = L"Start room and title screen apply on Game Over or Restart.";
    const int hintNatural = TextWidth(dc, hintText);
    const int em = TextWidth(dc, L"M");
    int inner = afterBox + labelW;
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
    const int hintW = inner - afterBox;
    const int hintH = WrapHeight(dc, hintText, hintW > 20 ? hintW : 20);
    SelectObject(dc, old);
    ReleaseDC(hwnd, dc);

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
    gChkSound = MakeCheck(hwnd, inst, IDC_SOUND, checks[6], x, y, inner, rowH);
    y += rowH + pad;

    gSettingsClientW = pad + inner + pad;
    gSettingsClientH = y;
    gBuiltFont = fontH;
    gBuiltDark = gTheme.dark;
}

void FitSettingsWindow(HWND hwnd) {
    RECT rc{0, 0, gSettingsClientW, gSettingsClientH};
    AdjustWindowRectEx(&rc, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_TOOLWINDOW);
    SetWindowPos(hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
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
        gConfig.enemiesCantHitYou = !gConfig.enemiesCantHitYou;
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
        // Stored for later. Paint does not filter the picture yet.
        gConfig.showCrtFilter = !gConfig.showCrtFilter;
        InvalidateCheck(gChkCrt);
        break;
    case IDC_FULL:
        SendMessageW(owner, WM_APP_FULLSCREEN, gConfig.fullScreen ? 0 : 1, 0);
        break;
    case IDC_SOUND:
        SendMessageW(owner, WM_APP_SOUND, gConfig.sound ? 0 : 1, 0);
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
        HWND owner = GetWindow(hwnd, GW_OWNER);
        if (owner) {
            PlaceOnOwner(hwnd, owner);
        }
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
    case WM_DRAWITEM:
        PaintCheck(reinterpret_cast<const DRAWITEMSTRUCT*>(lp));
        return TRUE;
    case WM_COMMAND:
        OnSettingCommand(hwnd, LOWORD(wp), HIWORD(wp));
        return 0;
    case WM_SETTINGCHANGE:
    case WM_SYSCOLORCHANGE:
    case WM_THEMECHANGED:
        if (gSettingsReady) {
            RelayoutSettings(hwnd);
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
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

}  // namespace

HMENU CreateAppMenu() {
    EnsureUiClasses();
    HMENU bar = CreateMenu();
    HMENU file = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, ID_RESTART, L"&Restart Game\tCtrl+R");
    AppendMenuW(file, MF_STRING | MF_GRAYED, ID_SAVE, L"&Save game...\tCtrl+S");
    AppendMenuW(file, MF_STRING | MF_GRAYED, ID_LOAD, L"&Load game...\tCtrl+O");
    AppendMenuW(file, MF_STRING | MF_GRAYED, ID_QUICKSAVE, L"&Quick save\tF6");
    AppendMenuW(file, MF_STRING | MF_GRAYED, ID_QUICKLOAD, L"Quick &Load\tF9");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, ID_EXIT, L"E&xit\tAlt+F4");
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"&File");

    HMENU about = CreatePopupMenu();
    AppendMenuW(about, MF_STRING, ID_MANUAL, L"Game &Manual\tF1");
    AppendMenuW(about, MF_STRING, ID_ABOUT, L"&About Bruce Lee...\tCtrl+F1");
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
        {FCONTROL | FVIRTKEY, 'R', ID_RESTART},
        {FVIRTKEY, VK_F1, ID_MANUAL},
        {FCONTROL | FVIRTKEY, VK_F1, ID_ABOUT},
        {FCONTROL | FVIRTKEY, VK_OEM_COMMA, ID_SETTINGS},
    };
    return CreateAcceleratorTableW(keys, 4);
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
    InvalidateCheck(gChkSound);
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
    PlaceOnOwner(hwnd, owner);
    gSettingsReady = true;
    ShowWindow(hwnd, SW_SHOW);
    SetFocus(gChkLives);
}

void CloseSettings() {
    if (gSettings) {
        DestroyWindow(gSettings);
        gSettings = nullptr;
    }
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
    EnableWindow(owner, FALSE);

    MSG msg{};
    for (;;) {
        if (!IsWindow(dlg)) {
            break;
        }
        const BOOL got = GetMessageW(&msg, nullptr, 0, 0);
        if (got == 0) {
            PostQuitMessage(static_cast<int>(msg.wParam));
            break;
        }
        if (got == -1) {
            break;
        }
        if (msg.message == WM_KEYDOWN &&
            (msg.wParam == VK_ESCAPE || msg.wParam == VK_RETURN) &&
            (msg.hwnd == dlg || IsChild(dlg, msg.hwnd))) {
            DestroyWindow(dlg);
            continue;
        }
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
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
