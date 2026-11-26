#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <functional>
#include <string>
#include <utility>

namespace catbrowser {

// Pure Win32 window: 30px URL bar + GO button + a child HWND used as the
// WebView2 host. No toolbars, tabs, bookmarks or other browser UI.
class PlatformWindow {
public:
    explicit PlatformWindow(HINSTANCE instance);
    ~PlatformWindow();

    PlatformWindow(const PlatformWindow&) = delete;
    PlatformWindow& operator=(const PlatformWindow&) = delete;

    bool create();
    int messageLoop();

    HWND webHost() const { return webHost_; }

    void toggleFullscreen();

    void setOnNavigate(std::function<void(const std::string&)> callback) {
        onNavigate_ = std::move(callback);
    }
    void setOnReload(std::function<void()> callback) {
        onReload_ = std::move(callback);
    }
    void setOnResize(std::function<void(int, int)> callback) {
        onResize_ = std::move(callback);
    }
    void setOnTranslate(std::function<void(bool toJapanese)> callback) {
        onTranslate_ = std::move(callback);
    }
    void setOnVisibility(std::function<void(bool visible)> callback) {
        onVisibility_ = std::move(callback);
    }

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK editProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void submitUrl();
    void layout();

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND urlEdit_ = nullptr;
    HWND goButton_ = nullptr;
    HWND translateButton_ = nullptr;
    HWND directionButton_ = nullptr;
    HWND webHost_ = nullptr;
    WNDPROC originalEditProc_ = nullptr;
    HFONT uiFont_ = nullptr;

    RECT savedRect_{};
    LONG_PTR savedStyle_ = 0;
    bool fullscreen_ = false;
    bool translateToJapanese_ = true;

    std::function<void(const std::string&)> onNavigate_;
    std::function<void()> onReload_;
    std::function<void(int, int)> onResize_;
    std::function<void(bool)> onTranslate_;
    std::function<void(bool)> onVisibility_;
};

} // namespace catbrowser
