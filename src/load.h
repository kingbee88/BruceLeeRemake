#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Looks beside the exe, then in data\ next to it and one directory up.
std::vector<uint8_t> LoadFile(const wchar_t* name, size_t bytes);
