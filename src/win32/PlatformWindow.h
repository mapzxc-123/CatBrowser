#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "../common/Search.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace catbrowser {

// Pure Win32 window: a compact 32px control bar + a child HWND used as the
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

    // 1.4 UI state, restored from CatBrowser.ini at startup.
    void setEngine(search::Engine engine);
    void setTheme(bool dark);
    void setHistory(std::vector<std::wstring> history) {
        history_ = std::move(history);
        historyIndex_ = -1;
        historyDraft_.clear();
    }
    void setNavigationState(bool canBack, bool canForward);

    void setOnNavigate(std::function<void(const std::string&)> callback) {
        onNavigate_ = std::move(callback);
    }
    // Receives the trimmed raw input (URL or search query), for history.txt.
    void setOnHistoryCommitted(std::function<void(const std::string&)> callback) {
        onHistoryCommitted_ = std::move(callback);
    }
    void setOnReload(std::function<void()> callback) {
        onReload_ = std::move(callback);
    }
    void setOnBack(std::function<void()> callback) {
        onBack_ = std::move(callback);
    }
    void setOnForward(std::function<void()> callback) {
        onForward_ = std::move(callback);
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
    void setOnEngineChanged(std::function<void(search::Engine)> callback) {
        onEngineChanged_ = std::move(callback);
    }
    void setOnThemeChanged(std::function<void(bool dark)> callback) {
        onThemeChanged_ = std::move(callback);
    }

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK editProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void submitUrl();
    void layout();
    void refreshButtonText();
    void browseHistory(bool older);
    HBRUSH controlBrush() const { return darkTheme_ ? darkBrush_ : lightBrush_; }

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND urlEdit_ = nullptr;
    HWND goButton_ = nullptr;
    HWND translateButton_ = nullptr;
    HWND directionButton_ = nullptr;
    HWND backButton_ = nullptr;
    HWND forwardButton_ = nullptr;
    HWND reloadButton_ = nullptr;
    HWND fullscreenButton_ = nullptr;
    HWND modeButton_ = nullptr;
    HWND engineButton_ = nullptr;
    HWND themeButton_ = nullptr;
    HWND webHost_ = nullptr;
    WNDPROC originalEditProc_ = nullptr;
    HFONT uiFont_ = nullptr;
    HBRUSH darkBrush_ = nullptr;
    HBRUSH lightBrush_ = nullptr;

    RECT savedRect_{};
    LONG_PTR savedStyle_ = 0;
    bool fullscreen_ = false;
    bool translateToJapanese_ = true;
    bool searchMode_ = false;
    bool darkTheme_ = true;
    search::Engine engine_ = search::Engine::DuckDuckGo;

    std::vector<std::wstring> history_;
    int historyIndex_ = -1;
    std::wstring historyDraft_;

    std::function<void(const std::string&)> onNavigate_;
    std::function<void(const std::string&)> onHistoryCommitted_;
    std::function<void()> onReload_;
    std::function<void()> onBack_;
    std::function<void()> onForward_;
    std::function<void(int, int)> onResize_;
    std::function<void(bool)> onTranslate_;
    std::function<void(bool)> onVisibility_;
    std::function<void(search::Engine)> onEngineChanged_;
    std::function<void(bool)> onThemeChanged_;
};

} // namespace catbrowser
