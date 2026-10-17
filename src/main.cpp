#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>

#include "bruce.h"
#include "config.h"
#include "draw.h"
#include "game.h"
#include "load.h"
#include "room.h"

#include <cwchar>

#pragma comment(lib, "d2d1.lib")

namespace {

constexpr D2D1_COLOR_F kBorderF{0.f, 0.f, 0.f, 1.f};

ID2D1Factory* gFactory = nullptr;
ID2D1HwndRenderTarget* gTarget = nullptr;
ID2D1Bitmap* gBitmap = nullptr;
bool gInTitle = false;
bool gTitleStart = false;

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
        } else if (gInTitle && (wp == VK_SPACE || wp == VK_RETURN)) {
            // title_loop leaves on the fire button. Space is that button here.
            // The picture stays until the key is up, so the first game frame
            // does not treat the same press as a punch.
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

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR commandLine, int show) {
    LoadConfig(commandLine);
    // StartRoom is the room the player sees, counting from 1. $29 is one less.
    int start = gConfig.startRoom;
    if (start < 1) {
        start = 1;
    }
    if (start > kRoomCount) {
        start = kRoomCount;
    }
    g.frames = LoadFile(L"bruce.bin", static_cast<size_t>(kFrames * 2 * kFrameBytes));
    if (g.frames.empty() || !EnterRoom(start - 1)) {
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
        ComposeTitle(title.data());
        gInTitle = true;
    } else {
        PlaceBruce();
        Compose();
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
        if (!gInTitle && g.roomId != shownRoom) {
            shownRoom = g.roomId;
            wchar_t title[40];
            swprintf_s(title, L"Bruce Lee - room %d", g.roomId + 1);
            SetWindowTextW(hwnd, title);
        }
        if (gInTitle) {
            const bool space = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
            const bool enter = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
            if (gTitleStart && !space && !enter) {
                gInTitle = false;
                gTitleStart = false;
                PlaceBruce();
                Compose();
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
