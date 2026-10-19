#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>

#include "bruce.h"
#include "config.h"
#include "draw.h"
#include "front.h"
#include "game.h"
#include "load.h"
#include "pad.h"
#include "room.h"

#include <cwchar>

#pragma comment(lib, "d2d1.lib")

namespace {

constexpr D2D1_COLOR_F kBorderF{0.f, 0.f, 0.f, 1.f};

ID2D1Factory* gFactory = nullptr;
ID2D1HwndRenderTarget* gTarget = nullptr;
ID2D1Bitmap* gBitmap = nullptr;

// Title picture, then the options page, then between_rooms, then play.
enum class Stage { Title, Menu, Player, Play };
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

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE:
        if (gTarget) {
            gTarget->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp)));
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
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

void BeginMenu() {
    gTwoPlayers = false;
    gHumanOpponent = false;
    gMenuStart = false;
    // A key already held when the page appears waits for a fresh press.
    gPrevF3 = KeyDown(VK_F3);
    gPrevF5 = KeyDown(VK_F5);
    gStage = Stage::Menu;
    ComposeMenu(gTwoPlayers, gHumanOpponent);
}

void BeginPlay() {
    // clear_work_ram writes 4 falls for the computer and 9 for a human opponent.
    g.lives = gHumanOpponent ? 9 : 4;
    gStage = Stage::Play;
    PlaceBruce();
    Compose();
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
        gPlayerFrames = gTwoPlayers ? 43 : 21;
        gStage = Stage::Player;
        // $03 is 0 at cold start, so the page says PLAYER 1.
        ComposePlayerUp(0);
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
    if (g.frames.empty() || !EnterRoom(start)) {
        MessageBoxW(nullptr, L"Could not read data\\bruce.bin and the room files.",
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
        if (!LoadFront()) {
            MessageBoxW(nullptr, L"Could not read data\\front.bin.", L"Bruce Lee", MB_ICONERROR);
            return 1;
        }
        ComposeTitle(title.data());
        gStage = Stage::Title;
    } else {
        PlaceBruce();
        Compose();
        gStage = Stage::Play;
    }

    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &gFactory))) {
        return 1;
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    // IDC_ARROW follows UNICODE, so it is a narrow pointer unless that macro
    // is set. 32512 is the arrow cursor, passed wide to match LoadCursorW.
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = L"BruceLeeWindow";
    RegisterClassW(&wc);

    RECT rc{0, 0, kWidth * 3, kHeight * 3};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExW(
        0, wc.lpszClassName, L"Bruce Lee",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        80, 80,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, inst, nullptr);
    if (!hwnd) {
        return 1;
    }
    ShowWindow(hwnd, show);

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
                BeginMenu();
            }
        } else if (gStage == Stage::Menu) {
            PollMenu();
        } else if (gStage == Stage::Player) {
            gPlayerFrames--;
            if (gPlayerFrames <= 0) {
                BeginPlay();
            }
        } else {
            Tick();
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        UpdateWindow(hwnd);
        // One Tick is one original logic frame. The raster IRQ decrements
        // $48 once per screen, and the game waits for a count of 3, so the
        // actors step every three PAL frames.
        Sleep(60);
    }

    ReleaseTarget();
    gFactory->Release();
    return static_cast<int>(msg.wParam);
}
