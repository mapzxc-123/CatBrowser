#pragma once

#include <string>
#include <string_view>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace astb3ast::text {

inline std::string wideToUtf8(std::wstring_view input) {
    if (input.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(
        CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
        nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}

inline std::wstring utf8ToWide(std::string_view input) {
    if (input.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(
        CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
        nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
        result.data(), size);
    return result;
}

} // namespace astb3ast::text
#endif
