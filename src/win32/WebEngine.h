#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "../common/Translation.h"

#include <functional>
#include <string>
#include <utility>

#include <wrl.h>
#include <WebView2.h>

namespace astb3ast {

// Thin wrapper around the Win32 WebView2 API. The loader is linked statically
// so the final artifact is a single .exe (apart from the WebView2 Runtime).
class WebEngine {
public:
    bool create(HWND host, const std::string& initialUrl);

    void navigate(const std::string& url);
    void reload();
    void goBack();
    void goForward();
    void focus();
    void resize(int width, int height);
    void setVisible(bool visible);

    // 1.4 settings: light/dark web content and a fixed download directory.
    void setTheme(bool dark);
    void setDownloadDirectory(std::wstring directory);

    // One-shot, offline page translation. No observers or timers are installed.
    void translate(translation::Direction direction);

    void setOnReload(std::function<void()> callback) {
        onReload_ = std::move(callback);
    }
    void setOnFullscreenToggle(std::function<void()> callback) {
        onFullscreenToggle_ = std::move(callback);
    }
    void setOnOpenSearch(std::function<void()> callback) {
        onOpenSearch_ = std::move(callback);
    }
    void setOnNavigationStateChanged(std::function<void(bool canBack, bool canForward)> callback) {
        onNavigationStateChanged_ = std::move(callback);
    }

private:
    void applySettings();
    void registerAccelerators();
    void registerNavigationObserver();
    void registerDownloadHandler();
    void applyTheme();

    Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment_;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webView_;

    HWND host_ = nullptr;
    std::string pendingUrl_;
    std::wstring downloadDir_;
    bool darkTheme_ = true;
    bool downloadHandlerRegistered_ = false;
    EventRegistrationToken acceleratorToken_{};
    EventRegistrationToken navigationToken_{};
    EventRegistrationToken downloadToken_{};

    std::function<void()> onReload_;
    std::function<void()> onFullscreenToggle_;
    std::function<void()> onOpenSearch_;
    std::function<void(bool, bool)> onNavigationStateChanged_;
};

} // namespace astb3ast
