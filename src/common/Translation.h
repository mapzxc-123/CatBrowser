#pragma once

#include <string>
#include <utility>
#include <vector>

namespace catbrowser::translation {

enum class Direction {
    ToJapanese,
    ToEnglish,
};

using Entry = std::pair<std::string, std::string>;

// Adds user-supplied EN -> JA pairs from dict.txt. Called once before the
// first translation; an empty list keeps the built-in dictionary unchanged.
void setUserEntries(std::vector<Entry> entries);

// Builds a self-contained JavaScript snippet that replaces visible text nodes
// using the built-in dictionary. It never installs observers or timers, so no
// work is performed after the single on-demand execution finishes.
std::string buildTranslateScript(Direction direction);

// Restores text nodes changed by buildTranslateScript().
std::string buildRestoreScript();

} // namespace catbrowser::translation
