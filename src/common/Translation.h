#pragma once

#include <string>

namespace catbrowser::translation {

enum class Direction {
    ToJapanese,
    ToEnglish,
};

// Builds a self-contained JavaScript snippet that replaces visible text nodes
// using the built-in dictionary. It never installs observers or timers, so no
// work is performed after the single on-demand execution finishes.
std::string buildTranslateScript(Direction direction);

// Restores text nodes changed by buildTranslateScript().
std::string buildRestoreScript();

} // namespace catbrowser::translation
