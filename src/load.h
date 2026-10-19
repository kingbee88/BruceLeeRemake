#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Directory that contains the running exe, with no trailing slash.
std::wstring ExeDirectory();

// Read exactly `bytes` from name. Searches beside the exe, then ..\data\,
// then ..\..\data\ (exe in build\Debug or build\Release), then data\.
// Returns an empty vector when the file is missing or short.
std::vector<uint8_t> LoadFile(const wchar_t* name, size_t bytes);
