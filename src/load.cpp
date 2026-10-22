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

#include "load.h"

#include <string>

std::wstring ExeDirectory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full(path);
    const auto slash = full.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : full.substr(0, slash);
}

std::vector<uint8_t> LoadFile(const wchar_t* name, size_t bytes) {
    const std::wstring dir = ExeDirectory();
    const std::wstring rels[] = {L"\\", L"\\..\\data\\", L"\\..\\..\\data\\", L"\\data\\"};
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

std::wstring FindDataFile(const wchar_t* name) {
    const std::wstring dir = ExeDirectory();
    const std::wstring rels[] = {L"\\", L"\\..\\data\\", L"\\..\\..\\data\\", L"\\data\\"};
    for (const std::wstring& rel : rels) {
        const std::wstring path = dir + rel + name;
        const DWORD attr = GetFileAttributesW(path.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            continue;
        }
        wchar_t full[MAX_PATH]{};
        const DWORD n = GetFullPathNameW(path.c_str(), MAX_PATH, full, nullptr);
        if (n == 0 || n >= MAX_PATH) {
            return path;
        }
        return full;
    }
    return {};
}
