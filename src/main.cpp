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
#include <wincodec.h>
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
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <new>
#include <string>
#include <vector>

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")

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
// Space during play, the gamepad menu button, or another program taking
// the keyboard. Settings is this program, so it does not pause the game.
// A click on the picture, Space, or the menu button clears the pause.
// Focus coming back does not: the picture stays covered until one of those.
bool gPaused = false;
// ShowWindow activates the window before the loop. Ignore that, and any
// activate that arrives while the window is still being created.
bool gRunLoop = false;
// The click that resumes also completes a double-click. That second half
// must not flip full screen.
bool gPauseClick = false;
// The menu button is an edge. Armed once the picture has the keyboard, so
// a button already held when focus returns does not resume on its own.
bool gMenuArmed = false;
bool gPrevMenu = false;
// The View button opens Settings, and closes it on the next press. Armed
// while this program is in front, including while Settings itself is.
bool gViewArmed = false;
bool gPrevView = false;
// $1385. Nonzero skips the options tune. clear_work_ram does not touch it,
// so F1 and a new game keep the choice made with S.
bool gSkipOptionsTune = false;
// $09BB. open_room decrements it once. Below zero, the tune starts and the
// byte reloads with 4. S and clear_work_ram store 0, so the next options
// page that is allowed to play starts the tune at once.
uint8_t gOptionsTuneHold = 0;
// This visit called start_tune. Turning sound on again restarts that tune.
bool gOptionsTunePlaying = false;
// Holding Y while paused. Negative hides the bar under PAUSED.
constexpr ULONGLONG kRestartHoldMs = 2000;
ULONGLONG gRestartHoldStart = 0;
bool gRestartHolding = false;
float gRestartProgress = -1.f;

bool WindowHasKeyboardFocus(HWND hwnd) {
    // The foreground window owns the keyboard. A menu can clear GetFocus
    // without giving the keys to another window, so this follows the
    // foreground window rather than GetFocus.
    return hwnd != nullptr && !IsIconic(hwnd) && GetForegroundWindow() == hwnd;
}

bool ApplicationHasKeyboardFocus() {
    const HWND fg = GetForegroundWindow();
    if (fg == nullptr) {
        return false;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

void SetPaused(bool on) {
    if (gPaused == on) {
        return;
    }
    gPaused = on;
    SetAudioPaused(on);
    // $1A42. Space and the idle timeout enter the $0105 pause by storing
    // $11 and calling timed_caption, so the word is up, and the following
    // frames skip the countdown. The picture is already composed for this
    // frame, so it is drawn again with the word showing.
    if (on && gStage == Stage::Play) {
        ResetPlayerCaption();
        Compose();
    }
}

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

void CoverPause() {
    if (!gPaused) {
        return;
    }
    DrawPausedBanner(gRestartProgress);
}

void RestorePause(const std::vector<uint32_t>& under) {
    if (under.empty()) {
        return;
    }
    for (int y = 0; y < kPausedBandHeight; ++y) {
        uint32_t* dst = g.pixels.data() + static_cast<size_t>(kPausedBandTop + y) * kWidth;
        const uint32_t* src = under.data() + static_cast<size_t>(y) * kWidth;
        std::memcpy(dst, src, kWidth * sizeof(uint32_t));
    }
}

bool gFlash = false;
ULONGLONG gFlashStart = 0;

std::vector<uint32_t> CoverFlash() {
    std::vector<uint32_t> under;
    if (!gFlash || g.pixels.size() < static_cast<size_t>(kWidth) * kHeight) {
        return under;
    }
    const int age = static_cast<int>(GetTickCount64() - gFlashStart);
    if (age < 0 || age >= kCameraFlashMs) {
        gFlash = false;
        return under;
    }
    under.resize(static_cast<size_t>(kCameraFlashW) * kCameraFlashH);
    for (int y = 0; y < kCameraFlashH; ++y) {
        const uint32_t* src = g.pixels.data() + static_cast<size_t>(kCameraFlashY + y) * kWidth + kCameraFlashX;
        std::memcpy(under.data() + static_cast<size_t>(y) * kCameraFlashW, src,
                    kCameraFlashW * sizeof(uint32_t));
    }
    DrawCameraFlash(age);
    return under;
}

void RestoreFlash(const std::vector<uint32_t>& under) {
    if (under.size() != static_cast<size_t>(kCameraFlashW) * kCameraFlashH) {
        return;
    }
    if (g.pixels.size() < static_cast<size_t>(kWidth) * kHeight) {
        return;
    }
    for (int y = 0; y < kCameraFlashH; ++y) {
        uint32_t* dst = g.pixels.data() + static_cast<size_t>(kCameraFlashY + y) * kWidth + kCameraFlashX;
        std::memcpy(dst, under.data() + static_cast<size_t>(y) * kCameraFlashW,
                    kCameraFlashW * sizeof(uint32_t));
    }
}

void StartFlash() {
    gFlash = true;
    gFlashStart = GetTickCount64();
}

std::vector<uint32_t> SavePauseBand() {
    std::vector<uint32_t> under(static_cast<size_t>(kPausedBandHeight) * kWidth);
    for (int y = 0; y < kPausedBandHeight; ++y) {
        const uint32_t* src = g.pixels.data() + static_cast<size_t>(kPausedBandTop + y) * kWidth;
        std::memcpy(under.data() + static_cast<size_t>(y) * kWidth, src, kWidth * sizeof(uint32_t));
    }
    return under;
}

template <class T>
struct ComPtr {
    T* p = nullptr;
    ComPtr() = default;
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    ~ComPtr() {
        if (p) {
            p->Release();
        }
    }
    T* Get() const { return p; }
    T* operator->() const { return p; }
    explicit operator bool() const { return p != nullptr; }
    void Reset() {
        if (p) {
            p->Release();
            p = nullptr;
        }
    }
    // Empty slot for an out-parameter. The caller has not stored a pointer yet.
    T** Receive() { return &p; }
};

// CoInitialize on this thread, released after the COM pointers that use it.
struct ComApartment {
    bool balance = false;
    ComApartment() {
        const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        balance = co == S_OK || co == S_FALSE;
    }
    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;
    ~ComApartment() {
        if (balance) {
            CoUninitialize();
        }
    }
};

// Where the 320 by 200 picture sits in the window. The CRT resample is 1:1
// in that same rectangle. A filter with every part off keeps the sharp scale.
struct FramePlace {
    D2D1_RECT_F sharp{};
    D2D1_RECT_F crtDest{};
    UINT crtW = 0;
    UINT crtH = 0;
    bool crt = false;
    CrtOptions options{};
};

FramePlace PlaceFrame(float cw, float ch) {
    FramePlace place;
    place.options = CrtOptions{gConfig.crtCurve, gConfig.crtScanlines, gConfig.crtBlur, gConfig.crtVignette};
    const float scale = (cw / kWidth < ch / kHeight) ? cw / kWidth : ch / kHeight;
    const float dw = kWidth * scale;
    const float dh = kHeight * scale;
    place.sharp = D2D1::RectF((cw - dw) * 0.5f, (ch - dh) * 0.5f,
                              (cw - dw) * 0.5f + dw, (ch - dh) * 0.5f + dh);
    const bool crtParts = place.options.curve || place.options.scanlines || place.options.blur || place.options.vignette;
    // ShowCRTFilter resamples into the same fitted rectangle. The samples are
    // already window pixels, so the blit is 1:1. Off, and a filter with every
    // part turned off, keeps the nearest-neighbor scale of the 320 by 200 picture.
    if (gConfig.showCrtFilter && crtParts && dw >= 1.f && dh >= 1.f) {
        place.crtW = static_cast<UINT>(dw);
        place.crtH = static_cast<UINT>(dh);
        const float ox = std::floor((cw - static_cast<float>(place.crtW)) * 0.5f);
        const float oy = std::floor((ch - static_cast<float>(place.crtH)) * 0.5f);
        place.crtDest = D2D1::RectF(ox, oy, ox + static_cast<float>(place.crtW), oy + static_cast<float>(place.crtH));
        place.crt = true;
    }
    return place;
}

HRESULT DrawPlaced(ID2D1RenderTarget* target, ID2D1Bitmap* bitmap, const D2D1_RECT_F& dest) {
    target->BeginDraw();
    target->Clear(kBorderF);
    target->DrawBitmap(bitmap, dest, 1.f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
    return target->EndDraw();
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
    // The band is painted into g.pixels for this blit, then put back, so a
    // later logic frame still has the picture that was under the letters.
    std::vector<uint32_t> underPause;
    if (gPaused) {
        underPause = SavePauseBand();
        CoverPause();
    }
    // Same as the pause band: in the picture for this blit, then put back.
    const std::vector<uint32_t> underFlash = CoverFlash();
    const D2D1_SIZE_F size = gTarget->GetSize();
    const FramePlace place = PlaceFrame(size.width, size.height);
    bool crt = false;
    if (place.crt && EnsureCrtBitmap(place.crtW, place.crtH)) {
        ApplyCrt(g.pixels.data(), kWidth, kHeight, gCrtPixels.data(),
                 static_cast<int>(place.crtW), static_cast<int>(place.crtH), place.options);
        gCrtBitmap->CopyFromMemory(nullptr, gCrtPixels.data(),
                                   place.crtW * static_cast<UINT32>(sizeof(uint32_t)));
        crt = true;
    }
    if (!crt) {
        gBitmap->CopyFromMemory(nullptr, g.pixels.data(), kWidth * sizeof(uint32_t));
    }
    const HRESULT hr = DrawPlaced(gTarget, crt ? gCrtBitmap : gBitmap, crt ? place.crtDest : place.sharp);
    RestoreFlash(underFlash);
    RestorePause(underPause);
    if (hr == D2DERR_RECREATE_TARGET) {
        ReleaseTarget();
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

bool EncodePng(IWICImagingFactory* factory, IWICBitmap* bitmap, std::vector<uint8_t>& png) {
    ComPtr<IStream> stream;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, stream.Receive()))) {
        return false;
    }
    ComPtr<IWICBitmapEncoder> encoder;
    if (FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.Receive()))) {
        return false;
    }
    if (FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache))) {
        return false;
    }
    ComPtr<IWICBitmapFrameEncode> frame;
    if (FAILED(encoder->CreateNewFrame(frame.Receive(), nullptr))) {
        return false;
    }
    if (FAILED(frame->Initialize(nullptr)) || FAILED(frame->WriteSource(bitmap, nullptr)) ||
        FAILED(frame->Commit()) || FAILED(encoder->Commit())) {
        return false;
    }
    STATSTG stat{};
    if (FAILED(stream->Stat(&stat, STATFLAG_NONAME))) {
        return false;
    }
    if (stat.cbSize.QuadPart < 8 || stat.cbSize.QuadPart > 64ull * 1024ull * 1024ull) {
        return false;
    }
    const ULONG n = static_cast<ULONG>(stat.cbSize.QuadPart);
    const LARGE_INTEGER zero{};
    if (FAILED(stream->Seek(zero, STREAM_SEEK_SET, nullptr))) {
        return false;
    }
    png.resize(n);
    ULONG read = 0;
    return SUCCEEDED(stream->Read(png.data(), n, &read)) && read == n;
}

// Same draw as Paint, into a bitmap the size of the render target. The menu
// bar is outside that target, so it is not in the picture.
bool RenderWindowPng(std::vector<uint8_t>& png) {
    const D2D1_SIZE_U pixel = gTarget->GetPixelSize();
    if (pixel.width == 0 || pixel.height == 0 || pixel.width > 16384 || pixel.height > 16384) {
        return false;
    }
    float dpiX = 96.f;
    float dpiY = 96.f;
    gTarget->GetDpi(&dpiX, &dpiY);
    if (dpiX < 1.f) {
        dpiX = 96.f;
    }
    if (dpiY < 1.f) {
        dpiY = 96.f;
    }
    const D2D1_SIZE_F size = gTarget->GetSize();
    const FramePlace place = PlaceFrame(size.width, size.height);
    bool ok = false;
    {
        // Declared first so its destructor runs after the COM pointers.
        ComApartment apartment;
        ComPtr<IWICImagingFactory> factory;
        if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                       IID_PPV_ARGS(factory.Receive())))) {
            ComPtr<IWICBitmap> wic;
            if (SUCCEEDED(factory->CreateBitmap(pixel.width, pixel.height, GUID_WICPixelFormat32bppBGR,
                                                WICBitmapCacheOnDemand, wic.Receive()))) {
                const D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
                    D2D1_RENDER_TARGET_TYPE_DEFAULT,
                    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
                    dpiX, dpiY);
                ComPtr<ID2D1Bitmap> picture;
                bool drew = false;
                {
                    ComPtr<ID2D1RenderTarget> shot;
                    if (SUCCEEDED(gFactory->CreateWicBitmapRenderTarget(wic.Get(), props, shot.Receive()))) {
                        shot->SetDpi(dpiX, dpiY);
                        const D2D1_BITMAP_PROPERTIES bitmapProps = D2D1::BitmapProperties(
                            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), dpiX, dpiY);
                        const uint64_t crtCount = static_cast<uint64_t>(place.crtW) * place.crtH;
                        const bool drawCrt = place.crt && crtCount > 0 && crtCount <= 16384ull * 16384ull;
                        if (drawCrt) {
                            std::vector<uint32_t> crtPixels(static_cast<size_t>(crtCount));
                            ApplyCrt(g.pixels.data(), kWidth, kHeight, crtPixels.data(),
                                     static_cast<int>(place.crtW), static_cast<int>(place.crtH), place.options);
                            if (SUCCEEDED(shot->CreateBitmap(D2D1::SizeU(place.crtW, place.crtH), crtPixels.data(),
                                                             place.crtW * static_cast<UINT32>(sizeof(uint32_t)),
                                                             bitmapProps, picture.Receive())) &&
                                SUCCEEDED(DrawPlaced(shot.Get(), picture.Get(), place.crtDest))) {
                                drew = true;
                            }
                        }
                        if (!drew) {
                            picture.Reset();
                            if (SUCCEEDED(shot->CreateBitmap(D2D1::SizeU(kWidth, kHeight), g.pixels.data(),
                                                             kWidth * static_cast<UINT32>(sizeof(uint32_t)),
                                                             bitmapProps, picture.Receive())) &&
                                SUCCEEDED(DrawPlaced(shot.Get(), picture.Get(), place.sharp))) {
                                drew = true;
                            }
                        }
                    }
                }
                if (drew) {
                    ok = EncodePng(factory.Get(), wic.Get(), png);
                }
            }
        }
    }
    return ok;
}

// ScreenShot00.png through ScreenShot99.png. An existing file, of any kind,
// is left alone and the next number is tried.
bool WriteScreenshotFile(const std::vector<uint8_t>& png, std::wstring& error) {
    if (png.empty() || png.size() > 0xFFFFFFFFu) {
        error = L"Could not take a screenshot.";
        return false;
    }
    const std::wstring dir = ConfigDirectory();
    for (int n = 0; n < 100; ++n) {
        wchar_t name[32];
        swprintf_s(name, L"ScreenShot%02d.png", n);
        const std::wstring path = dir + L"\\" + name;
        const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                        FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                continue;
            }
            error = L"Could not write " + path;
            return false;
        }
        DWORD written = 0;
        const BOOL wrote = WriteFile(file, png.data(), static_cast<DWORD>(png.size()), &written, nullptr);
        CloseHandle(file);
        if (!wrote || written != png.size()) {
            error = L"Could not write " + path;
            return false;
        }
        return true;
    }
    error = L"ScreenShot00.png through ScreenShot99.png already exist in " + dir + L".";
    return false;
}

bool SaveScreenshot(HWND hwnd, std::wstring& error) {
    if (!EnsureTarget(hwnd) || g.pixels.size() < static_cast<size_t>(kWidth) * kHeight) {
        error = L"Could not take a screenshot.";
        return false;
    }
    std::vector<uint8_t> png;
    if (!RenderWindowPng(png)) {
        error = L"Could not take a screenshot.";
        return false;
    }
    return WriteScreenshotFile(png, error);
}

void TakeScreenshot(HWND hwnd) {
    // The band is in the picture the window is showing. Put it back before
    // this returns, including when the save fails, so the next frame is clean.
    std::vector<uint32_t> underPause;
    if (gPaused) {
        underPause = SavePauseBand();
        CoverPause();
    }
    // A flash already on screen is part of this picture. The one for this
    // save starts after the file is written, so the file does not contain it.
    const std::vector<uint32_t> underFlash = CoverFlash();
    std::wstring error;
    bool ok = false;
    try {
        ok = SaveScreenshot(hwnd, error);
    } catch (const std::bad_alloc&) {
        error = L"Could not take a screenshot.";
    }
    RestoreFlash(underFlash);
    RestorePause(underPause);
    if (ok) {
        StartFlash();
        return;
    }
    if (error.empty()) {
        error = L"Could not take a screenshot.";
    }
    MessageBoxW(hwnd, error.c_str(), L"Bruce Lee Remake", MB_ICONERROR);
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
    case WM_ACTIVATEAPP:
        // wParam is false when another program is taking the foreground.
        // Moving to Settings stays inside this process and does not pause.
        if (gRunLoop && gConfig.pauseOnFocusLoss && wp == FALSE) {
            SetPaused(true);
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    case WM_LBUTTONDOWN:
        if (gPaused) {
            SetPaused(false);
            gPauseClick = true;
            return 0;
        }
        gPauseClick = false;
        return DefWindowProcW(hwnd, msg, wp, lp);
    case WM_LBUTTONDBLCLK:
        if (gPauseClick) {
            gPauseClick = false;
            return 0;
        }
        SetFullScreen(hwnd, !gFullScreen);
        return 0;
    case WM_NCLBUTTONDOWN:
        // The picture click is WM_LBUTTONDOWN. The caption is part of the
        // window too. Close, minimize, and the menu bar keep the pause.
        if (gPaused && wp != HTCLOSE && wp != HTMINBUTTON && wp != HTMENU) {
            SetPaused(false);
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
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
        case ID_SCREENSHOT:
            TakeScreenshot(hwnd);
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
        case ID_KEYS:
            ShowKeyboardShortcuts(hwnd);
            return 0;
        case ID_CONTROLLER:
            ShowControllerMapping(hwnd);
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
    case WM_APP_RESET_TOP:
        ResetTopScore();
        // The score line is part of the play picture. Title, options, and
        // game over keep their own pictures.
        if (gStage == Stage::Play) {
            Compose();
        }
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
        // Bit 30 is set when the key was already down, so a repeat does not
        // flip the pause or the credit line twice. Alt belongs to the menu.
        // in_game_key runs before the pause skip, so V, S, and Space still
        // work while the picture is held.
        if ((lp & (1 << 30)) == 0 && (lp & (1 << 29)) == 0 && !UiOwnsKeyboard() &&
            WindowHasKeyboardFocus(hwnd)) {
            if (wp == 'V' && gStage == Stage::Play) {
                ToggleCreditLine();
                if (gPaused) {
                    Compose();
                }
                return 0;
            }
            if (wp == 'S' && gStage == Stage::Play) {
                // $1B4E. Toggle $1385 and clear $09BB. The rooms stay quiet.
                // The next options page hears the change.
                gSkipOptionsTune = !gSkipOptionsTune;
                gOptionsTuneHold = 0;
                return 0;
            }
            if (wp == VK_SPACE && (gStage == Stage::Play || gPaused)) {
                // $3C toggles $0105 during play. On the title picture Space
                // is still the fire button, so a pause from focus loss is
                // cleared here and that press does not also leave the picture.
                SetPaused(!gPaused);
                return 0;
            }
        }
        if (gPaused) {
            return 0;
        }
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
            MessageBoxW(hwnd, L"Could not read data\\sound.bin.", L"Bruce Lee Remake", MB_ICONERROR);
        } else if (gStage == Stage::Menu && gOptionsTunePlaying) {
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
    // init_hardware, after the PLAYER page. The word starts showing.
    ResetPlayerCaption();
    Compose();
}

void BeginMenu(bool fresh) {
    // $06BD. $1385 nonzero leaves the page silent. Otherwise $09BB counts
    // down once per visit. Below zero the tune starts and $09BB reloads
    // with 4. The title picture is not this page, and it stays silent.
    gOptionsTunePlaying = false;
    if (!gSkipOptionsTune) {
        gOptionsTuneHold = static_cast<uint8_t>(gOptionsTuneHold - 1);
        if ((gOptionsTuneHold & 0x80) != 0) {
            gOptionsTuneHold = 4;
            gOptionsTunePlaying = true;
            StartMenuMusic();
        }
    }
    if (!gOptionsTunePlaying) {
        StopMenuMusic();
    }
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
    SetWindowTextW(hwnd, L"Bruce Lee Remake");
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
        MessageBoxW(hwnd, L"Could not open the start room.", L"Bruce Lee Remake", MB_ICONERROR);
        return;
    }
    if (gConfig.showTitleScreen) {
        BeginMenu(false);
        return;
    }
    GiveStock();
    PlaceBruce();
    ResetPlayerCaption();
    Compose();
    gStage = Stage::Play;
}

// F1 is start, the cold start. clear_work_ram, then open_room, the options
// page. The title picture is only the boot screen before start, so this
// does not show it. $42CB and $42CC keep the two option choices. The top
// score stays, as it does after game over. The start room is the port's
// room 0: play begins there when the options page is left.
void RestartGame(HWND hwnd) {
    SetPaused(false);
    StopMenuMusic();
    ClearOutOfLives();
    ResetScore();
    ResetCollectedLamps();
    ResetRoomBonus();
    ResetRun();
    gTitleStart = false;
    gMenuStart = false;
    gAfterFall = false;
    gGameOverFrames = 0;
    gPlayerFrames = 0;
    gPrevF3 = false;
    gPrevF5 = false;
    // clear_work_ram zeros $09BB before open_room. The tune then starts
    // at once, unless S has set $1385.
    gOptionsTuneHold = 0;
    if (!EnterRoom(ClampedStartRoom())) {
        MessageBoxW(hwnd, L"Could not open the start room.", L"Bruce Lee Remake", MB_ICONERROR);
        return;
    }
    gShownRoom = -1;
    SetWindowTextW(hwnd, L"Bruce Lee Remake");
    BeginMenu(false);
}

void ShowPlayerPage(bool afterFall) {
    // $0729 and $0ED0. The PLAYER page resets the tune and clears the SID.
    StopMenuMusic();
    // clear_work_ram, after the options page and before play. A fall does
    // not take that path, so $09BB stays as the options page left it.
    if (!afterFall) {
        gOptionsTuneHold = 0;
    }
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

// Y held for two seconds while paused returns to the options page.
// Releasing Y clears the bar. The count is wall time, not logic frames.
void PollRestartHold(HWND hwnd, bool yDown) {
    if (!gPaused || !yDown) {
        gRestartHolding = false;
        gRestartProgress = -1.f;
        return;
    }
    const ULONGLONG now = GetTickCount64();
    if (!gRestartHolding) {
        gRestartHolding = true;
        gRestartHoldStart = now;
    }
    const ULONGLONG elapsed = now - gRestartHoldStart;
    if (elapsed >= kRestartHoldMs) {
        gRestartHolding = false;
        gRestartProgress = -1.f;
        RestartGame(hwnd);
        SetPaused(false);
        return;
    }
    gRestartProgress = static_cast<float>(elapsed) / static_cast<float>(kRestartHoldMs);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR commandLine, int show) {
    // Gamepad names come from the Windows Runtime. That init has to be the
    // first COM init on this thread, before the screenshot and picture loads.
    PadSession pads;
    LoadConfig(commandLine);
    SyncGamepads();
    // StartRoom is $29, counting from 0. 0 is the opening room.
    g.frames = LoadFile(L"bruce.bin", static_cast<size_t>(kFrames * 2 * kFrameBytes));
    // front.bin holds the PLAYER page. A fall shows it even when the title is skipped.
    if (g.frames.empty() || !LoadFighters() || !LoadFront() || !EnterRoom(ClampedStartRoom())) {
        MessageBoxW(nullptr, L"Could not read data\\bruce.bin, the fighter pictures, the player page, and the room files.",
                    L"Bruce Lee Remake", MB_ICONERROR);
        return 1;
    }
    g.pixels.assign(static_cast<size_t>(kWidth * kHeight), kPalette[0x0B]);
    gTitle = LoadFile(L"title.bin", 8000);
    if (gConfig.showTitleScreen) {
        if (gTitle.empty()) {
            MessageBoxW(nullptr, L"Could not read data\\title.bin.", L"Bruce Lee Remake", MB_ICONERROR);
            return 1;
        }
        ComposeTitle(gTitle.data());
        gStage = Stage::Title;
    } else {
        PlaceBruce();
        ResetPlayerCaption();
        Compose();
        gStage = Stage::Play;
    }

    if (!StartAudio()) {
        MessageBoxW(nullptr, L"Could not read data\\sound.bin.", L"Bruce Lee Remake", MB_ICONERROR);
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
        0, wc.lpszClassName, L"Bruce Lee Remake",
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
    // The welcome wizard runs while this window is still hidden, so the
    // title does not sit behind it. A quit there leaves the window unshown.
    if (gConfig.firstTimeRun && !ShowIntroWizard(hwnd)) {
        DestroyWindow(hwnd);
        if (accel) {
            DestroyAcceleratorTable(accel);
        }
        ReleaseUi();
        StopAudio();
        ReleaseTarget();
        gFactory->Release();
        gFactory = nullptr;
        return 0;
    }
    if (gConfig.fullScreen) {
        SetFullScreen(hwnd, true);
    }
    ShowWindow(hwnd, gFullScreen ? SW_SHOW : show);
    // ShowWindow can put a hidden window back on its created rect.
    RefitFullScreen(hwnd);
    gRunLoop = true;

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
            // A held F12 repeats. Each repeat would write another file.
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_F12 && (msg.lParam & (1 << 30)) != 0) {
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
            swprintf_s(title, L"Bruce Lee Remake - room %d", g.roomId);
            SetWindowTextW(hwnd, title);
        }
        BeginPadFrame();
        const bool gameFocus = WindowHasKeyboardFocus(hwnd);
        if (!gameFocus) {
            gMenuArmed = false;
        }
        // PauseOnFocusLoss=False on the command line keeps the frame moving
        // while another program is in front. Settings is still this program.
        if (!ApplicationHasKeyboardFocus() && gConfig.pauseOnFocusLoss) {
            SetPaused(true);
        }
        // Settings is another window of this program, so View still reaches
        // it there. Another program in front leaves the pad unread.
        bool restartY = false;
        if (gameFocus || UiOwnsKeyboard()) {
            const PadInput pad = ReadPad();
            if (gameFocus) {
                // A fresh press pauses, and the next one resumes. The click
                // path only resumes.
                if (gMenuArmed && pad.menu && !gPrevMenu) {
                    SetPaused(!gPaused);
                }
                gPrevMenu = pad.menu;
                gMenuArmed = true;
                restartY = pad.y;
            }
            if (gViewArmed && pad.view && !gPrevView) {
                if (SettingsIsOpen()) {
                    CloseSettings();
                } else {
                    ToggleSettings(hwnd);
                }
            }
            gPrevView = pad.view;
            gViewArmed = true;
        } else {
            gViewArmed = false;
        }
        PollRestartHold(hwnd, restartY);
        if (!gPaused) {
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
