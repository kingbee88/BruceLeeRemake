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
#include <roapi.h>
#include <xinput.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>
#include <windows.gaming.input.h>

#include "pad.h"

#include "config.h"

#include <string>
#include <vector>

#pragma comment(lib, "runtimeobject.lib")
#pragma comment(lib, "xinput.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace {

namespace Wgi = ABI::Windows::Gaming::Input;
namespace Collections = ABI::Windows::Foundation::Collections;

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Wrappers::HString;
using Microsoft::WRL::Wrappers::HStringReference;

// The same dead zone XInput documents for the left stick, so the stick
// starts acting at the same tilt it did when every pad was read that way.
constexpr double kStickDead =
    static_cast<double>(XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) / 32767.0;

// XInputGetState on an empty slot can stall. A slot that just answered
// "not connected" is left alone for a second.
ULONGLONG gXInputQuiet[4] = {};

enum class PadApi { Unknown, Wgi, XInput };
PadApi gApi = PadApi::Unknown;
bool gRoReady = false;
bool gRoOwned = false;

ComPtr<Wgi::IGamepadStatics> gGamepadStatics;
ComPtr<Wgi::IRawGameControllerStatics> gRawStatics;
ComPtr<IDirectInput8W> gDirectInput;
ComPtr<Wgi::IGamepad> gActiveWgi;
ComPtr<Wgi::IRawGameController> gActiveRaw;
int gActiveAxes = 0;
int gActiveButtons = 0;
int gActiveSwitches = 0;
std::vector<int> gActiveLabels;
// A controller DirectInput knows about and Windows.Gaming.Input does not.
ComPtr<IDirectInputDevice8W> gActiveDirect;
bool gActiveDirectOn = false;
LONG gDirectXMin = 0;
LONG gDirectXMax = 65535;
LONG gDirectYMin = 0;
LONG gDirectYMax = 65535;
bool gDirectHasY = false;
int gDirectButtons = 0;
int gDirectPovs = 0;
int gActiveXInput = -1;
std::wstring gActiveId;
ULONGLONG gNextPadScan = 0;
bool haveCached = false;
PadInput cached{};

struct ListedPad {
    std::wstring id;
    std::wstring name;
    ComPtr<Wgi::IGamepad> gamepad;
    // Set for a controller Windows does not treat as an Xbox-style gamepad.
    // The stick and its buttons are read from here.
    ComPtr<Wgi::IRawGameController> raw;
    int xinput = -1;
    // Set when this controller is not in Windows.Gaming.Input. instance is
    // the DirectInput id used to open it.
    bool directInput = false;
    GUID instance{};
    UINT16 vid = 0;
    UINT16 pid = 0;
    int axisCount = 0;
    int buttonCount = 0;
    int switchCount = 0;
    std::vector<int> labels;
};

GamepadList gShown;

std::wstring FromHString(const HString& value) {
    UINT32 length = 0;
    const wchar_t* raw = WindowsGetStringRawBuffer(value.Get(), &length);
    if (raw == nullptr || length == 0) {
        return {};
    }
    return std::wstring(raw, length);
}

std::wstring CleanId(std::wstring id) {
    std::wstring out;
    out.reserve(id.size());
    for (wchar_t ch : id) {
        if (ch >= 32 && ch != 0x7f) {
            out.push_back(ch);
        }
    }
    if (out.size() > 1024) {
        out.resize(1024);
    }
    return out;
}

bool ForegroundIsOurs() {
    const HWND fg = GetForegroundWindow();
    if (fg == nullptr) {
        return false;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

void Take(PadInput& pad, const XINPUT_GAMEPAD& gamepad) {
    const WORD buttons = gamepad.wButtons;
    if ((buttons & XINPUT_GAMEPAD_DPAD_LEFT) != 0) {
        pad.left = true;
    }
    if ((buttons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0) {
        pad.right = true;
    }
    if ((buttons & XINPUT_GAMEPAD_DPAD_UP) != 0) {
        pad.up = true;
    }
    if ((buttons & XINPUT_GAMEPAD_DPAD_DOWN) != 0) {
        pad.down = true;
    }
    if ((buttons & XINPUT_GAMEPAD_A) != 0) {
        pad.fire = true;
    }
    // The menu button. Xbox pads report it as Start.
    if ((buttons & XINPUT_GAMEPAD_START) != 0) {
        pad.menu = true;
    }
    // The View button. Xbox pads report it as Back.
    if ((buttons & XINPUT_GAMEPAD_BACK) != 0) {
        pad.view = true;
    }
    if ((buttons & XINPUT_GAMEPAD_Y) != 0) {
        pad.y = true;
    }

    // The stick is a direction once it leaves the dead zone XInput defines
    // for the left thumb. Past that, each axis is on or off, same as a hat.
    const SHORT x = gamepad.sThumbLX;
    const SHORT y = gamepad.sThumbLY;
    if (x < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
        pad.left = true;
    } else if (x > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
        pad.right = true;
    }
    if (y > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
        pad.up = true;
    } else if (y < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
        pad.down = true;
    }
}

void ApplyWgi(PadInput& pad, const Wgi::GamepadReading& reading) {
    const Wgi::GamepadButtons buttons = reading.Buttons;
    if ((buttons & Wgi::GamepadButtons_DPadLeft) != 0) {
        pad.left = true;
    }
    if ((buttons & Wgi::GamepadButtons_DPadRight) != 0) {
        pad.right = true;
    }
    if ((buttons & Wgi::GamepadButtons_DPadUp) != 0) {
        pad.up = true;
    }
    if ((buttons & Wgi::GamepadButtons_DPadDown) != 0) {
        pad.down = true;
    }
    if ((buttons & Wgi::GamepadButtons_A) != 0) {
        pad.fire = true;
    }
    if ((buttons & Wgi::GamepadButtons_Menu) != 0) {
        pad.menu = true;
    }
    if ((buttons & Wgi::GamepadButtons_View) != 0) {
        pad.view = true;
    }
    if ((buttons & Wgi::GamepadButtons_Y) != 0) {
        pad.y = true;
    }

    const double x = reading.LeftThumbstickX;
    const double y = reading.LeftThumbstickY;
    if (x < -kStickDead) {
        pad.left = true;
    } else if (x > kStickDead) {
        pad.right = true;
    }
    if (y > kStickDead) {
        pad.up = true;
    } else if (y < -kStickDead) {
        pad.down = true;
    }
}

void ApplyLabel(PadInput& pad, int label) {
    switch (static_cast<Wgi::GameControllerButtonLabel>(label)) {
    case Wgi::GameControllerButtonLabel_XboxA:
    case Wgi::GameControllerButtonLabel_LetterA:
    case Wgi::GameControllerButtonLabel_Cross:
        pad.fire = true;
        break;
    case Wgi::GameControllerButtonLabel_XboxY:
    case Wgi::GameControllerButtonLabel_LetterY:
    case Wgi::GameControllerButtonLabel_Triangle:
        pad.y = true;
        break;
    case Wgi::GameControllerButtonLabel_XboxMenu:
    case Wgi::GameControllerButtonLabel_XboxStart:
    case Wgi::GameControllerButtonLabel_Menu:
    case Wgi::GameControllerButtonLabel_Start:
    case Wgi::GameControllerButtonLabel_Options:
    case Wgi::GameControllerButtonLabel_Plus:
        pad.menu = true;
        break;
    case Wgi::GameControllerButtonLabel_XboxView:
    case Wgi::GameControllerButtonLabel_XboxBack:
    case Wgi::GameControllerButtonLabel_View:
    case Wgi::GameControllerButtonLabel_Back:
    case Wgi::GameControllerButtonLabel_Select:
    case Wgi::GameControllerButtonLabel_Share:
    case Wgi::GameControllerButtonLabel_Minus:
        pad.view = true;
        break;
    case Wgi::GameControllerButtonLabel_XboxUp:
    case Wgi::GameControllerButtonLabel_Up:
        pad.up = true;
        break;
    case Wgi::GameControllerButtonLabel_XboxDown:
    case Wgi::GameControllerButtonLabel_Down:
        pad.down = true;
        break;
    case Wgi::GameControllerButtonLabel_XboxLeft:
    case Wgi::GameControllerButtonLabel_Left:
        pad.left = true;
        break;
    case Wgi::GameControllerButtonLabel_XboxRight:
    case Wgi::GameControllerButtonLabel_Right:
        pad.right = true;
        break;
    default:
        break;
    }
}

// A hat reports one of eight directions. Diagonals set both axes.
void ApplySwitch(PadInput& pad, int position) {
    if (position == Wgi::GameControllerSwitchPosition_Up ||
        position == Wgi::GameControllerSwitchPosition_UpLeft ||
        position == Wgi::GameControllerSwitchPosition_UpRight) {
        pad.up = true;
    }
    if (position == Wgi::GameControllerSwitchPosition_Down ||
        position == Wgi::GameControllerSwitchPosition_DownLeft ||
        position == Wgi::GameControllerSwitchPosition_DownRight) {
        pad.down = true;
    }
    if (position == Wgi::GameControllerSwitchPosition_Left ||
        position == Wgi::GameControllerSwitchPosition_UpLeft ||
        position == Wgi::GameControllerSwitchPosition_DownLeft) {
        pad.left = true;
    }
    if (position == Wgi::GameControllerSwitchPosition_Right ||
        position == Wgi::GameControllerSwitchPosition_UpRight ||
        position == Wgi::GameControllerSwitchPosition_DownRight) {
        pad.right = true;
    }
}

// axis 0 is horizontal, 0 at the left. axis 1 is vertical. On a plain stick
// the value runs 0..1 and the low half is up. A stick that reports negative
// values is centered at 0, and the high half is up, as on an Xbox stick.
void ApplyRawAxes(PadInput& pad, double x, double y, bool hasY) {
    const bool centered = x < 0.0 || y < 0.0 || x > 1.0 || y > 1.0;
    const double origin = centered ? 0.0 : 0.5;
    const double xRel = x - origin;
    if (xRel < -kStickDead) {
        pad.left = true;
    } else if (xRel > kStickDead) {
        pad.right = true;
    }
    if (!hasY) {
        return;
    }
    const double yRel = y - origin;
    if (centered) {
        if (yRel > kStickDead) {
            pad.up = true;
        } else if (yRel < -kStickDead) {
            pad.down = true;
        }
    } else if (yRel < -kStickDead) {
        pad.up = true;
    } else if (yRel > kStickDead) {
        pad.down = true;
    }
}

void CloseDirectInput() {
    if (gActiveDirect) {
        gActiveDirect->Unacquire();
        gActiveDirect.Reset();
    }
    gActiveDirectOn = false;
    gDirectHasY = false;
    gDirectButtons = 0;
    gDirectPovs = 0;
}

bool HasActive() {
    return gActiveWgi || gActiveRaw || gActiveDirectOn || gActiveXInput >= 0;
}

void ClearActive() {
    gActiveWgi.Reset();
    gActiveRaw.Reset();
    gActiveAxes = 0;
    gActiveButtons = 0;
    gActiveSwitches = 0;
    gActiveLabels.clear();
    CloseDirectInput();
    gActiveXInput = -1;
    gActiveId.clear();
}

bool ActiveStillWorks() {
    if (gActiveWgi) {
        Wgi::GamepadReading reading{};
        return SUCCEEDED(gActiveWgi->GetCurrentReading(&reading));
    }
    if (gActiveRaw) {
        boolean buttons[64] = {};
        Wgi::GameControllerSwitchPosition switches[8] = {};
        double axes[8] = {};
        UINT64 timestamp = 0;
        const int buttonCount = gActiveButtons < 64 ? gActiveButtons : 64;
        const int switchCount = gActiveSwitches < 8 ? gActiveSwitches : 8;
        const int axisCount = gActiveAxes < 8 ? gActiveAxes : 8;
        return SUCCEEDED(gActiveRaw->GetCurrentReading(
            static_cast<UINT32>(buttonCount), buttonCount > 0 ? buttons : nullptr,
            static_cast<UINT32>(switchCount), switchCount > 0 ? switches : nullptr,
            static_cast<UINT32>(axisCount), axisCount > 0 ? axes : nullptr, &timestamp));
    }
    if (gActiveDirectOn && gActiveDirect) {
        HRESULT hr = gActiveDirect->Poll();
        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
            hr = gActiveDirect->Acquire();
        }
        if (FAILED(hr) && hr != DI_NOEFFECT) {
            return false;
        }
        DIJOYSTATE2 state{};
        hr = gActiveDirect->GetDeviceState(sizeof(state), &state);
        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
            if (FAILED(gActiveDirect->Acquire())) {
                return false;
            }
            hr = gActiveDirect->GetDeviceState(sizeof(state), &state);
        }
        return SUCCEEDED(hr);
    }
    if (gActiveXInput >= 0) {
        const ULONGLONG now = GetTickCount64();
        if (now < gXInputQuiet[gActiveXInput]) {
            return false;
        }
        XINPUT_STATE state{};
        return XInputGetState(static_cast<DWORD>(gActiveXInput), &state) == ERROR_SUCCESS;
    }
    return false;
}

bool ReadActiveInto(PadInput& pad) {
    if (gActiveWgi) {
        Wgi::GamepadReading reading{};
        if (FAILED(gActiveWgi->GetCurrentReading(&reading))) {
            return false;
        }
        ApplyWgi(pad, reading);
        return true;
    }
    if (gActiveRaw) {
        boolean buttons[64] = {};
        Wgi::GameControllerSwitchPosition switches[8] = {};
        double axes[8] = {};
        UINT64 timestamp = 0;
        const int buttonCount = gActiveButtons < 64 ? gActiveButtons : 64;
        const int switchCount = gActiveSwitches < 8 ? gActiveSwitches : 8;
        const int axisCount = gActiveAxes < 8 ? gActiveAxes : 8;
        if (FAILED(gActiveRaw->GetCurrentReading(
                static_cast<UINT32>(buttonCount), buttonCount > 0 ? buttons : nullptr,
                static_cast<UINT32>(switchCount), switchCount > 0 ? switches : nullptr,
                static_cast<UINT32>(axisCount), axisCount > 0 ? axes : nullptr, &timestamp))) {
            return false;
        }
        for (int i = 0; i < buttonCount; ++i) {
            if (!buttons[i]) {
                continue;
            }
            const int label = i < static_cast<int>(gActiveLabels.size()) ? gActiveLabels[static_cast<size_t>(i)] : 0;
            if (label == Wgi::GameControllerButtonLabel_None) {
                pad.fire = true;
            } else {
                ApplyLabel(pad, label);
            }
        }
        for (int i = 0; i < switchCount; ++i) {
            ApplySwitch(pad, switches[i]);
        }
        if (axisCount > 0) {
            ApplyRawAxes(pad, axes[0], axisCount > 1 ? axes[1] : 0.5, axisCount > 1);
        }
        return true;
    }
    if (gActiveDirectOn && gActiveDirect) {
        HRESULT hr = gActiveDirect->Poll();
        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
            hr = gActiveDirect->Acquire();
        }
        DIJOYSTATE2 state{};
        hr = gActiveDirect->GetDeviceState(sizeof(state), &state);
        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
            if (FAILED(gActiveDirect->Acquire())) {
                return false;
            }
            hr = gActiveDirect->GetDeviceState(sizeof(state), &state);
        }
        if (FAILED(hr)) {
            return false;
        }
        const auto tilt = [](LONG value, LONG lo, LONG hi, bool& low, bool& high) {
            const LONG span = hi - lo;
            if (span <= 0) {
                return;
            }
            const double mid = (static_cast<double>(lo) + static_cast<double>(hi)) * 0.5;
            const double dead = static_cast<double>(span) * 0.5 * kStickDead;
            if (static_cast<double>(value) < mid - dead) {
                low = true;
            } else if (static_cast<double>(value) > mid + dead) {
                high = true;
            }
        };
        bool lowX = false;
        bool highX = false;
        bool lowY = false;
        bool highY = false;
        tilt(state.lX, gDirectXMin, gDirectXMax, lowX, highX);
        if (lowX) {
            pad.left = true;
        }
        if (highX) {
            pad.right = true;
        }
        // Low Y is up, matching the usual DirectInput stick.
        if (gDirectHasY) {
            tilt(state.lY, gDirectYMin, gDirectYMax, lowY, highY);
            if (lowY) {
                pad.up = true;
            }
            if (highY) {
                pad.down = true;
            }
        }
        for (int i = 0; i < gDirectPovs; ++i) {
            const DWORD pov = state.rgdwPOV[i];
            if (pov == 0xFFFFFFFF) {
                continue;
            }
            if (pov <= 4500 || pov >= 31500) {
                pad.up = true;
            }
            if (pov >= 4500 && pov <= 13500) {
                pad.right = true;
            }
            if (pov >= 13500 && pov <= 22500) {
                pad.down = true;
            }
            if (pov >= 22500 && pov <= 31500) {
                pad.left = true;
            }
        }
        for (int i = 0; i < gDirectButtons; ++i) {
            if ((state.rgbButtons[i] & 0x80) != 0) {
                pad.fire = true;
            }
        }
        return true;
    }
    if (gActiveXInput >= 0) {
        XINPUT_STATE state{};
        if (XInputGetState(static_cast<DWORD>(gActiveXInput), &state) != ERROR_SUCCESS) {
            gXInputQuiet[gActiveXInput] = GetTickCount64() + 1000;
            return false;
        }
        Take(pad, state.Gamepad);
        return true;
    }
    return true;
}

bool LoadFactories() {
    const HRESULT pads = RoGetActivationFactory(
        HStringReference(L"Windows.Gaming.Input.Gamepad").Get(),
        __uuidof(Wgi::IGamepadStatics),
        reinterpret_cast<void**>(gGamepadStatics.GetAddressOf()));
    if (FAILED(pads)) {
        gGamepadStatics.Reset();
        return false;
    }
    const HRESULT raw = RoGetActivationFactory(
        HStringReference(L"Windows.Gaming.Input.RawGameController").Get(),
        __uuidof(Wgi::IRawGameControllerStatics),
        reinterpret_cast<void**>(gRawStatics.GetAddressOf()));
    if (FAILED(raw)) {
        gGamepadStatics.Reset();
        gRawStatics.Reset();
        return false;
    }
    return true;
}

bool UseWgi() {
    if (gApi == PadApi::Wgi) {
        return true;
    }
    if (gApi == PadApi::XInput) {
        return false;
    }
    if (gRoReady && LoadFactories()) {
        gApi = PadApi::Wgi;
        return true;
    }
    gApi = PadApi::XInput;
    gGamepadStatics.Reset();
    gRawStatics.Reset();
    return false;
}

void FinishLabels(std::vector<ListedPad>& items) {
    std::vector<std::wstring> rawNames(items.size());
    std::vector<std::wstring> rawIds(items.size());
    for (size_t i = 0; i < items.size(); ++i) {
        rawNames[i] = items[i].name.empty() ? L"Gamepad" : items[i].name;
        rawIds[i] = items[i].id.empty() ? (L"pad:" + std::to_wstring(i)) : CleanId(items[i].id);
    }
    for (size_t i = 0; i < items.size(); ++i) {
        int nameCopies = 0;
        int idCopies = 0;
        for (size_t j = 0; j < items.size(); ++j) {
            if (rawNames[j] == rawNames[i]) {
                nameCopies++;
            }
        }
        for (size_t j = 0; j < i; ++j) {
            if (rawIds[j] == rawIds[i]) {
                idCopies++;
            }
        }
        if (nameCopies > 1) {
            int index = 1;
            for (size_t j = 0; j < i; ++j) {
                if (rawNames[j] == rawNames[i]) {
                    index++;
                }
            }
            items[i].name = rawNames[i] + L" (" + std::to_wstring(index) + L")";
        } else {
            items[i].name = rawNames[i];
        }
        if (idCopies > 0) {
            items[i].id = rawIds[i] + L"#" + std::to_wstring(idCopies + 1);
        } else {
            items[i].id = rawIds[i];
        }
    }
}

struct EnumResult {
    bool ok = false;
    std::vector<ListedPad> items;
};

// The name joy.cpl shows. Windows.Gaming.Input often calls a plain stick
// "HID-compliant game controller". The product id is packed as pid then vid.
struct ProductNameQuery {
    UINT16 vid = 0;
    UINT16 pid = 0;
    bool found = false;
    wchar_t name[128] = {};
};

BOOL CALLBACK ProductNameCallback(LPCDIDEVICEINSTANCEW inst, LPVOID ctx) {
    auto* query = static_cast<ProductNameQuery*>(ctx);
    const DWORD packed = inst->guidProduct.Data1;
    if (static_cast<UINT16>(packed) != query->vid || static_cast<UINT16>(packed >> 16) != query->pid) {
        return DIENUM_CONTINUE;
    }
    wcsncpy_s(query->name, inst->tszProductName, _TRUNCATE);
    query->found = query->name[0] != L'\0';
    return DIENUM_STOP;
}

bool EnsureDirectInput() {
    if (gDirectInput) {
        return true;
    }
    return SUCCEEDED(DirectInput8Create(
        GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W,
        reinterpret_cast<void**>(gDirectInput.GetAddressOf()), nullptr));
}

std::wstring DirectInputProductName(UINT16 vid, UINT16 pid) {
    if ((vid == 0 && pid == 0) || !EnsureDirectInput()) {
        return {};
    }
    ProductNameQuery query;
    query.vid = vid;
    query.pid = pid;
    gDirectInput->EnumDevices(DI8DEVCLASS_GAMECTRL, ProductNameCallback, &query, DIEDFL_ATTACHEDONLY);
    if (!query.found) {
        return {};
    }
    return query.name;
}

struct DirectDevice {
    GUID instance{};
    GUID product{};
    wchar_t name[MAX_PATH] = {};
};

BOOL CALLBACK CollectDirectInput(LPCDIDEVICEINSTANCEW inst, LPVOID ctx) {
    auto* found = static_cast<std::vector<DirectDevice>*>(ctx);
    if (found->size() >= 64) {
        return DIENUM_STOP;
    }
    DirectDevice dev;
    dev.instance = inst->guidInstance;
    dev.product = inst->guidProduct;
    wcsncpy_s(dev.name, inst->tszProductName, _TRUNCATE);
    found->push_back(dev);
    return DIENUM_CONTINUE;
}

// XInput pads also show up in DirectInput, under a different product id.
// The raw-input path of those aliases contains IG_.
std::vector<DWORD> XInputProductIds() {
    std::vector<DWORD> ids;
    UINT count = 0;
    if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1) || count == 0) {
        return ids;
    }
    std::vector<RAWINPUTDEVICELIST> list(count);
    if (GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1)) {
        return ids;
    }
    for (UINT i = 0; i < count; ++i) {
        if (list[i].dwType != RIM_TYPEHID) {
            continue;
        }
        UINT bytes = 0;
        GetRawInputDeviceInfoW(list[i].hDevice, RIDI_DEVICENAME, nullptr, &bytes);
        if (bytes < sizeof(wchar_t)) {
            continue;
        }
        // The size is in bytes on some Windows versions and characters on
        // others. A buffer of that many wchar_t is large enough for either,
        // and the same value is handed back as the capacity.
        std::vector<wchar_t> name(static_cast<size_t>(bytes) + 1);
        if (GetRawInputDeviceInfoW(list[i].hDevice, RIDI_DEVICENAME, name.data(), &bytes) == static_cast<UINT>(-1)) {
            continue;
        }
        if (wcsstr(name.data(), L"IG_") == nullptr) {
            continue;
        }
        const wchar_t* vidText = wcsstr(name.data(), L"VID_");
        const wchar_t* pidText = wcsstr(name.data(), L"PID_");
        if (vidText == nullptr || pidText == nullptr) {
            continue;
        }
        const unsigned long vid = wcstoul(vidText + 4, nullptr, 16);
        const unsigned long pid = wcstoul(pidText + 4, nullptr, 16);
        ids.push_back(static_cast<DWORD>(vid) | (static_cast<DWORD>(pid) << 16));
    }
    return ids;
}

bool ListedVidPid(const std::vector<ListedPad>& items, UINT16 vid, UINT16 pid) {
    if (vid == 0 && pid == 0) {
        return false;
    }
    for (const ListedPad& item : items) {
        if (item.vid == vid && item.pid == pid) {
            return true;
        }
    }
    return false;
}

std::wstring DirectInputId(const GUID& guid) {
    wchar_t buf[80];
    swprintf_s(buf, L"di:{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
               guid.Data1, guid.Data2, guid.Data3,
               guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
               guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    return buf;
}

// Controllers joy.cpl lists that Windows.Gaming.Input does not. Xbox pads are
// already in the list, so their DirectInput aliases are left out.
void AddDirectInputOnly(std::vector<ListedPad>& items) {
    if (!EnsureDirectInput()) {
        return;
    }
    std::vector<DirectDevice> found;
    if (FAILED(gDirectInput->EnumDevices(DI8DEVCLASS_GAMECTRL, CollectDirectInput, &found, DIEDFL_ATTACHEDONLY))) {
        return;
    }
    const std::vector<DWORD> xinputIds = XInputProductIds();
    for (const DirectDevice& dev : found) {
        bool xinput = false;
        for (DWORD id : xinputIds) {
            if (id == dev.product.Data1) {
                xinput = true;
                break;
            }
        }
        if (xinput) {
            continue;
        }
        const UINT16 vid = static_cast<UINT16>(dev.product.Data1);
        const UINT16 pid = static_cast<UINT16>(dev.product.Data1 >> 16);
        if (ListedVidPid(items, vid, pid)) {
            continue;
        }
        ListedPad item;
        item.directInput = true;
        item.instance = dev.instance;
        item.vid = vid;
        item.pid = pid;
        item.name = dev.name;
        item.id = DirectInputId(dev.instance);
        items.push_back(std::move(item));
    }
}

void ReadAxisRange(DWORD offset, LONG& low, LONG& high) {
    DIPROPRANGE range{};
    range.diph.dwSize = sizeof(range);
    range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    range.diph.dwObj = offset;
    range.diph.dwHow = DIPH_BYOFFSET;
    if (!gActiveDirect ||
        FAILED(gActiveDirect->GetProperty(DIPROP_RANGE, &range.diph)) ||
        range.lMin >= range.lMax) {
        low = 0;
        high = 65535;
        return;
    }
    low = range.lMin;
    high = range.lMax;
}

bool OpenDirectInput(const GUID& instance) {
    CloseDirectInput();
    if (!EnsureDirectInput()) {
        return false;
    }
    if (FAILED(gDirectInput->CreateDevice(instance, gActiveDirect.GetAddressOf(), nullptr))) {
        return false;
    }
    if (FAILED(gActiveDirect->SetDataFormat(&c_dfDIJoystick2))) {
        gActiveDirect.Reset();
        return false;
    }
    gActiveDirect->SetCooperativeLevel(GetDesktopWindow(), DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
    DIDEVCAPS caps{};
    caps.dwSize = sizeof(caps);
    if (SUCCEEDED(gActiveDirect->GetCapabilities(&caps))) {
        gDirectButtons = static_cast<int>(caps.dwButtons);
        if (gDirectButtons > 128) {
            gDirectButtons = 128;
        }
        gDirectPovs = static_cast<int>(caps.dwPOVs);
        if (gDirectPovs > 4) {
            gDirectPovs = 4;
        }
        gDirectHasY = caps.dwAxes >= 2;
    }
    ReadAxisRange(DIJOFS_X, gDirectXMin, gDirectXMax);
    ReadAxisRange(DIJOFS_Y, gDirectYMin, gDirectYMax);
    gActiveDirect->Acquire();
    gActiveDirectOn = true;
    return true;
}

void FillIdentity(Wgi::IRawGameController* raw, ListedPad& item, unsigned fallbackIndex) {
    if (raw == nullptr) {
        return;
    }
    raw->get_HardwareVendorId(&item.vid);
    raw->get_HardwareProductId(&item.pid);
    ComPtr<Wgi::IRawGameController2> raw2;
    raw->QueryInterface(IID_PPV_ARGS(raw2.GetAddressOf()));
    if (raw2) {
        HString id;
        HString name;
        if (SUCCEEDED(raw2->get_NonRoamableId(id.GetAddressOf()))) {
            item.id = FromHString(id);
        }
        if (SUCCEEDED(raw2->get_DisplayName(name.GetAddressOf()))) {
            item.name = FromHString(name);
        }
    }
    if (item.id.empty()) {
        wchar_t buf[96];
        swprintf_s(buf, L"wgi:vid_%04X&pid_%04X#%u", item.vid, item.pid, fallbackIndex);
        item.id = buf;
    }
}

bool AlreadyListed(const std::vector<ListedPad>& items, const std::wstring& id) {
    if (id.empty()) {
        return false;
    }
    for (const ListedPad& item : items) {
        if (item.id == id) {
            return true;
        }
    }
    return false;
}

void ReadRawLayout(Wgi::IRawGameController* raw, ListedPad& item) {
    INT32 axes = 0;
    INT32 buttons = 0;
    INT32 switches = 0;
    raw->get_AxisCount(&axes);
    raw->get_ButtonCount(&buttons);
    raw->get_SwitchCount(&switches);
    if (axes < 0) {
        axes = 0;
    }
    if (buttons < 0) {
        buttons = 0;
    }
    if (switches < 0) {
        switches = 0;
    }
    if (axes > 8) {
        axes = 8;
    }
    if (buttons > 64) {
        buttons = 64;
    }
    if (switches > 8) {
        switches = 8;
    }
    item.axisCount = axes;
    item.buttonCount = buttons;
    item.switchCount = switches;
    item.labels.resize(static_cast<size_t>(buttons));
    for (int i = 0; i < buttons; ++i) {
        Wgi::GameControllerButtonLabel label = Wgi::GameControllerButtonLabel_None;
        if (SUCCEEDED(raw->GetButtonLabel(i, &label))) {
            item.labels[static_cast<size_t>(i)] = label;
        }
    }
}

EnumResult EnumWgi() {
    EnumResult result;
    if (!gGamepadStatics || !gRawStatics) {
        return result;
    }
    ComPtr<Collections::IVectorView<Wgi::Gamepad*>> view;
    const bool padsOk = SUCCEEDED(gGamepadStatics->get_Gamepads(view.GetAddressOf())) && view;
    unsigned count = 0;
    if (padsOk && SUCCEEDED(view->get_Size(&count))) {
        if (count > 64) {
            count = 64;
        }
        result.ok = true;
        for (unsigned i = 0; i < count; ++i) {
            ComPtr<Wgi::IGamepad> pad;
            if (FAILED(view->GetAt(i, pad.GetAddressOf())) || !pad) {
                continue;
            }
            ListedPad item;
            item.gamepad = pad;
            ComPtr<Wgi::IGameController> controller;
            ComPtr<Wgi::IRawGameController> raw;
            if (SUCCEEDED(pad.As(&controller)) && controller &&
                SUCCEEDED(gRawStatics->FromGameController(controller.Get(), raw.GetAddressOf())) && raw) {
                FillIdentity(raw.Get(), item, i);
            }
            if (item.id.empty()) {
                wchar_t buf[32];
                swprintf_s(buf, L"wgi:gamepad#%u", i);
                item.id = buf;
            }
            result.items.push_back(std::move(item));
        }
    }
    ComPtr<Collections::IVectorView<Wgi::RawGameController*>> rawView;
    unsigned rawCount = 0;
    if (SUCCEEDED(gRawStatics->get_RawGameControllers(rawView.GetAddressOf())) && rawView &&
        SUCCEEDED(rawView->get_Size(&rawCount))) {
        if (rawCount > 64) {
            rawCount = 64;
        }
        result.ok = true;
        for (unsigned i = 0; i < rawCount; ++i) {
            ComPtr<Wgi::IRawGameController> raw;
            if (FAILED(rawView->GetAt(i, raw.GetAddressOf())) || !raw) {
                continue;
            }
            ListedPad item;
            FillIdentity(raw.Get(), item, i);
            if (AlreadyListed(result.items, item.id)) {
                continue;
            }
            item.raw = raw;
            ReadRawLayout(raw.Get(), item);
            const std::wstring product = DirectInputProductName(item.vid, item.pid);
            if (!product.empty()) {
                item.name = product;
            }
            result.items.push_back(std::move(item));
        }
    }
    if (!result.ok) {
        return result;
    }
    AddDirectInputOnly(result.items);
    FinishLabels(result.items);
    return result;
}

EnumResult EnumXInput() {
    EnumResult result;
    result.ok = true;
    const ULONGLONG now = GetTickCount64();
    for (DWORD i = 0; i < 4; ++i) {
        if (now < gXInputQuiet[i]) {
            continue;
        }
        XINPUT_STATE state{};
        if (XInputGetState(i, &state) != ERROR_SUCCESS) {
            gXInputQuiet[i] = now + 1000;
            continue;
        }
        ListedPad item;
        item.xinput = static_cast<int>(i);
        item.id = L"xinput:" + std::to_wstring(i);
        item.name = L"Gamepad " + std::to_wstring(i + 1);
        result.items.push_back(std::move(item));
    }
    AddDirectInputOnly(result.items);
    FinishLabels(result.items);
    return result;
}

EnumResult Enumerate() {
    if (UseWgi()) {
        return EnumWgi();
    }
    return EnumXInput();
}

void Bind(const std::vector<ListedPad>& items, int active) {
    if (active < 0 || active >= static_cast<int>(items.size())) {
        ClearActive();
        return;
    }
    const ListedPad& item = items[static_cast<size_t>(active)];
    if (item.id == gActiveId && ActiveStillWorks()) {
        return;
    }
    ClearActive();
    gActiveId = item.id;
    if (item.directInput) {
        OpenDirectInput(item.instance);
        return;
    }
    gActiveWgi = item.gamepad;
    gActiveRaw = item.gamepad ? nullptr : item.raw;
    gActiveAxes = item.axisCount;
    gActiveButtons = item.buttonCount;
    gActiveSwitches = item.switchCount;
    gActiveLabels = item.labels;
    gActiveXInput = item.xinput;
}

GamepadList ToList(const std::vector<ListedPad>& items, int active) {
    GamepadList list;
    list.active = active;
    list.items.reserve(items.size());
    for (const ListedPad& item : items) {
        GamepadChoice choice;
        choice.id = item.id;
        choice.name = item.name;
        list.items.push_back(std::move(choice));
    }
    return list;
}

void ReleasePadRuntime() {
    ClearActive();
    gGamepadStatics.Reset();
    gRawStatics.Reset();
    gDirectInput.Reset();
    gShown = {};
    gApi = PadApi::Unknown;
    if (gRoOwned) {
        RoUninitialize();
        gRoOwned = false;
    }
    gRoReady = false;
}

}  // namespace

PadSession::PadSession() {
    const HRESULT hr = RoInitialize(RO_INIT_SINGLETHREADED);
    gRoOwned = hr == S_OK;
    gRoReady = SUCCEEDED(hr);
}

PadSession::~PadSession() {
    ReleasePadRuntime();
}

void BeginPadFrame() {
    haveCached = false;
}

GamepadList SyncGamepads() {
    const EnumResult enumerated = Enumerate();
    if (!enumerated.ok) {
        return gShown;
    }
    int active = -1;
    if (!gConfig.gamepad.empty()) {
        for (int i = 0; i < static_cast<int>(enumerated.items.size()); ++i) {
            if (enumerated.items[static_cast<size_t>(i)].id == gConfig.gamepad) {
                active = i;
                break;
            }
        }
    }
    if (active < 0 && !enumerated.items.empty()) {
        active = 0;
        gConfig.gamepad = enumerated.items[0].id;
    }
    Bind(enumerated.items, active);
    gShown = ToList(enumerated.items, active);
    gNextPadScan = GetTickCount64() + 1000;
    return gShown;
}

PadInput ReadPad() {
    if (haveCached) {
        return cached;
    }
    haveCached = true;
    cached = {};
    // Another program is in front. Leave the sticks with that program.
    if (!ForegroundIsOurs()) {
        return cached;
    }
    if (HasActive()) {
        if (!ReadActiveInto(cached)) {
            // The chosen pad is gone. Pick it again if it is still there,
            // otherwise the first one that is.
            ClearActive();
            SyncGamepads();
            cached = {};
            ReadActiveInto(cached);
        }
        return cached;
    }
    const ULONGLONG now = GetTickCount64();
    if (now >= gNextPadScan) {
        SyncGamepads();
        ReadActiveInto(cached);
    }
    return cached;
}
