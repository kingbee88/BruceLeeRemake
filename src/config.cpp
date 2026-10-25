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

#include "config.h"

#include "load.h"
#include "room.h"

#include <cstddef>
#include <cwctype>
#include <string>

Config gConfig;

namespace {

std::wstring gConfigPath;
bool gConfigUtf16 = false;
bool gConfigUtf8Bom = false;
bool gConfigCrlf = true;

std::wstring Trim(std::wstring text) {
    size_t begin = 0;
    while (begin < text.size() && iswspace(text[begin])) {
        ++begin;
    }
    size_t end = text.size();
    while (end > begin && iswspace(text[end - 1])) {
        --end;
    }
    return text.substr(begin, end - begin);
}

std::wstring Lower(std::wstring text) {
    for (wchar_t& ch : text) {
        ch = static_cast<wchar_t>(towlower(ch));
    }
    return text;
}

bool ParseBool(const std::wstring& text, bool& value) {
    const std::wstring word = Lower(text);
    if (word == L"true" || word == L"yes" || word == L"on" || word == L"1") {
        value = true;
        return true;
    }
    if (word == L"false" || word == L"no" || word == L"off" || word == L"0") {
        value = false;
        return true;
    }
    return false;
}

bool ParseInt(const std::wstring& text, int& value) {
    if (text.empty()) {
        return false;
    }
    wchar_t* end = nullptr;
    const long parsed = wcstol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != L'\0') {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

void ApplySetting(std::wstring key, std::wstring value) {
    key = Lower(Trim(key));
    value = Trim(value);
    if (key == L"infinitelives") {
        ParseBool(value, gConfig.infiniteLives);
    } else if (key == L"fixbugsinoriginal") {
        ParseBool(value, gConfig.fixBugsInOriginal);
    } else if (key == L"enemiescanthityou") {
        ParseBool(value, gConfig.enemiesCantHitYou);
    } else if (key == L"startroom") {
        ParseInt(value, gConfig.startRoom);
    } else if (key == L"showtitlescreen") {
        ParseBool(value, gConfig.showTitleScreen);
    } else if (key == L"fullscreen") {
        ParseBool(value, gConfig.fullScreen);
    } else if (key == L"showcrtfilter") {
        ParseBool(value, gConfig.showCrtFilter);
    } else if (key == L"crtcurve") {
        ParseBool(value, gConfig.crtCurve);
    } else if (key == L"crtscanlines") {
        ParseBool(value, gConfig.crtScanlines);
    } else if (key == L"crtblur") {
        ParseBool(value, gConfig.crtBlur);
    } else if (key == L"crtvignette") {
        ParseBool(value, gConfig.crtVignette);
    } else if (key == L"sound") {
        ParseBool(value, gConfig.sound);
    } else if (key == L"firsttimerun") {
        ParseBool(value, gConfig.firstTimeRun);
    }
}

void ApplyToken(std::wstring token) {
    token = Trim(token);
    while (!token.empty() && (token.front() == L'/' || token.front() == L'-')) {
        token.erase(token.begin());
    }
    const size_t split = token.find_first_of(L"=:");
    if (split == std::wstring::npos) {
        return;
    }
    const std::wstring key = Lower(Trim(token.substr(0, split)));
    const std::wstring value = Trim(token.substr(split + 1));
    // Not an ini setting. A test passes it so another program in front does
    // not freeze the run, and closing the game does not store it.
    if (key == L"pauseonfocusloss") {
        ParseBool(value, gConfig.pauseOnFocusLoss);
        return;
    }
    ApplySetting(key, value);
}

// The ini is ASCII or UTF-8. A UTF-8 or UTF-16 BOM is skipped so Notepad
// can save the file without the first key disappearing.
std::wstring WidenFile(const std::string& bytes) {
    if (bytes.size() >= 2 &&
        static_cast<unsigned char>(bytes[0]) == 0xFF &&
        static_cast<unsigned char>(bytes[1]) == 0xFE) {
        std::wstring text;
        text.reserve((bytes.size() - 2) / 2);
        for (size_t i = 2; i + 1 < bytes.size(); i += 2) {
            const unsigned char lo = static_cast<unsigned char>(bytes[i]);
            const unsigned char hi = static_cast<unsigned char>(bytes[i + 1]);
            text.push_back(static_cast<wchar_t>(lo | (hi << 8)));
        }
        return text;
    }
    size_t start = 0;
    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        start = 3;
    }
    return std::wstring(bytes.begin() + static_cast<std::ptrdiff_t>(start), bytes.end());
}

void ApplyIniText(const std::wstring& text) {
    size_t lineStart = 0;
    while (lineStart <= text.size()) {
        const size_t lineEnd = text.find(L'\n', lineStart);
        std::wstring line = text.substr(lineStart, lineEnd == std::wstring::npos ? std::wstring::npos : lineEnd - lineStart);
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        line = Trim(line);
        if (!line.empty() && line.front() != L'#') {
            const size_t split = line.find(L'=');
            if (split != std::wstring::npos) {
                ApplySetting(line.substr(0, split), line.substr(split + 1));
            }
        }
        if (lineEnd == std::wstring::npos) {
            break;
        }
        lineStart = lineEnd + 1;
    }
}

bool ReadIni(const std::wstring& path, std::string& bytes) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    bytes.clear();
    char buffer[4096];
    DWORD read = 0;
    while (ReadFile(file, buffer, sizeof buffer, &read, nullptr) && read > 0) {
        bytes.append(buffer, read);
    }
    CloseHandle(file);
    return true;
}

void NoteEncoding(const std::string& bytes) {
    gConfigUtf16 = bytes.size() >= 2 &&
                   static_cast<unsigned char>(bytes[0]) == 0xFF &&
                   static_cast<unsigned char>(bytes[1]) == 0xFE;
    gConfigUtf8Bom = !gConfigUtf16 && bytes.size() >= 3 &&
                     static_cast<unsigned char>(bytes[0]) == 0xEF &&
                     static_cast<unsigned char>(bytes[1]) == 0xBB &&
                     static_cast<unsigned char>(bytes[2]) == 0xBF;
    gConfigCrlf = bytes.find("\r\n") != std::string::npos || bytes.find('\n') == std::string::npos;
}

const wchar_t* CanonicalKey(const std::wstring& key) {
    if (key == L"infinitelives") return L"InfiniteLives";
    if (key == L"enemiescanthityou") return L"EnemiesCantHitYou";
    if (key == L"fixbugsinoriginal") return L"FixBugsInOriginal";
    if (key == L"startroom") return L"StartRoom";
    if (key == L"showtitlescreen") return L"ShowTitleScreen";
    if (key == L"fullscreen") return L"FullScreen";
    if (key == L"showcrtfilter") return L"ShowCRTFilter";
    if (key == L"crtcurve") return L"CrtCurve";
    if (key == L"crtscanlines") return L"CrtScanlines";
    if (key == L"crtblur") return L"CrtBlur";
    if (key == L"crtvignette") return L"CrtVignette";
    if (key == L"sound") return L"Sound";
    if (key == L"firsttimerun") return L"FirstTimeRun";
    return nullptr;
}

std::wstring ValueOf(const std::wstring& key) {
    const auto boolean = [](bool on) { return std::wstring(on ? L"True" : L"False"); };
    if (key == L"infinitelives") return boolean(gConfig.infiniteLives);
    if (key == L"enemiescanthityou") return boolean(gConfig.enemiesCantHitYou);
    if (key == L"fixbugsinoriginal") return boolean(gConfig.fixBugsInOriginal);
    if (key == L"startroom") return std::to_wstring(ClampedStartRoom());
    if (key == L"showtitlescreen") return boolean(gConfig.showTitleScreen);
    if (key == L"fullscreen") return boolean(gConfig.fullScreen);
    if (key == L"showcrtfilter") return boolean(gConfig.showCrtFilter);
    if (key == L"crtcurve") return boolean(gConfig.crtCurve);
    if (key == L"crtscanlines") return boolean(gConfig.crtScanlines);
    if (key == L"crtblur") return boolean(gConfig.crtBlur);
    if (key == L"crtvignette") return boolean(gConfig.crtVignette);
    if (key == L"sound") return boolean(gConfig.sound);
    if (key == L"firsttimerun") return boolean(gConfig.firstTimeRun);
    return {};
}

// Replace the value on a live Key = Value line. Comments and unknown keys
// come back unchanged. `canon` is set when this line is one of the settings.
std::wstring RewriteLine(const std::wstring& line, const wchar_t*& canon) {
    canon = nullptr;
    const std::wstring trimmed = Trim(line);
    if (trimmed.empty() || trimmed.front() == L'#') {
        return line;
    }
    const size_t split = trimmed.find(L'=');
    if (split == std::wstring::npos) {
        return line;
    }
    const std::wstring key = Lower(Trim(trimmed.substr(0, split)));
    canon = CanonicalKey(key);
    if (canon == nullptr) {
        return line;
    }
    const size_t eq = line.find(L'=');
    return line.substr(0, eq + 1) + L" " + ValueOf(key);
}

std::wstring Newline() {
    return gConfigCrlf ? std::wstring(L"\r\n") : std::wstring(L"\n");
}

std::wstring RewriteIni(const std::wstring& text) {
    const wchar_t* order[] = {
        L"FirstTimeRun", L"InfiniteLives", L"EnemiesCantHitYou", L"StartRoom",
        L"ShowTitleScreen", L"FixBugsInOriginal", L"FullScreen", L"ShowCRTFilter",
        L"CrtCurve", L"CrtScanlines", L"CrtBlur", L"CrtVignette", L"Sound",
    };
    constexpr int kSettingCount = 13;
    bool seen[kSettingCount] = {};
    std::wstring out;
    size_t lineStart = 0;
    const std::wstring nl = Newline();
    while (lineStart <= text.size()) {
        const size_t lineEnd = text.find(L'\n', lineStart);
        std::wstring line = text.substr(lineStart, lineEnd == std::wstring::npos ? std::wstring::npos : lineEnd - lineStart);
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        const wchar_t* canon = nullptr;
        out += RewriteLine(line, canon);
        if (canon != nullptr) {
            for (int i = 0; i < kSettingCount; ++i) {
                if (std::wstring(order[i]) == canon) {
                    seen[i] = true;
                }
            }
        }
        if (lineEnd == std::wstring::npos) {
            break;
        }
        out += nl;
        lineStart = lineEnd + 1;
        if (lineStart == text.size()) {
            break;
        }
    }
    bool missing = false;
    for (bool flag : seen) {
        if (!flag) {
            missing = true;
        }
    }
    if (missing) {
        if (!out.empty() && out.back() != L'\n') {
            out += nl;
        }
        for (int i = 0; i < kSettingCount; ++i) {
            if (seen[i]) {
                continue;
            }
            const std::wstring key = Lower(order[i]);
            out += std::wstring(order[i]) + L" = " + ValueOf(key) + nl;
        }
    }
    return out;
}

std::string EncodeIni(const std::wstring& text) {
    std::string bytes;
    if (gConfigUtf16) {
        bytes.push_back(static_cast<char>(0xFF));
        bytes.push_back(static_cast<char>(0xFE));
        for (wchar_t ch : text) {
            bytes.push_back(static_cast<char>(ch & 0xFF));
            bytes.push_back(static_cast<char>((ch >> 8) & 0xFF));
        }
        return bytes;
    }
    if (gConfigUtf8Bom) {
        bytes.push_back(static_cast<char>(0xEF));
        bytes.push_back(static_cast<char>(0xBB));
        bytes.push_back(static_cast<char>(0xBF));
    }
    for (wchar_t ch : text) {
        bytes.push_back(ch < 256 ? static_cast<char>(ch) : '?');
    }
    return bytes;
}

bool WriteIni(const std::wstring& path, const std::string& bytes) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    const BOOL ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    CloseHandle(file);
    return ok && written == bytes.size();
}

std::wstring FullPath(const std::wstring& path) {
    wchar_t full[MAX_PATH]{};
    const DWORD n = GetFullPathNameW(path.c_str(), MAX_PATH, full, nullptr);
    if (n == 0 || n >= MAX_PATH) {
        return path;
    }
    return full;
}

void ApplyCommandLine(const wchar_t* commandLine) {
    if (commandLine == nullptr) {
        return;
    }
    std::wstring token;
    bool quoted = false;
    for (const wchar_t* cursor = commandLine;; ++cursor) {
        const wchar_t ch = *cursor;
        if (ch == L'"') {
            quoted = !quoted;
        } else if (ch == L'\0' || (!quoted && iswspace(ch))) {
            if (!token.empty()) {
                ApplyToken(token);
                token.clear();
            }
            if (ch == L'\0') {
                break;
            }
        } else {
            token.push_back(ch);
        }
    }
}

}  // namespace

void LoadConfig(const wchar_t* commandLine) {
    const std::wstring dir = ExeDirectory();
    // The file beside the exe wins even when it sets nothing. One folder up
    // is the workspace copy when the exe is in build\. Two folders up is
    // that copy when the exe is in build\Debug or build\Release. That opened
    // file is the only one SaveConfig writes.
    const std::wstring candidates[] = {
        dir + L"\\config.ini",
        dir + L"\\..\\config.ini",
        dir + L"\\..\\..\\config.ini",
    };
    std::string bytes;
    gConfigPath.clear();
    for (const std::wstring& path : candidates) {
        if (ReadIni(path, bytes)) {
            gConfigPath = FullPath(path);
            NoteEncoding(bytes);
            break;
        }
    }
    if (!bytes.empty()) {
        ApplyIniText(WidenFile(bytes));
    }
    ApplyCommandLine(commandLine);
    if (gConfig.startRoom < 0) {
        gConfig.startRoom = 0;
    }
    if (gConfig.startRoom >= kRoomCount) {
        gConfig.startRoom = kRoomCount - 1;
    }
}

int ClampedStartRoom() {
    int start = gConfig.startRoom;
    if (start < 0) {
        start = 0;
    }
    if (start >= kRoomCount) {
        start = kRoomCount - 1;
    }
    return start;
}

void SaveConfig() {
    if (gConfigPath.empty()) {
        gConfigPath = ExeDirectory() + L"\\config.ini";
        gConfigUtf16 = false;
        gConfigUtf8Bom = false;
        gConfigCrlf = true;
    }
    std::string existing;
    std::wstring text;
    if (ReadIni(gConfigPath, existing)) {
        NoteEncoding(existing);
        text = WidenFile(existing);
    } else {
        text = L"# Settings for BruceLee.exe. The Settings window writes these values when the program closes.";
        text += Newline();
    }
    const std::string bytes = EncodeIni(RewriteIni(text));
    if (!WriteIni(gConfigPath, bytes)) {
        MessageBoxW(nullptr, (L"Could not write " + gConfigPath).c_str(), L"Bruce Lee", MB_ICONERROR);
    }
}
