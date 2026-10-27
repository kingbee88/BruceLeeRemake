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

// One player's bound controller. Player 1 is slot 0, player 2 is slot 1.
struct BoundPad {
    ComPtr<Wgi::IGamepad> wgi;
    ComPtr<Wgi::IRawGameController> raw;
    int axes = 0;
    int buttons = 0;
    int switches = 0;
    std::vector<int> labels;
    ComPtr<IDirectInputDevice8W> direct;
    bool directOn = false;
    LONG xMin = 0;
    LONG xMax = 65535;
    LONG yMin = 0;
    LONG yMax = 65535;
    bool hasY = false;
    int directButtons = 0;
    int directPovs = 0;
    int xinput = -1;
    std::wstring id;
    // Kept when the id string changes, so a DirectInput alias and the later
    // Windows.Gaming.Input id stay one controller.
    UINT16 vid = 0;
    UINT16 pid = 0;
    // Set once the saved id has been found. Until then an incomplete list
    // must not replace that id with whichever pad happened to be first.
    bool savedMatched = false;
};

BoundPad gSlot[2];
bool gSlotCached[2] = {};
PadInput gSlotInput[2] = {};
ULONGLONG gNextPadScan = 0;

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

void CloseDirectInput(BoundPad& slot) {
    if (slot.direct) {
        slot.direct->Unacquire();
        slot.direct.Reset();
    }
    slot.directOn = false;
    slot.hasY = false;
    slot.directButtons = 0;
    slot.directPovs = 0;
}

bool HasActive(const BoundPad& slot) {
    return slot.wgi || slot.raw || slot.directOn || slot.xinput >= 0;
}

void ClearActive(BoundPad& slot) {
    slot.wgi.Reset();
    slot.raw.Reset();
    slot.axes = 0;
    slot.buttons = 0;
    slot.switches = 0;
    slot.labels.clear();
    CloseDirectInput(slot);
    slot.xinput = -1;
    slot.id.clear();
}

bool ActiveStillWorks(BoundPad& slot) {
    if (slot.wgi) {
        Wgi::GamepadReading reading{};
        return SUCCEEDED(slot.wgi->GetCurrentReading(&reading));
    }
    if (slot.raw) {
        boolean buttons[64] = {};
        Wgi::GameControllerSwitchPosition switches[8] = {};
        double axes[8] = {};
        UINT64 timestamp = 0;
        const int buttonCount = slot.buttons < 64 ? slot.buttons : 64;
        const int switchCount = slot.switches < 8 ? slot.switches : 8;
        const int axisCount = slot.axes < 8 ? slot.axes : 8;
        return SUCCEEDED(slot.raw->GetCurrentReading(
            static_cast<UINT32>(buttonCount), buttonCount > 0 ? buttons : nullptr,
            static_cast<UINT32>(switchCount), switchCount > 0 ? switches : nullptr,
            static_cast<UINT32>(axisCount), axisCount > 0 ? axes : nullptr, &timestamp));
    }
    if (slot.directOn && slot.direct) {
        HRESULT hr = slot.direct->Poll();
        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
            hr = slot.direct->Acquire();
        }
        if (FAILED(hr) && hr != DI_NOEFFECT) {
            return false;
        }
        DIJOYSTATE2 state{};
        hr = slot.direct->GetDeviceState(sizeof(state), &state);
        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
            if (FAILED(slot.direct->Acquire())) {
                return false;
            }
            hr = slot.direct->GetDeviceState(sizeof(state), &state);
        }
        return SUCCEEDED(hr);
    }
    if (slot.xinput >= 0) {
        const ULONGLONG now = GetTickCount64();
        if (now < gXInputQuiet[slot.xinput]) {
            return false;
        }
        XINPUT_STATE state{};
        return XInputGetState(static_cast<DWORD>(slot.xinput), &state) == ERROR_SUCCESS;
    }
    return false;
}

bool ReadActiveInto(BoundPad& slot, PadInput& pad) {
    if (slot.wgi) {
        Wgi::GamepadReading reading{};
        if (FAILED(slot.wgi->GetCurrentReading(&reading))) {
            return false;
        }
        ApplyWgi(pad, reading);
        return true;
    }
    if (slot.raw) {
        boolean buttons[64] = {};
        Wgi::GameControllerSwitchPosition switches[8] = {};
        double axes[8] = {};
        UINT64 timestamp = 0;
        const int buttonCount = slot.buttons < 64 ? slot.buttons : 64;
        const int switchCount = slot.switches < 8 ? slot.switches : 8;
        const int axisCount = slot.axes < 8 ? slot.axes : 8;
        if (FAILED(slot.raw->GetCurrentReading(
                static_cast<UINT32>(buttonCount), buttonCount > 0 ? buttons : nullptr,
                static_cast<UINT32>(switchCount), switchCount > 0 ? switches : nullptr,
                static_cast<UINT32>(axisCount), axisCount > 0 ? axes : nullptr, &timestamp))) {
            return false;
        }
        for (int i = 0; i < buttonCount; ++i) {
            if (!buttons[i]) {
                continue;
            }
            const int label = i < static_cast<int>(slot.labels.size()) ? slot.labels[static_cast<size_t>(i)] : 0;
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
    if (slot.directOn && slot.direct) {
        HRESULT hr = slot.direct->Poll();
        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
            hr = slot.direct->Acquire();
        }
        DIJOYSTATE2 state{};
        hr = slot.direct->GetDeviceState(sizeof(state), &state);
        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
            if (FAILED(slot.direct->Acquire())) {
                return false;
            }
            hr = slot.direct->GetDeviceState(sizeof(state), &state);
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
        tilt(state.lX, slot.xMin, slot.xMax, lowX, highX);
        if (lowX) {
            pad.left = true;
        }
        if (highX) {
            pad.right = true;
        }
        // Low Y is up, matching the usual DirectInput stick.
        if (slot.hasY) {
            tilt(state.lY, slot.yMin, slot.yMax, lowY, highY);
            if (lowY) {
                pad.up = true;
            }
            if (highY) {
                pad.down = true;
            }
        }
        for (int i = 0; i < slot.directPovs; ++i) {
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
        for (int i = 0; i < slot.directButtons; ++i) {
            if ((state.rgbButtons[i] & 0x80) != 0) {
                pad.fire = true;
            }
        }
        return true;
    }
    if (slot.xinput >= 0) {
        XINPUT_STATE state{};
        if (XInputGetState(static_cast<DWORD>(slot.xinput), &state) != ERROR_SUCCESS) {
            gXInputQuiet[slot.xinput] = GetTickCount64() + 1000;
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
    // DirectInput ids for controllers already present under another id.
    std::vector<std::wstring> aliasIds;
    std::vector<int> aliasOf;
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

std::wstring DirectInputId(const GUID& guid) {
    wchar_t buf[80];
    swprintf_s(buf, L"di:{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
               guid.Data1, guid.Data2, guid.Data3,
               guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
               guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    return buf;
}

// Controllers joy.cpl lists that Windows.Gaming.Input does not. Xbox pads are
// already in the list, so their DirectInput aliases are left out. An alias
// of a pad that is already listed keeps its DirectInput id so a saved id
// from that list still selects the same pad.
void AddDirectInputOnly(EnumResult& result) {
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
        int listed = -1;
        if (vid != 0 || pid != 0) {
            for (int i = 0; i < static_cast<int>(result.items.size()); ++i) {
                const ListedPad& item = result.items[static_cast<size_t>(i)];
                if (item.vid == vid && item.pid == pid) {
                    listed = i;
                    break;
                }
            }
        }
        if (listed >= 0) {
            result.aliasIds.push_back(DirectInputId(dev.instance));
            result.aliasOf.push_back(listed);
            continue;
        }
        ListedPad item;
        item.directInput = true;
        item.instance = dev.instance;
        item.vid = vid;
        item.pid = pid;
        item.name = dev.name;
        item.id = DirectInputId(dev.instance);
        result.items.push_back(std::move(item));
    }
}

void ReadAxisRange(BoundPad& slot, DWORD offset, LONG& low, LONG& high) {
    DIPROPRANGE range{};
    range.diph.dwSize = sizeof(range);
    range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    range.diph.dwObj = offset;
    range.diph.dwHow = DIPH_BYOFFSET;
    if (!slot.direct ||
        FAILED(slot.direct->GetProperty(DIPROP_RANGE, &range.diph)) ||
        range.lMin >= range.lMax) {
        low = 0;
        high = 65535;
        return;
    }
    low = range.lMin;
    high = range.lMax;
}

bool OpenDirectInput(BoundPad& slot, const GUID& instance) {
    CloseDirectInput(slot);
    if (!EnsureDirectInput()) {
        return false;
    }
    if (FAILED(gDirectInput->CreateDevice(instance, slot.direct.GetAddressOf(), nullptr))) {
        return false;
    }
    if (FAILED(slot.direct->SetDataFormat(&c_dfDIJoystick2))) {
        slot.direct.Reset();
        return false;
    }
    slot.direct->SetCooperativeLevel(GetDesktopWindow(), DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
    DIDEVCAPS caps{};
    caps.dwSize = sizeof(caps);
    if (SUCCEEDED(slot.direct->GetCapabilities(&caps))) {
        slot.directButtons = static_cast<int>(caps.dwButtons);
        if (slot.directButtons > 128) {
            slot.directButtons = 128;
        }
        slot.directPovs = static_cast<int>(caps.dwPOVs);
        if (slot.directPovs > 4) {
            slot.directPovs = 4;
        }
        slot.hasY = caps.dwAxes >= 2;
    }
    ReadAxisRange(slot, DIJOFS_X, slot.xMin, slot.xMax);
    ReadAxisRange(slot, DIJOFS_Y, slot.yMin, slot.yMax);
    slot.direct->Acquire();
    slot.directOn = true;
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
    AddDirectInputOnly(result);
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
    AddDirectInputOnly(result);
    FinishLabels(result.items);
    return result;
}

EnumResult Enumerate() {
    if (UseWgi()) {
        return EnumWgi();
    }
    return EnumXInput();
}

void Bind(BoundPad& slot, const std::vector<ListedPad>& items, int active) {
    if (active < 0 || active >= static_cast<int>(items.size())) {
        ClearActive(slot);
        return;
    }
    const ListedPad& item = items[static_cast<size_t>(active)];
    slot.vid = item.vid;
    slot.pid = item.pid;
    if (item.id == slot.id && ActiveStillWorks(slot)) {
        return;
    }
    ClearActive(slot);
    slot.id = item.id;
    if (item.directInput) {
        OpenDirectInput(slot, item.instance);
        return;
    }
    slot.wgi = item.gamepad;
    slot.raw = item.gamepad ? nullptr : item.raw;
    slot.axes = item.axisCount;
    slot.buttons = item.buttonCount;
    slot.switches = item.switchCount;
    slot.labels = item.labels;
    slot.xinput = item.xinput;
}

GamepadList ToList(const std::vector<ListedPad>& items, int active, int active2) {
    GamepadList list;
    list.active = active;
    list.active2 = active2;
    list.items.reserve(items.size());
    for (const ListedPad& item : items) {
        GamepadChoice choice;
        choice.id = item.id;
        choice.name = item.name;
        list.items.push_back(std::move(choice));
    }
    return list;
}

bool GamepadIsNone(const std::wstring& id) {
    return id == kGamepadNone;
}

// How one saved id lines up with the pads connected right now.
struct PadResolve {
    int index = -1;
    bool matched = false;
    bool rewriteId = false;
    bool waiting = false;
    bool fallback = false;
};

PadResolve MatchPlayer(const EnumResult& enumerated, const std::wstring& saved, const BoundPad& slot) {
    PadResolve found;
    const std::vector<ListedPad>& items = enumerated.items;
    if (GamepadIsNone(saved)) {
        return found;
    }
    if (!saved.empty()) {
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            if (items[static_cast<size_t>(i)].id == saved) {
                found.index = i;
                found.matched = true;
                return found;
            }
        }
        for (size_t i = 0; i < enumerated.aliasIds.size() && i < enumerated.aliasOf.size(); ++i) {
            const int index = enumerated.aliasOf[i];
            if (enumerated.aliasIds[i] != saved || index < 0 || index >= static_cast<int>(items.size())) {
                continue;
            }
            found.index = index;
            found.matched = true;
            found.rewriteId = true;
            return found;
        }
        if (slot.savedMatched && (slot.vid != 0 || slot.pid != 0)) {
            for (int i = 0; i < static_cast<int>(items.size()); ++i) {
                const ListedPad& item = items[static_cast<size_t>(i)];
                if (item.vid == slot.vid && item.pid == slot.pid) {
                    found.index = i;
                    found.matched = true;
                    found.rewriteId = true;
                    return found;
                }
            }
        }
        if (!slot.savedMatched) {
            // The saved pad has not been seen. Leave its id alone.
            found.waiting = true;
            return found;
        }
        found.fallback = true;
        return found;
    }
    found.fallback = true;
    return found;
}

int FirstFree(const std::vector<char>& taken) {
    for (int i = 0; i < static_cast<int>(taken.size()); ++i) {
        if (!taken[static_cast<size_t>(i)]) {
            return i;
        }
    }
    return -1;
}

void ReleasePadRuntime() {
    for (BoundPad& slot : gSlot) {
        ClearActive(slot);
        slot.vid = 0;
        slot.pid = 0;
        slot.savedMatched = false;
    }
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
    gSlotCached[0] = false;
    gSlotCached[1] = false;
}

GamepadList SyncGamepads() {
    const EnumResult enumerated = Enumerate();
    if (!enumerated.ok) {
        return gShown;
    }
    const std::vector<ListedPad>& items = enumerated.items;
    std::wstring* ids[2] = {&gConfig.gamepad, &gConfig.gamepad2};
    PadResolve resolved[2];
    for (int player = 0; player < 2; ++player) {
        resolved[player] = MatchPlayer(enumerated, *ids[player], gSlot[player]);
    }
    // Both configs named one pad. Player 1 keeps it.
    if (resolved[0].matched && resolved[1].matched && resolved[0].index == resolved[1].index) {
        resolved[1].matched = false;
        resolved[1].rewriteId = false;
        resolved[1].index = -1;
        resolved[1].fallback = true;
    }
    std::vector<char> taken(items.size(), 0);
    for (int player = 0; player < 2; ++player) {
        if (resolved[player].matched && resolved[player].index >= 0) {
            taken[static_cast<size_t>(resolved[player].index)] = 1;
        }
    }
    for (int player = 0; player < 2; ++player) {
        PadResolve& found = resolved[player];
        if (!found.fallback) {
            continue;
        }
        const int pick = FirstFree(taken);
        if (pick >= 0) {
            found.index = pick;
            found.matched = true;
            found.rewriteId = true;
            taken[static_cast<size_t>(pick)] = 1;
            gSlot[player].savedMatched = true;
        } else if (!ids[player]->empty() && !GamepadIsNone(*ids[player])) {
            *ids[player] = kGamepadNone;
            gSlot[player].savedMatched = false;
            found.index = -1;
        }
    }
    for (int player = 0; player < 2; ++player) {
        PadResolve& found = resolved[player];
        if (!found.waiting) {
            continue;
        }
        const int pick = FirstFree(taken);
        if (pick >= 0) {
            found.index = pick;
            taken[static_cast<size_t>(pick)] = 1;
        }
    }
    for (int player = 0; player < 2; ++player) {
        const PadResolve& found = resolved[player];
        if (found.rewriteId && found.index >= 0) {
            *ids[player] = items[static_cast<size_t>(found.index)].id;
        }
        if (found.matched) {
            gSlot[player].savedMatched = true;
        }
        Bind(gSlot[player], items, found.index);
    }
    gShown = ToList(items, resolved[0].index, resolved[1].index);
    gNextPadScan = GetTickCount64() + 1000;
    return gShown;
}

PadInput ReadPadSlot(int player) {
    if (gSlotCached[player]) {
        return gSlotInput[player];
    }
    gSlotCached[player] = true;
    gSlotInput[player] = {};
    // Another program is in front. Leave the sticks with that program.
    if (!ForegroundIsOurs()) {
        return gSlotInput[player];
    }
    BoundPad& slot = gSlot[player];
    if (HasActive(slot)) {
        if (!ReadActiveInto(slot, gSlotInput[player])) {
            // The chosen pad is gone. Pick it again if it is still there,
            // otherwise the first one that player can take.
            ClearActive(slot);
            SyncGamepads();
            gSlotInput[player] = {};
            ReadActiveInto(gSlot[player], gSlotInput[player]);
        } else if (!slot.savedMatched) {
            // The first list can be a DirectInput alias of the saved pad.
            // Look again until that saved id shows up, and keep reading this
            // pad in the meantime.
            const ULONGLONG now = GetTickCount64();
            if (now >= gNextPadScan) {
                SyncGamepads();
            }
        }
        return gSlotInput[player];
    }
    const ULONGLONG now = GetTickCount64();
    if (now >= gNextPadScan) {
        SyncGamepads();
        ReadActiveInto(gSlot[player], gSlotInput[player]);
    }
    return gSlotInput[player];
}

PadInput ReadPad() {
    return ReadPadSlot(0);
}

PadInput ReadPad2() {
    return ReadPadSlot(1);
}
