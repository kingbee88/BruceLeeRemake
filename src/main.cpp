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
constexpr int kFrames = 7;

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
    int pose = 0;
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

// $537E stores each AND nibble, and $5431 packs the watched slots into $B6.
// Solid patterns 3, 7 and 11-15 become bit 3 while the cell property is 0.
// $1CC7 undoes the move when bit 3 is set. A nonzero property swaps those
// patterns into bit 2. Bit 0 is the edge patterns 1, 4, 5, 6, 7, 9 and 13.
// Falling is skipped when bit 0 or bit 2 is set ($B6 & $05). Statue ink is
// outside both sets, so walking into it is left as it is.
struct Contact {
    bool wall = false;
    bool ground = false;
};

const uint8_t* PoseBytes(int pose, int plane);

void AddPose(Contact& hit, int pose, int sx, int sy) {
    uint8_t mask[kFrameBytes] = {};
    for (int plane = 0; plane < 2; ++plane) {
        const uint8_t* frame = PoseBytes(pose, plane);
        for (int i = 0; i < kFrameBytes; ++i) {
            mask[i] |= frame[i];
        }
    }
    for (int y = 0; y < kSpriteRows; ++y) {
        const int py = sy + y;
        uint8_t cols[40] = {};
        bool used[40] = {};
        for (int x = 0; x < kSpriteCols; ++x) {
            const uint8_t bits = mask[y * 3 + x / 8];
            if ((bits & static_cast<uint8_t>(0x80 >> (x & 7))) == 0) {
                continue;
            }
            const int px = sx + x;
            if (px < 0 || px >= kWidth || py < 0 || py >= kHeight) {
                hit.wall = true;
                continue;
            }
            const int col = px / 8;
            cols[col] |= static_cast<uint8_t>(0x80 >> (px & 7));
            used[col] = true;
        }
        if (py < 0 || py >= kHeight) {
            continue;
        }
        const int row = py / 8;
        const int half = (row & 1) == 0 ? 128 : 0;
        const int rowByte = py & 7;
        for (int col = 0; col < 40; ++col) {
            if (!used[col]) {
                continue;
            }
            const int cell = row * 40 + col;
            const uint8_t tile = g.room[static_cast<size_t>(cell)];
            if (tile == 0xDC) {
                continue;
            }
            const uint8_t bg = g.charset[static_cast<size_t>(tile + half) * 8 + static_cast<size_t>(rowByte)];
            const uint8_t overlapped = static_cast<uint8_t>(cols[col] & bg);
            if (overlapped == 0) {
                continue;
            }
            const int prop = g.color[static_cast<size_t>(cell)] & 7;
            const int nibbles[2] = {overlapped >> 4, overlapped & 0x0F};
            for (const int v : nibbles) {
                if (v == 0) {
                    continue;
                }
                const bool solid = v == 3 || v == 7 || v >= 11;
                const bool edge = v == 1 || v == 4 || v == 5 || v == 6 || v == 7 || v == 9 || v == 13;
                if (solid && prop == 0) {
                    hit.wall = true;
                }
                if ((solid && prop != 0) || edge) {
                    hit.ground = true;
                }
            }
        }
    }
}

// Poses follow the animation selector at $A780.
// 0 right stand (base 2), 1 left stand (base 0),
// 2/3 left walk ($0E, $10), 4/5 right walk ($12, $14), 6 air (base 8).
// Each direction is its own drawing. Mirroring the other side slides the
// solid pixels off the wall and leaves him inside the graphic.
int StandPose() {
    return g.facing < 0 ? 1 : 0;
}

int WalkPose() {
    const int step = (g.tick / 6) & 1;
    return g.facing < 0 ? (2 + step) : (4 + step);
}

const uint8_t* PoseBytes(int pose, int plane) {
    return g.frames.data() + static_cast<size_t>(pose * 2 + plane) * kFrameBytes;
}

bool SpritePixel(const uint8_t* frame, int x, int y) {
    const uint8_t bits = frame[y * 3 + x / 8];
    return (bits & static_cast<uint8_t>(0x80 >> (x & 7))) != 0;
}

// Both strides are tested while he is walking, so the wider one cannot
// step into a wall that the other stride would be unable to leave.
Contact Probe(int sx, int sy) {
    Contact hit;
    if (g.vy != 0) {
        AddPose(hit, 6, sx, sy);
    } else if (g.tick > 0) {
        const int base = g.facing < 0 ? 2 : 4;
        AddPose(hit, base, sx, sy);
        AddPose(hit, base + 1, sx, sy);
    } else {
        AddPose(hit, StandPose(), sx, sy);
    }
    return hit;
}

bool PoseWall(int pose, int sx, int sy) {
    Contact hit;
    AddPose(hit, pose, sx, sy);
    return hit.wall;
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
        const uint8_t* frame = PoseBytes(g.pose, plane);
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
        const Contact hit = Probe(g.x, g.y + step);
        // Down stops on the don't-fall flags as well as a wall. Up is
        // cancelled only by the solid flag, the same bit $1CC7 pushes out.
        if (step > 0 ? (hit.wall || hit.ground) : hit.wall) {
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
    if (dx != 0) {
        g.tick += 1;
    } else {
        g.tick = 0;
    }
    if (dx != 0 && !Probe(g.x + dx, g.y).wall) {
        g.x += dx;
    }

    const Contact below = Probe(g.x, g.y + 1);
    const bool grounded = below.wall || below.ground;
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
    if (g.vy != 0) {
        g.pose = 6;
    } else if (g.tick > 0) {
        g.pose = WalkPose();
    } else {
        g.pose = StandPose();
    }
    // A new pose can overlap a wall the previous pose cleared.
    // $1CC7 pushes him back out when the solid flag is set.
    if (PoseWall(g.pose, g.x, g.y)) {
        const int prefer = g.facing < 0 ? 1 : -1;
        const int start = g.x;
        for (int i = 1; i <= kSpriteCols && PoseWall(g.pose, g.x, g.y); ++i) {
            if (!PoseWall(g.pose, start + prefer * i, g.y)) {
                g.x = start + prefer * i;
            } else if (!PoseWall(g.pose, start - prefer * i, g.y)) {
                g.x = start - prefer * i;
            }
        }
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
    for (int n = 0; n < 48 && PoseWall(g.pose, g.x, g.y); ++n) {
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
