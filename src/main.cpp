#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include <d2d1.h>
#include <d2d1helper.h>

#include "bruce.h"
#include "config.h"
#include "draw.h"
#include "fighter.h"
#include "front.h"
#include "game.h"
#include "load.h"
#include "pad.h"
#include "room.h"
#include "sound.h"

#include <cwchar>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwmapi.lib")

namespace {

constexpr D2D1_COLOR_F kBorderF{0.f, 0.f, 0.f, 1.f};

ID2D1Factory* gFactory = nullptr;
ID2D1HwndRenderTarget* gTarget = nullptr;
ID2D1Bitmap* gBitmap = nullptr;

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
    if (gBitmap) {
        gBitmap->Release();
        gBitmap = nullptr;
    }
    if (gTarget) {
        gTarget->Release();
        gTarget = nullptr;
    }
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
    gBitmap->CopyFromMemory(nullptr, g.pixels.data(), kWidth * sizeof(uint32_t));
    const D2D1_SIZE_F size = gTarget->GetSize();
    const float cw = size.width;
    const float ch = size.height;
    const float scale = (cw / kWidth < ch / kHeight) ? cw / kWidth : ch / kHeight;
    const float dw = kWidth * scale;
    const float dh = kHeight * scale;
    const D2D1_RECT_F dest = D2D1::RectF((cw - dw) * 0.5f, (ch - dh) * 0.5f,
                                         (cw - dw) * 0.5f + dw, (ch - dh) * 0.5f + dh);

    gTarget->BeginDraw();
    gTarget->Clear(kBorderF);
    gTarget->DrawBitmap(gBitmap, dest, 1.f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
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
        RefitFullScreen(hwnd);
    } else {
        gFullScreen = false;
        SetBorderlessFrame(hwnd, false);
        const LONG visible = GetWindowLongW(hwnd, GWL_STYLE) & WS_VISIBLE;
        SetWindowLongW(hwnd, GWL_STYLE, gWindowedStyle | visible);
        SetWindowLongW(hwnd, GWL_EXSTYLE, gWindowedExStyle);
        SetWindowPlacement(hwnd, &gWindowedPlace);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOCOPYBITS);
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

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
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            DestroyWindow(hwnd);
        } else if (gStage == Stage::Title && (wp == VK_SPACE || wp == VK_RETURN)) {
            // The title picture leaves on the fire button. Space is that
            // button here. The picture stays until the key is up, so the
            // options page does not see the same press.
            gTitleStart = true;
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

bool KeyDown(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
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

// open_room after the game-over wait. Score, falls, lamps, and the run
// counters start over. The top score stays. The room is 0, which is what
// clear_work_ram leaves in $29. The options page keeps the choices already
// made, the way $42CB and $42CC are copied back.
void StartAfterGameOver() {
    ResetScore();
    ResetCollectedLamps();
    ResetRoomBonus();
    ResetRun();
    EnterRoom(0);
    if (gConfig.showTitleScreen) {
        BeginMenu(false);
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
    int start = gConfig.startRoom;
    if (start < 0) {
        start = 0;
    }
    if (start >= kRoomCount) {
        start = kRoomCount - 1;
    }
    g.frames = LoadFile(L"bruce.bin", static_cast<size_t>(kFrames * 2 * kFrameBytes));
    // front.bin holds the PLAYER page. A fall shows it even when the title is skipped.
    if (g.frames.empty() || !LoadFighters() || !LoadFront() || !EnterRoom(start)) {
        MessageBoxW(nullptr, L"Could not read data\\bruce.bin, the fighter pictures, the player page, and the room files.",
                    L"Bruce Lee", MB_ICONERROR);
        return 1;
    }
    g.pixels.assign(static_cast<size_t>(kWidth * kHeight), kPalette[0x0B]);
    if (gConfig.showTitleScreen) {
        const std::vector<uint8_t> title = LoadFile(L"title.bin", 8000);
        if (title.empty()) {
            MessageBoxW(nullptr, L"Could not read data\\title.bin.", L"Bruce Lee", MB_ICONERROR);
            return 1;
        }
        ComposeTitle(title.data());
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

    RECT rc{0, 0, kWidth * 3, kHeight * 3};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    // Hidden until the fullscreen choice is applied, so the windowed frame
    // does not flash up when FullScreen is on.
    HWND hwnd = CreateWindowExW(
        0, wc.lpszClassName, L"Bruce Lee",
        WS_OVERLAPPEDWINDOW,
        80, 80,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, inst, nullptr);
    if (!hwnd) {
        StopAudio();
        return 1;
    }
    if (gConfig.fullScreen) {
        SetFullScreen(hwnd, true);
    }
    ShowWindow(hwnd, gFullScreen ? SW_SHOW : show);
    // ShowWindow can put a hidden window back on its created rect.
    RefitFullScreen(hwnd);

    MSG msg{};
    bool running = true;
    int shownRoom = -1;
    while (running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running) {
            break;
        }
        if (gStage == Stage::Play && g.roomId != shownRoom) {
            shownRoom = g.roomId;
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
                shownRoom = -1;
                StartAfterGameOver();
            }
        } else {
            Tick();
            // lose_life has spent the fall. A count still positive runs
            // between_rooms in this room. A negative count is game over.
            if (OutOfLives()) {
                shownRoom = -1;
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

    StopAudio();
    ReleaseTarget();
    gFactory->Release();
    return static_cast<int>(msg.wParam);
}
