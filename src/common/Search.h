#pragma once

#include <string>

namespace catbrowser::search {

enum class Engine {
    DuckDuckGo,
    Google,
    Bing,
};

// Lower-case names used in CatBrowser.ini.
Engine engineFromName(const std::string& name);
const char* engineName(Engine engine);   // "ddg" / "google" / "bing"
const wchar_t* engineLabel(Engine engine); // short button label

// Percent-encodes the query and returns the engine's search URL.
std::string buildUrl(Engine engine, const std::string& query);

} // namespace catbrowser::search
