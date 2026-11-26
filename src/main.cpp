#include "App.h"
#include "common/Text.h"

#if defined(_WIN32)
#include <shellapi.h>
#include <vector>

namespace {

// Per-monitor DPI awareness v2 must be requested before any HWND is created.
// Loaded dynamically so the binary still runs on older Windows versions.
void enableDpiAwareness() {
    HMODULE user32 = LoadLibraryW(L"user32.dll");
    if (user32 == nullptr) {
        return;
    }
    using SetDpiAwarenessContextFn = BOOL(WINAPI*)(void*);
    auto setContext = reinterpret_cast<SetDpiAwarenessContextFn>(
        GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (setContext != nullptr) {
        // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == ((context)-4)
        setContext(reinterpret_cast<void*>(static_cast<INT_PTR>(-4)));
    }
    FreeLibrary(user32);
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    enableDpiAwareness();

    catbrowser::AppConfig config;
    config.nativeInstance = hInstance;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv != nullptr) {
        if (argc > 1) {
            config.initialUrl = catbrowser::text::wideToUtf8(argv[1]);
        }
        LocalFree(argv);
    }

    return catbrowser::runPlatform(config);
}
#else

int main(int argc, char** argv) {
    catbrowser::AppConfig config;
    if (argc > 1) {
        config.initialUrl = argv[1];
    }
    return catbrowser::runPlatform(config);
}
#endif
