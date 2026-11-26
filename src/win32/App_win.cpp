#include "../App.h"
#include "../common/Log.h"
#include "PlatformWindow.h"
#include "WebEngine.h"

namespace catbrowser {

int runPlatform(const AppConfig& config) {
    // Force the discrete GPU / high performance power scheme.
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    PlatformWindow window(static_cast<HINSTANCE>(config.nativeInstance));
    WebEngine engine;

    if (!window.create()) {
        log::warn("Failed to create the Win32 window");
        CoUninitialize();
        return 1;
    }

    window.setOnNavigate([&](const std::string& url) {
        engine.navigate(url);
        engine.focus(); // Enter moves focus straight into the WebView.
    });
    window.setOnReload([&] { engine.reload(); });
    window.setOnResize([&](int width, int height) {
        engine.resize(width, height);
    });

    engine.setOnReload([&] { engine.reload(); });
    engine.setOnFullscreenToggle([&] { window.toggleFullscreen(); });

    engine.create(window.webHost(), config.initialUrl);

    const int result = window.messageLoop();

    CoUninitialize();
    return result;
}

} // namespace catbrowser
