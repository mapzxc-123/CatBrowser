#include "../App.h"
#include "../common/Log.h"
#include "../common/Translation.h"
#include "LocalData.h"
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

    localdata::Settings settings = localdata::loadSettings();
    translation::setUserEntries(localdata::loadUserDictionary());

    window.setEngine(settings.engine);
    window.setTheme(settings.darkTheme);
    window.setHistory(localdata::loadHistory());
    engine.setTheme(settings.darkTheme);
    engine.setDownloadDirectory(settings.downloadDir);

    window.setOnNavigate([&](const std::string& url) {
        engine.navigate(url);
        engine.focus(); // Enter moves focus straight into the WebView.
    });
    window.setOnHistoryCommitted([&](const std::string& input) {
        window.setHistory(localdata::addHistory(input));
    });
    window.setOnReload([&] { engine.reload(); });
    window.setOnBack([&] { engine.goBack(); });
    window.setOnForward([&] { engine.goForward(); });
    window.setOnResize([&](int width, int height) {
        engine.resize(width, height);
    });
    window.setOnTranslate([&](bool toJapanese) {
        // One-shot, offline dictionary pass. No timers, no observers, no network.
        engine.translate(toJapanese ? translation::Direction::ToJapanese
                                     : translation::Direction::ToEnglish);
    });
    window.setOnVisibility([&](bool visible) {
        engine.setVisible(visible);
    });
    window.setOnEngineChanged([&](search::Engine selected) {
        settings.engine = selected;
        localdata::saveSettings(settings);
    });
    window.setOnThemeChanged([&](bool dark) {
        settings.darkTheme = dark;
        engine.setTheme(dark);
        localdata::saveSettings(settings);
    });

    engine.setOnReload([&] { engine.reload(); });
    engine.setOnFullscreenToggle([&] { window.toggleFullscreen(); });
    engine.setOnNavigationStateChanged([&](bool canBack, bool canForward) {
        window.setNavigationState(canBack, canForward);
    });

    engine.create(window.webHost(), config.initialUrl);

    const int result = window.messageLoop();

    CoUninitialize();
    return result;
}

} // namespace catbrowser
