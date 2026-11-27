#include "Search.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string_view>

namespace astb3ast::search {
namespace {

bool isUnreserved(unsigned char c) {
    return std::isalnum(c) != 0 || c == '-' || c == '_' || c == '.' || c == '~';
}

std::string percentEncode(std::string_view value) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size() * 2);
    for (unsigned char c : value) {
        if (isUnreserved(c)) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[(c >> 4) & 0x0f]);
            out.push_back(kHex[c & 0x0f]);
        }
    }
    return out;
}

std::string lower(std::string_view value) {
    std::string out(value);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

} // namespace

Engine engineFromName(const std::string& name) {
    const std::string value = lower(name);
    if (value == "google") {
        return Engine::Google;
    }
    if (value == "bing") {
        return Engine::Bing;
    }
    return Engine::DuckDuckGo;
}

const char* engineName(Engine engine) {
    switch (engine) {
    case Engine::Google: return "google";
    case Engine::Bing: return "bing";
    case Engine::DuckDuckGo:
    default: return "ddg";
    }
}

const wchar_t* engineLabel(Engine engine) {
    switch (engine) {
    case Engine::Google: return L"Google";
    case Engine::Bing: return L"Bing";
    case Engine::DuckDuckGo:
    default: return L"DDG";
    }
}

std::string buildUrl(Engine engine, const std::string& query) {
    const std::string encoded = percentEncode(query);
    switch (engine) {
    case Engine::Google:
        return "https://www.google.com/search?q=" + encoded;
    case Engine::Bing:
        return "https://www.bing.com/search?q=" + encoded;
    case Engine::DuckDuckGo:
    default:
        return "https://duckduckgo.com/?q=" + encoded;
    }
}

} // namespace astb3ast::search
