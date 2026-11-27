#pragma once

#include "../common/Search.h"

#include <string>
#include <utility>
#include <vector>

namespace catbrowser::localdata {

struct Settings {
    search::Engine engine = search::Engine::DuckDuckGo;
    bool darkTheme = true;
    std::wstring downloadDir; // empty means WebView2's default Downloads folder
};

// All files are plain UTF-8 and live next to CatBrowser.exe.
Settings loadSettings();
void saveSettings(const Settings& settings);

std::vector<std::wstring> loadHistory();
// Adds the newest entry, removes duplicates, trims to the keep limit and
// returns the updated list (newest first) for edit-control completion.
std::vector<std::wstring> addHistory(const std::string& entry);

// Each line is "english<TAB>japanese" (or "english=japanese"). Lines starting
// with '#' are comments. Returned pairs are UTF-8 EN -> JA entries.
std::vector<std::pair<std::string, std::string>> loadUserDictionary();

} // namespace catbrowser::localdata
