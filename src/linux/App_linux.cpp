#include <sys/resource.h>

#include "../App.h"
#include "../common/Log.h"
#include "PlatformWindow.h"
#include "WebEngine.h"

namespace catbrowser {

int runPlatform(const AppConfig& config) {
    // Needs CAP_SYS_NICE or a raised RLIMIT_NICE; otherwise we simply keep
    // the default priority instead of failing to start.
    if (setpriority(PRIO_PROCESS, 0, -10) != 0 &&
        setpriority(PRIO_PROCESS, 0, -5) != 0) {
        log::warn("Could not raise nice value (requires CAP_SYS_NICE / RLIMIT_NICE)");
    }

    gtk_init(nullptr, nullptr);

    PlatformWindow window(config.initialUrl);
    WebEngine engine;

    window.setOnNavigate([&](const std::string& url) {
        engine.navigate(url);
        engine.focus(); // Enter moves focus into WebKitWebView immediately.
    });
    window.setOnReload([&] { engine.reload(); });

    window.create();
    engine.create(window.webContainer(), config.initialUrl);
    window.show();

    gtk_main();
    return 0;
}

} // namespace catbrowser
