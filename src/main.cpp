#include "App.h"
#include "common/Text.h"

#if defined(_WIN32)
#include <shellapi.h>
#include <vector>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
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
