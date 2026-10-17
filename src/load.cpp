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
