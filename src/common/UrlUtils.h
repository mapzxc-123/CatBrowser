#pragma once

#include <string>
#include <string_view>

namespace catbrowser::url {

// Adds only an https:// default scheme. Never consults a search engine.
// Returns an empty string for anything that is not a plain http(s) URL
// (whitespace, quotes, explicit non-http schemes, empty input).
std::string normalize(std::string_view input);

bool isAllowed(std::string_view url);

} // namespace catbrowser::url
