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
#include <dwmapi.h>
#include <d2d1.h>
#include <d2d1helper.h>

#include "bruce.h"
#include "config.h"
#include "crt.h"
#include "draw.h"
#include "fighter.h"
#include "front.h"
#include "game.h"
#include "load.h"
#include "pad.h"
#include "room.h"
#include "sound.h"
#include "ui.h"

#include <cmath>
#include <cwchar>
#include <vector>

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwmapi.lib")

namespace {

constexpr D2D1_COLOR_F kBorderF{0.f, 0.f, 0.f, 1.f};

ID2D1Factory* gFactory = nullptr;
ID2D1HwndRenderTarget* gTarget = nullptr;
ID2D1Bitmap* gBitmap = nullptr;
// The filtered picture is already at window size, so this bitmap changes
// when the fitted rectangle does.
ID2D1Bitmap* gCrtBitmap = nullptr;
std::vector<uint32_t> gCrtPixels;
UINT gCrtWidth = 0;
UINT gCrtHeight = 0;

// Title picture, then the options page, then between_rooms, then play.
// GameOver is $188E, after the last fall.
enum class Stage { Title, Menu, Player, Play, GameOver };
Stage gStage = Stage::Play;
bool gTitleStart = false;
bool gMenuStart = false;
bool gTwoPlayers = false;     // $42C9. F3. Clear is 1 PLAYER.
bool gHumanOpponent = false;  // $42CA. F5. Clear is COMPUTER.
bool gPrevF3 = false;
bool gPrevF5 = false;
// between_rooms counts $48 from $40, or $80 when two players are selected.
// This loop sleeps three PAL frames. 21 passes is 63 of the 64. 43 is 129
// of the 128.
int gPlayerFrames = 0;
// A fall, rather than the opening page. The countdown then returns to the
// room instead of starting a new game.
bool gAfterFall = false;
// game_over waits $FE raster counts, then $40 more. 254 + 64 PAL frames.
// This loop sleeps three of those, so 106 passes is the same 318.
constexpr int kGameOverFrames = 106;
int gGameOverFrames = 0;

void ReleaseTarget() {
    if (gCrtBitmap) {
        gCrtBitmap->Release();
        gCrtBitmap = nullptr;
    }
    gCrtWidth = 0;
    gCrtHeight = 0;
    if (gBitmap) {
        gBitmap->Release();
        gBitmap = nullptr;
    }
    if (gTarget) {
        gTarget->Release();
        gTarget = nullptr;
    }
}

bool EnsureCrtBitmap(UINT width, UINT height) {
    if (gCrtBitmap != nullptr && gCrtWidth == width && gCrtHeight == height) {
        return true;
    }
    if (gCrtBitmap != nullptr) {
        gCrtBitmap->Release();
        gCrtBitmap = nullptr;
    }
    gCrtWidth = 0;
    gCrtHeight = 0;
    gCrtPixels.resize(static_cast<size_t>(width) * height);
    const HRESULT hr = gTarget->CreateBitmap(
        D2D1::SizeU(width, height),
        nullptr,
        width * static_cast<UINT32>(sizeof(uint32_t)),
        D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)),
        &gCrtBitmap);
    if (FAILED(hr) || gCrtBitmap == nullptr) {
        gCrtBitmap = nullptr;
        return false;
    }
    gCrtWidth = width;
    gCrtHeight = height;
    return true;
}

bool EnsureTarget(HWND hwnd) {
    if (gTarget) {
        return true;
    }
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const UINT w = static_cast<UINT>(rc.right - rc.left);
    const UINT h = static_cast<UINT>(rc.bottom - rc.top);
    const HRESULT hr = gFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(w, h)),
        &gTarget);
    if (FAILED(hr)) {
        return false;
    }
    gTarget->CreateBitmap(
        D2D1::SizeU(kWidth, kHeight),
        g.pixels.data(),
        kWidth * sizeof(uint32_t),
        D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)),
        &gBitmap);
    return gBitmap != nullptr;
}

void Paint(HWND hwnd) {
    if (!EnsureTarget(hwnd)) {
        return;
    }
    const D2D1_SIZE_F size = gTarget->GetSize();
    const float cw = size.width;
    const float ch = size.height;
    const float scale = (cw / kWidth < ch / kHeight) ? cw / kWidth : ch / kHeight;
    const float dw = kWidth * scale;
    const float dh = kHeight * scale;
    const D2D1_RECT_F sharp = D2D1::RectF((cw - dw) * 0.5f, (ch - dh) * 0.5f,
                                          (cw - dw) * 0.5f + dw, (ch - dh) * 0.5f + dh);

    // ShowCRTFilter resamples into the same fitted rectangle. The samples are
    // already window pixels, so the blit is 1:1. Off, and a filter with every
    // part turned off, keeps the nearest-neighbor scale of the 320 by 200 picture.
    const CrtOptions crtOptions{gConfig.crtCurve, gConfig.crtScanlines, gConfig.crtBlur, gConfig.crtVignette};
    const bool crtParts = crtOptions.curve || crtOptions.scanlines || crtOptions.blur || crtOptions.vignette;
    bool crt = false;
    D2D1_RECT_F crtDest{};
    if (gConfig.showCrtFilter && crtParts && dw >= 1.f && dh >= 1.f) {
        const UINT destW = static_cast<UINT>(dw);
        const UINT destH = static_cast<UINT>(dh);
        if (EnsureCrtBitmap(destW, destH)) {
            ApplyCrt(g.pixels.data(), kWidth, kHeight, gCrtPixels.data(),
                     static_cast<int>(destW), static_cast<int>(destH), crtOptions);
            gCrtBitmap->CopyFromMemory(nullptr, gCrtPixels.data(),
                                       destW * static_cast<UINT32>(sizeof(uint32_t)));
            const float ox = std::floor((cw - static_cast<float>(destW)) * 0.5f);
            const float oy = std::floor((ch - static_cast<float>(destH)) * 0.5f);
            crtDest = D2D1::RectF(ox, oy, ox + static_cast<float>(destW), oy + static_cast<float>(destH));
            crt = true;
        }
    }
    if (!crt) {
        gBitmap->CopyFromMemory(nullptr, g.pixels.data(), kWidth * sizeof(uint32_t));
    }

    gTarget->BeginDraw();
    gTarget->Clear(kBorderF);
    if (crt) {
        gTarget->DrawBitmap(gCrtBitmap, crtDest, 1.f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
    } else {
        gTarget->DrawBitmap(gBitmap, sharp, 1.f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
    }
    const HRESULT hr = gTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        ReleaseTarget();
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

// Borderless, the size of one monitor. The display mode is left alone.
// FullScreen in config.ini chooses the start. A double-click switches.
// The maximize button turns it on.
bool gFullScreen = false;
WINDOWPLACEMENT gWindowedPlace{};
LONG gWindowedStyle = 0;
LONG gWindowedExStyle = 0;
HMENU gMenu = nullptr;
HWND gHwnd = nullptr;
std::vector<uint8_t> gTitle;
int gShownRoom = -1;

bool MonitorBounds(HWND hwnd, RECT& bounds) {
    const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) {
        return false;
    }
    bounds = info.rcMonitor;
    return true;
}

void SetBorderlessFrame(HWND hwnd, bool borderless) {
    // Square corners and no caption edge, or the desktop shows in the corners.
    const DWM_WINDOW_CORNER_PREFERENCE corners = borderless ? DWMWCP_DONOTROUND : DWMWCP_DEFAULT;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
    const COLORREF border = borderless ? DWMWA_COLOR_NONE : DWMWA_COLOR_DEFAULT;
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &border, sizeof(border));
}

void RefitFullScreen(HWND hwnd) {
    if (!gFullScreen) {
        return;
    }
    RECT bounds{};
    if (!MonitorBounds(hwnd, bounds)) {
        return;
    }
    SetWindowPos(hwnd, HWND_TOP,
                 bounds.left, bounds.top,
                 bounds.right - bounds.left,
                 bounds.bottom - bounds.top,
                 SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_NOCOPYBITS);
}

void SetFullScreen(HWND hwnd, bool full) {
    if (full == gFullScreen) {
        return;
    }
    if (full) {
        WINDOWPLACEMENT place{};
        place.length = static_cast<UINT>(sizeof(place));
        RECT bounds{};
        if (!GetWindowPlacement(hwnd, &place) || !MonitorBounds(hwnd, bounds)) {
            RefreshSettingsDialog();
            return;
        }
        // A window created hidden reports SW_HIDE. Putting that back would hide it.
        if (place.showCmd == SW_HIDE) {
            place.showCmd = SW_SHOWNORMAL;
        }
        gWindowedPlace = place;
        gWindowedStyle = GetWindowLongW(hwnd, GWL_STYLE);
        gWindowedExStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
        const LONG visible = gWindowedStyle & WS_VISIBLE;
        SetWindowLongW(hwnd, GWL_STYLE, WS_POPUP | visible);
        SetWindowLongW(hwnd, GWL_EXSTYLE,
                       gWindowedExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                                            WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
        gFullScreen = true;
        SetBorderlessFrame(hwnd, true);
        SetMenu(hwnd, nullptr);
        RefitFullScreen(hwnd);
    } else {
        gFullScreen = false;
        SetBorderlessFrame(hwnd, false);
        if (gMenu) {
            SetMenu(hwnd, gMenu);
        }
        const LONG visible = GetWindowLongW(hwnd, GWL_STYLE) & WS_VISIBLE;
        SetWindowLongW(hwnd, GWL_STYLE, gWindowedStyle | visible);
        SetWindowLongW(hwnd, GWL_EXSTYLE, gWindowedExStyle);
        SetWindowPlacement(hwnd, &gWindowedPlace);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOCOPYBITS);
        DrawMenuBar(hwnd);
        ApplyAppTheme(hwnd);
    }
    gConfig.fullScreen = gFullScreen;
    RefreshSettingsDialog();
    InvalidateRect(hwnd, nullptr, FALSE);
}

void RestartGame(HWND hwnd);
void ApplySoundSetting(HWND hwnd, bool on);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_NCCALCSIZE:
        // Popup fullscreen has no caption. The client area is the whole window.
        if (gFullScreen && wp) {
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    case WM_SIZE:
        if (gTarget) {
            gTarget->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp)));
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_DISPLAYCHANGE:
        RefitFullScreen(hwnd);
        return 0;
    case WM_NCPAINT: {
        const LRESULT result = DefWindowProcW(hwnd, msg, wp, lp);
        // lp is a region here, not the menu-bar draw struct.
        PaintDarkMenuBar(hwnd, 0);
        return result;
    }
    case 0x0091:  // WM_UAHDRAWMENU. The themed menu bar asks for its background here.
        if (PaintDarkMenuBar(hwnd, lp)) {
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    case WM_SETTINGCHANGE:
    case WM_SYSCOLORCHANGE:
    case WM_THEMECHANGED:
        ApplyAppTheme(hwnd);
        return 0;
    case WM_LBUTTONDBLCLK:
        SetFullScreen(hwnd, !gFullScreen);
        return 0;
    case WM_NCLBUTTONDBLCLK:
        // Close and minimize keep their own clicks. Maximize enters fullscreen.
        if (wp == HTCLOSE || wp == HTMINBUTTON) {
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
        if (wp == HTMAXBUTTON) {
            SetFullScreen(hwnd, true);
            return 0;
        }
        SetFullScreen(hwnd, !gFullScreen);
        return 0;
    case WM_SYSCOMMAND:
        // The caption button, the system menu, and Win+Up all send this.
        // The low four bits are used by Windows and are not part of the command.
        if ((wp & 0xFFF0) == SC_MAXIMIZE) {
            SetFullScreen(hwnd, true);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd, &ps);
        Paint(hwnd);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_RESTART:
            RestartGame(hwnd);
            return 0;
        case ID_EXIT:
            DestroyWindow(hwnd);
            return 0;
        case ID_MANUAL:
            OpenManual(hwnd);
            return 0;
        case ID_ABOUT:
            ShowAbout(hwnd);
            return 0;
        case ID_SETTINGS:
            ToggleSettings(hwnd);
            return 0;
        default:
            break;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    case WM_APP_FULLSCREEN:
        SetFullScreen(hwnd, wp != 0);
        return 0;
    case WM_APP_SOUND:
        ApplySoundSetting(hwnd, wp != 0);
        return 0;
    case WM_MEASUREITEM:
        if (OnMenuMeasure(lp)) {
            return TRUE;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    case WM_DRAWITEM:
        if (OnMenuDraw(lp)) {
            return TRUE;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    case WM_MENUSELECT:
        OnMenuSelect(hwnd, wp, lp);
        return 0;
    case WM_EXITMENULOOP:
        HideGearTip();
        return 0;
    case WM_KEYDOWN:
        if (gStage == Stage::Title && (wp == VK_SPACE || wp == VK_RETURN)) {
            // The title picture leaves on the fire button. Space is that
            // button here. The picture stays until the key is up, so the
            // options page does not see the same press.
            gTitleStart = true;
        }
        return 0;
    case WM_DESTROY:
        // SetMenu(null) while full screen leaves the menu with us. An attached
        // menu is destroyed with the window.
        if (gMenu && GetMenu(hwnd) != gMenu) {
            DestroyMenu(gMenu);
        }
        gMenu = nullptr;
        SaveConfig();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

bool KeyDown(int vk) {
    if (UiOwnsKeyboard()) {
        return false;
    }
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

void ApplySoundSetting(HWND hwnd, bool on) {
    if (on) {
        gConfig.sound = true;
        if (!StartAudio()) {
            gConfig.sound = false;
            MessageBoxW(hwnd, L"Could not read data\\sound.bin.", L"Bruce Lee", MB_ICONERROR);
        } else if (gStage == Stage::Menu) {
            StartMenuMusic();
        }
    } else {
        gConfig.sound = false;
        StopAudio();
    }
    RefreshSettingsDialog();
}

void GiveStock() {
    // clear_work_ram writes 4 falls for the computer and 9 for a human opponent.
    g.lifeStock = gHumanOpponent ? 9 : 4;
    g.lives = g.lifeStock;
}

void BeginPlay() {
    GiveStock();
    gStage = Stage::Play;
    PlaceBruce();
    Compose();
}

void BeginMenu(bool fresh) {
    // $06DA. The options page starts the tune. The title picture stays silent.
    StartMenuMusic();
    if (fresh) {
        gTwoPlayers = false;
        gHumanOpponent = false;
    }
    gMenuStart = false;
    // A key already held when the page appears waits for a fresh press.
    gPrevF3 = KeyDown(VK_F3);
    gPrevF5 = KeyDown(VK_F5);
    gStage = Stage::Menu;
    ComposeMenu(gTwoPlayers, gHumanOpponent);
}

void ShowGameOver(HWND hwnd) {
    ClearOutOfLives();
    gGameOverFrames = kGameOverFrames;
    gStage = Stage::GameOver;
    ComposeGameOver(g.score);
    SetWindowTextW(hwnd, L"Bruce Lee");
}

// After the game-over wait. Score, falls, lamps, and the run counters start
// over. The top score stays. Play begins in the configured start room. When
// the title screen is on, the options page keeps the choices already made,
// the way $42CB and $42CC are copied back. The title picture is not shown.
void StartAfterGameOver(HWND hwnd) {
    ResetScore();
    ResetCollectedLamps();
    ResetRoomBonus();
    ResetRun();
    if (!EnterRoom(ClampedStartRoom())) {
        MessageBoxW(hwnd, L"Could not open the start room.", L"Bruce Lee", MB_ICONERROR);
        return;
    }
    if (gConfig.showTitleScreen) {
        BeginMenu(false);
        return;
    }
    GiveStock();
    PlaceBruce();
    Compose();
    gStage = Stage::Play;
}

// A new session with the settings as they are now. Start room and the title
// screen are read here. Top score stays, as it does after game over.
void RestartGame(HWND hwnd) {
    if (gConfig.showTitleScreen && gTitle.empty()) {
        gTitle = LoadFile(L"title.bin", 8000);
        if (gTitle.empty()) {
            MessageBoxW(hwnd, L"Could not read data\\title.bin.", L"Bruce Lee", MB_ICONERROR);
            return;
        }
    }
    StopMenuMusic();
    ClearOutOfLives();
    ResetScore();
    ResetCollectedLamps();
    ResetRoomBonus();
    ResetRun();
    gTwoPlayers = false;
    gHumanOpponent = false;
    gTitleStart = false;
    gMenuStart = false;
    gAfterFall = false;
    gGameOverFrames = 0;
    gPlayerFrames = 0;
    gPrevF3 = false;
    gPrevF5 = false;
    if (!EnterRoom(ClampedStartRoom())) {
        MessageBoxW(hwnd, L"Could not open the start room.", L"Bruce Lee", MB_ICONERROR);
        return;
    }
    gShownRoom = -1;
    SetWindowTextW(hwnd, L"Bruce Lee");
    if (gConfig.showTitleScreen) {
        ComposeTitle(gTitle.data());
        gStage = Stage::Title;
        return;
    }
    GiveStock();
    PlaceBruce();
    Compose();
    gStage = Stage::Play;
}

void ShowPlayerPage(bool afterFall) {
    // $0729 and $0ED0. The PLAYER page resets the tune and clears the SID.
    StopMenuMusic();
    gAfterFall = afterFall;
    gPlayerFrames = gTwoPlayers ? 43 : 21;
    gStage = Stage::Player;
    // $03 stays 0. This port plays as one Bruce, so the page says PLAYER 1.
    ComposePlayerUp(0);
}

void PollMenu() {
    const bool f3 = KeyDown(VK_F3);
    const bool f5 = KeyDown(VK_F5);
    // title_loop accepts F7, and the joystick button on $DC01.
    const bool start = KeyDown(VK_F7) || ReadPad().fire;
    if (f3 && !gPrevF3) {
        gTwoPlayers = !gTwoPlayers;
        ComposeMenu(gTwoPlayers, gHumanOpponent);
    }
    if (f5 && !gPrevF5) {
        gHumanOpponent = !gHumanOpponent;
        ComposeMenu(gTwoPlayers, gHumanOpponent);
    }
    gPrevF3 = f3;
    gPrevF5 = f5;
    if (start) {
        gMenuStart = true;
    }
    if (gMenuStart && !start) {
        gMenuStart = false;
        ShowPlayerPage(false);
    }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR commandLine, int show) {
    LoadConfig(commandLine);
    // StartRoom is $29, counting from 0. 0 is the opening room.
    g.frames = LoadFile(L"bruce.bin", static_cast<size_t>(kFrames * 2 * kFrameBytes));
    // front.bin holds the PLAYER page. A fall shows it even when the title is skipped.
    if (g.frames.empty() || !LoadFighters() || !LoadFront() || !EnterRoom(ClampedStartRoom())) {
        MessageBoxW(nullptr, L"Could not read data\\bruce.bin, the fighter pictures, the player page, and the room files.",
                    L"Bruce Lee", MB_ICONERROR);
        return 1;
    }
    g.pixels.assign(static_cast<size_t>(kWidth * kHeight), kPalette[0x0B]);
    gTitle = LoadFile(L"title.bin", 8000);
    if (gConfig.showTitleScreen) {
        if (gTitle.empty()) {
            MessageBoxW(nullptr, L"Could not read data\\title.bin.", L"Bruce Lee", MB_ICONERROR);
            return 1;
        }
        ComposeTitle(gTitle.data());
        gStage = Stage::Title;
    } else {
        PlaceBruce();
        Compose();
        gStage = Stage::Play;
    }

    if (!StartAudio()) {
        MessageBoxW(nullptr, L"Could not read data\\sound.bin.", L"Bruce Lee", MB_ICONERROR);
        return 1;
    }

    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &gFactory))) {
        StopAudio();
        return 1;
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    // Without this, a double-click arrives as two clicks and never toggles.
    wc.style = CS_DBLCLKS;
    // IDC_ARROW follows UNICODE, so it is a narrow pointer unless that macro
    // is set. 32512 is the arrow cursor, passed wide to match LoadCursorW.
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = L"BruceLeeWindow";
    RegisterClassW(&wc);

    ApplyAppTheme(nullptr);
    gMenu = CreateAppMenu();
    RECT rc{0, 0, kWidth * 3, kHeight * 3};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, TRUE);
    // Hidden until the fullscreen choice is applied, so the windowed frame
    // does not flash up when FullScreen is on.
    HWND hwnd = CreateWindowExW(
        0, wc.lpszClassName, L"Bruce Lee",
        WS_OVERLAPPEDWINDOW,
        80, 80,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, gMenu, inst, nullptr);
    if (!hwnd) {
        DestroyMenu(gMenu);
        gMenu = nullptr;
        StopAudio();
        return 1;
    }
    gHwnd = hwnd;
    ApplyAppTheme(hwnd);
    HACCEL accel = CreateAppAccelerators();
    if (gConfig.fullScreen) {
        SetFullScreen(hwnd, true);
    }
    ShowWindow(hwnd, gFullScreen ? SW_SHOW : show);
    // ShowWindow can put a hidden window back on its created rect.
    RefitFullScreen(hwnd);

    MSG msg{};
    bool running = true;
    while (running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE &&
                (msg.hwnd == hwnd || SettingsContains(msg.hwnd))) {
                CloseSettings();
                if (gFullScreen) {
                    SetFullScreen(hwnd, false);
                }
                continue;
            }
            if (accel && TranslateAcceleratorW(hwnd, accel, &msg)) {
                continue;
            }
            if (SettingsDialogMessage(&msg)) {
                continue;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running) {
            break;
        }
        if (gStage == Stage::Play && g.roomId != gShownRoom) {
            gShownRoom = g.roomId;
            wchar_t title[40];
            swprintf_s(title, L"Bruce Lee - room %d", g.roomId);
            SetWindowTextW(hwnd, title);
        }
        if (gStage == Stage::Title) {
            const bool space = KeyDown(VK_SPACE);
            const bool enter = KeyDown(VK_RETURN);
            const bool fire = space || enter || ReadPad().fire;
            if (fire) {
                gTitleStart = true;
            }
            // The picture stays until fire is up. A on the pad is that
            // button, the same as Space.
            if (gTitleStart && !fire) {
                gTitleStart = false;
                BeginMenu(true);
            }
        } else if (gStage == Stage::Menu) {
            PollMenu();
        } else if (gStage == Stage::Player) {
            gPlayerFrames--;
            if (gPlayerFrames <= 0) {
                if (gAfterFall) {
                    gAfterFall = false;
                    ResumeAfterFall();
                    gStage = Stage::Play;
                } else {
                    BeginPlay();
                }
            }
        } else if (gStage == Stage::GameOver) {
            gGameOverFrames--;
            if (gGameOverFrames <= 0) {
                gShownRoom = -1;
                StartAfterGameOver(hwnd);
            }
        } else {
            Tick();
            // lose_life has spent the fall. A count still positive runs
            // between_rooms in this room. A negative count is game over.
            if (OutOfLives()) {
                gShownRoom = -1;
                ShowGameOver(hwnd);
            } else if (BetweenLives()) {
                ShowPlayerPage(true);
            }
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        UpdateWindow(hwnd);
        // One Tick is one original logic frame. The raster IRQ decrements
        // $48 once per screen, and the game waits for a count of 3, so the
        // actors step every three PAL frames.
        Sleep(60);
    }

    if (accel) {
        DestroyAcceleratorTable(accel);
    }
    ReleaseUi();
    StopAudio();
    ReleaseTarget();
    gFactory->Release();
    return static_cast<int>(msg.wParam);
}
