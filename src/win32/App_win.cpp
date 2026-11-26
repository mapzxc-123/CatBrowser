#include "../App.h"
#include "../common/Log.h"
#include "PlatformWindow.h"
#include "WebEngine.h"

namespace catbrowser {

int runPlatform(const AppConfig& config) {
    // Slight boost without starving the rest of the system. HIGH_PRIORITY_CLASS
    // made the whole machine feel sluggish for no measurable gain here.
    SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

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
