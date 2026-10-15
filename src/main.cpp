#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>

#include <cstdint>
#include <string>
#include <vector>

#pragma comment(lib, "d2d1.lib")

namespace {

constexpr int kWidth = 320;
constexpr int kHeight = 200;
constexpr int kSpriteRows = 21;
constexpr int kSpriteCols = 24;
constexpr int kFrameBytes = kSpriteRows * 3;
constexpr int kFrames = 4;

// Pepto. Stored as 0xAARRGGBB, which is BGRA in memory on this machine.
// The title viewer used the same packing for light blue #6C5EB5.
constexpr uint32_t kPalette[16] = {
    0xFF000000u, 0xFFFFFFFFu, 0xFF68372Bu, 0xFF70A4B2u,
    0xFF6F3D86u, 0xFF588D43u, 0xFF352879u, 0xFFB8C76Fu,
    0xFF6F4F25u, 0xFF433900u, 0xFF9A6759u, 0xFF444444u,
    0xFF6C6C6Cu, 0xFF9AD284u, 0xFF6C5EB5u, 0xFF959595u,
};
constexpr D2D1_COLOR_F kBorderF{0.f, 0.f, 0.f, 1.f};

struct App {
    ID2D1Factory* factory = nullptr;
    ID2D1HwndRenderTarget* target = nullptr;
    ID2D1Bitmap* bitmap = nullptr;
    std::vector<uint8_t> charset;
    std::vector<uint8_t> room;
    std::vector<uint8_t> color;
    std::vector<uint8_t> frames;
    std::vector<uint32_t> pixels;
    int x = 0;
    int y = 0;
    int vy = 0;
    int facing = 1;
    int tick = 0;
    bool jumpHeld = false;
};

App g;

std::wstring ExeDirectory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full(path);
    const auto slash = full.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : full.substr(0, slash);
}

std::vector<uint8_t> LoadFile(const wchar_t* name, size_t bytes) {
    const std::wstring dir = ExeDirectory();
    const std::wstring rels[] = {L"\\", L"\\..\\data\\", L"\\data\\"};
    for (const std::wstring& rel : rels) {
        const std::wstring path = dir + rel + name;
        HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            continue;
        }
        std::vector<uint8_t> data(bytes);
        DWORD read = 0;
        const BOOL ok = ReadFile(file, data.data(), static_cast<DWORD>(data.size()), &read, nullptr);
        CloseHandle(file);
        if (ok && read == bytes) {
            return data;
        }
    }
    return {};
}

// Raster bands for room 0. Screen y 0 is raster 50. Splits are $78 and $97.
// Each band is background, $D022, $D023. Color RAM supplies the fourth color.
void BandAt(int py, int& bg, int& c01, int& c10) {
    const int raster = py + 50;
    if (raster < 0x78) {
        bg = 0x0B;
        c01 = 0x01;
        c10 = 0x0E;
        return;
    }
    if (raster < 0x97) {
        bg = 0x0B;
        c01 = 0x02;
        c10 = 0x01;
        return;
    }
    bg = 0x0C;
    c01 = 0x02;
    c10 = 0x01;
}

// One screen pixel per multicolor dot. Characters are 8 pixels tall.
int PixelColor(int x, int y) {
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) {
        return -1;
    }
    const int cell = (y / 8) * 40 + (x / 8);
    const uint8_t tile = g.room[static_cast<size_t>(cell)];
    if (tile == 0xDC) {
        return -1;
    }
    // Color RAM bit 3 turns multicolor on. The ink is only the low three
    // bits, so a stored $08 is black and a stored $0E is blue.
    int color = g.color[static_cast<size_t>(cell)] & 0x0F;
    if (color >= 8) {
        color &= 7;
    }
    // Even rows are the shadow copy. That copy is character|0x80, the lower
    // half of the same source glyph, stored in the second 1KB of the charset.
    const int half = ((y / 8) & 1) == 0 ? 128 : 0;
    const uint8_t bits = g.charset[static_cast<size_t>(tile + half) * 8 + static_cast<size_t>(y & 7)];
    const int pair = (bits >> (6 - 2 * ((x & 7) / 2))) & 3;
    int bg, c01, c10;
    BandAt(y, bg, c01, c10);
    const int colors[4] = {bg, c01, c10, color};
    const int ink = colors[pair];
    return ink == bg ? -1 : ink;
}

bool SolidPixel(int x, int y) {
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) {
        return true;
    }
    return PixelColor(x, y) >= 0;
}

// Pose 0 is the standing figure. Poses 1 and 2 are the two strides.
// Pose 3 is the kick, shown while he is off the ground.
const uint8_t* PoseBytes(int plane) {
    int frame = 0;
    if (g.vy != 0) {
        frame = 3;
    } else if (g.tick > 0) {
        frame = 1 + (g.tick / 6) % 2;
    }
    return g.frames.data() + static_cast<size_t>(frame * 2 + plane) * kFrameBytes;
}

bool SpritePixel(const uint8_t* frame, int x, int y) {
    const int src = g.facing < 0 ? (kSpriteCols - 1 - x) : x;
    const uint8_t bits = frame[y * 3 + src / 8];
    return (bits & static_cast<uint8_t>(0x80 >> (src & 7))) != 0;
}

// The game ANDs Bruce's sprite with the character bitmaps under him.
bool Overlaps(int sx, int sy) {
    for (int plane = 0; plane < 2; ++plane) {
        const uint8_t* frame = g.frames.data() + static_cast<size_t>(plane) * kFrameBytes;
        for (int y = 0; y < kSpriteRows; ++y) {
            for (int x = 0; x < kSpriteCols; ++x) {
                if (!SpritePixel(frame, x, y)) {
                    continue;
                }
                if (SolidPixel(sx + x, sy + y)) {
                    return true;
                }
            }
        }
    }
    return false;
}

// The score line is the ROM charset, copied to $8800 at startup. These are
// the same shapes: digits, and the words on the room 0 bar.
uint8_t StatusGlyph(char ch, int row) {
    static const uint8_t kDigit[10][8] = {
        {0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00},
        {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
        {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x30, 0x7E, 0x00},
        {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00},
        {0x0C, 0x1C, 0x3C, 0x6C, 0x7E, 0x0C, 0x0C, 0x00},
        {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00},
        {0x3C, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x3C, 0x00},
        {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00},
        {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00},
        {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x0C, 0x38, 0x00},
    };
    static const uint8_t kT[8] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00};
    static const uint8_t kO[8] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00};
    static const uint8_t kP[8] = {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00};
    static const uint8_t kF[8] = {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00};
    static const uint8_t kA[8] = {0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00};
    static const uint8_t kL[8] = {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00};
    static const uint8_t kS[8] = {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00};
    const uint8_t* bits = nullptr;
    if (ch >= '0' && ch <= '9') {
        bits = kDigit[ch - '0'];
    } else if (ch == 'T') {
        bits = kT;
    } else if (ch == 'O') {
        bits = kO;
    } else if (ch == 'P') {
        bits = kP;
    } else if (ch == 'F') {
        bits = kF;
    } else if (ch == 'A') {
        bits = kA;
    } else if (ch == 'L') {
        bits = kL;
    } else if (ch == 'S') {
        bits = kS;
    }
    return bits != nullptr ? bits[row] : 0;
}

void DrawStatus() {
    const char* text = "000000  TOP 000000  FALLS 04";
    const int origin = 8 * 8;
    for (int i = 0; text[i] != '\0'; ++i) {
        for (int row = 0; row < 8; ++row) {
            const uint8_t bits = StatusGlyph(text[i], row);
            for (int col = 0; col < 8; ++col) {
                if ((bits & static_cast<uint8_t>(0x80 >> col)) == 0) {
                    continue;
                }
                const int px = origin + i * 8 + col;
                if (px >= 0 && px < kWidth) {
                    g.pixels[static_cast<size_t>(row * kWidth + px)] = kPalette[0];
                }
            }
        }
    }
}

void Compose() {
    for (int y = 0; y < kHeight; ++y) {
        int bg, c01, c10;
        BandAt(y, bg, c01, c10);
        const uint32_t paper = kPalette[bg];
        uint32_t* row = g.pixels.data() + y * kWidth;
        for (int x = 0; x < kWidth; ++x) {
            const int ink = PixelColor(x, y);
            if (y < 8) {
                row[x] = kPalette[15];
            } else {
                row[x] = ink < 0 ? paper : kPalette[ink];
            }
        }
    }
    // The body is yellow. The second image is the black hair, eyes, and legs,
    // drawn on top. One sprite pixel is one screen pixel, which is the shape
    // in the reference picture.
    const int spriteColor[2] = {7, 0};
    for (int plane = 0; plane < 2; ++plane) {
        const uint8_t* frame = PoseBytes(plane);
        for (int y = 0; y < kSpriteRows; ++y) {
            for (int x = 0; x < kSpriteCols; ++x) {
                if (!SpritePixel(frame, x, y)) {
                    continue;
                }
                const int px = g.x + x;
                const int py = g.y + y;
                if (px >= 0 && py >= 0 && px < kWidth && py < kHeight) {
                    g.pixels[static_cast<size_t>(py * kWidth + px)] = kPalette[spriteColor[plane]];
                }
            }
        }
    }
    DrawStatus();
}

void MoveVertical(int dy) {
    const int step = dy > 0 ? 1 : -1;
    const int n = dy > 0 ? dy : -dy;
    for (int i = 0; i < n; ++i) {
        if (Overlaps(g.x, g.y + step)) {
            g.vy = 0;
            return;
        }
        g.y += step;
    }
}

void Tick() {
    const bool left = (GetAsyncKeyState(VK_LEFT) & 0x8000) != 0;
    const bool right = (GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0;
    const bool up = (GetAsyncKeyState(VK_UP) & 0x8000) != 0;
    if (left) {
        g.facing = -1;
    }
    if (right) {
        g.facing = 1;
    }
    const int dx = (right ? 2 : 0) - (left ? 2 : 0);
    if (dx != 0 && !Overlaps(g.x + dx, g.y)) {
        g.x += dx;
        g.tick += 1;
    } else if (dx == 0) {
        g.tick = 0;
    }

    const bool grounded = Overlaps(g.x, g.y + 1);
    if (up && grounded && g.vy == 0 && !g.jumpHeld) {
        g.vy = -8;
    }
    g.jumpHeld = up;
    if (!grounded || g.vy < 0) {
        if (g.vy < 4) {
            g.vy += 1;
        }
        MoveVertical(g.vy);
    }
    Compose();
}

void ReleaseTarget() {
    if (g.bitmap) {
        g.bitmap->Release();
        g.bitmap = nullptr;
    }
    if (g.target) {
        g.target->Release();
        g.target = nullptr;
    }
}

bool EnsureTarget(HWND hwnd) {
    if (g.target) {
        return true;
    }
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const UINT w = static_cast<UINT>(rc.right - rc.left);
    const UINT h = static_cast<UINT>(rc.bottom - rc.top);
    const HRESULT hr = g.factory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(w, h)),
        &g.target);
    if (FAILED(hr)) {
        return false;
    }
    g.target->CreateBitmap(
        D2D1::SizeU(kWidth, kHeight),
        g.pixels.data(),
        kWidth * sizeof(uint32_t),
        D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)),
        &g.bitmap);
    return g.bitmap != nullptr;
}

void Paint(HWND hwnd) {
    if (!EnsureTarget(hwnd)) {
        return;
    }
    g.bitmap->CopyFromMemory(nullptr, g.pixels.data(), kWidth * sizeof(uint32_t));
    const D2D1_SIZE_F size = g.target->GetSize();
    const float cw = size.width;
    const float ch = size.height;
    const float scale = (cw / kWidth < ch / kHeight) ? cw / kWidth : ch / kHeight;
    const float dw = kWidth * scale;
    const float dh = kHeight * scale;
    const D2D1_RECT_F dest = D2D1::RectF((cw - dw) * 0.5f, (ch - dh) * 0.5f,
                                         (cw - dw) * 0.5f + dw, (ch - dh) * 0.5f + dh);

    g.target->BeginDraw();
    g.target->Clear(kBorderF);
    g.target->DrawBitmap(g.bitmap, dest, 1.f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
    const HRESULT hr = g.target->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        ReleaseTarget();
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE:
        if (g.target) {
            g.target->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp)));
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
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

void PlaceBruce() {
    // Just to the right of the lamp, feet on the red line at the bottom.
    g.x = 36;
    g.y = 194 - kSpriteRows;
    for (int n = 0; n < 48 && Overlaps(g.x, g.y); ++n) {
        g.y -= 1;
    }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    g.charset = LoadFile(L"charset.bin", 2048);
    g.room = LoadFile(L"room0.bin", 1000);
    g.color = LoadFile(L"room0.col", 1000);
    g.frames = LoadFile(L"bruce.bin", static_cast<size_t>(kFrames * 2 * kFrameBytes));
    if (g.charset.empty() || g.room.empty() || g.color.empty() || g.frames.empty()) {
        MessageBoxW(nullptr, L"Could not read data\\charset.bin, room0.bin, room0.col, and bruce.bin.",
                    L"Bruce Lee", MB_ICONERROR);
        return 1;
    }
    g.pixels.assign(static_cast<size_t>(kWidth * kHeight), kPalette[0x0B]);
    PlaceBruce();
    Compose();

    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g.factory))) {
        return 1;
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
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
        Tick();
        InvalidateRect(hwnd, nullptr, FALSE);
        UpdateWindow(hwnd);
        Sleep(16);
    }

    ReleaseTarget();
    g.factory->Release();
    return static_cast<int>(msg.wParam);
}
