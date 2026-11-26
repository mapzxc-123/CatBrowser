#pragma once

#include <string>

namespace catbrowser {

// Platform-independent launch configuration.
struct AppConfig {
    // UTF-8. Empty means "about:blank".
    std::string initialUrl;

    // Windows only: HINSTANCE of the process. Ignored on macOS/Linux.
    void* nativeInstance = nullptr;
};

// Implemented exactly once per platform (App_win.cpp / App_mac.mm / App_linux.cpp).
int runPlatform(const AppConfig& config);

} // namespace catbrowser
