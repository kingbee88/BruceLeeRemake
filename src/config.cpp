#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "config.h"

#include "load.h"

#include <cstddef>
#include <cwctype>
#include <string>

Config gConfig;

namespace {

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
    ApplySetting(token.substr(0, split), token.substr(split + 1));
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
    std::string bytes;
    // The file beside the exe wins even when it sets nothing. One folder up
    // is the workspace copy when the exe is in build\. Two folders up is
    // that copy when the exe is in build\Debug or build\Release.
    if (!ReadIni(dir + L"\\config.ini", bytes)) {
        if (!ReadIni(dir + L"\\..\\config.ini", bytes)) {
            ReadIni(dir + L"\\..\\..\\config.ini", bytes);
        }
    }
    if (!bytes.empty()) {
        ApplyIniText(WidenFile(bytes));
    }
    ApplyCommandLine(commandLine);
}
