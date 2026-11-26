#include "UrlUtils.h"

#include <algorithm>
#include <cctype>

namespace catbrowser::url {
namespace {

bool startsWithIgnoreCase(std::string_view value, std::string_view prefix) {
    if (value.size() < prefix.size()) {
        return false;
    }
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(value[i])) != prefix[i]) {
            return false;
        }
    }
    return true;
}

bool hasForbiddenCharacter(std::string_view value) {
    return std::any_of(value.begin(), value.end(), [](unsigned char c) {
        // Whitespace usually means a search query; the other characters are
        // not valid in a URL entered here and could smuggle another scheme.
        return std::isspace(c) != 0 || c == '"' || c == '<' || c == '>' ||
               c == '\\' || c == '`' || c == '\'';
    });
}

} // namespace

std::string normalize(std::string_view input) {
    size_t begin = 0;
    size_t end = input.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(input[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(input[end - 1]))) {
        --end;
    }

    const std::string_view value = input.substr(begin, end - begin);
    if (value.empty() || hasForbiddenCharacter(value)) {
        return {};
    }

    if (startsWithIgnoreCase(value, "https://") ||
        startsWithIgnoreCase(value, "http://")) {
        return std::string(value);
    }

    // Reject other explicit schemes such as javascript:, file:, data:, etc.
    // A colon before the first slash is interpreted as a scheme separator.
    const size_t colon = value.find(':');
    const size_t slash = value.find('/');
    if (colon != std::string_view::npos &&
        (slash == std::string_view::npos || colon < slash)) {
        return {};
    }

    return "https://" + std::string(value);
}

bool isAllowed(std::string_view value) {
    return startsWithIgnoreCase(value, "https://") ||
           startsWithIgnoreCase(value, "http://");
}

} // namespace catbrowser::url
