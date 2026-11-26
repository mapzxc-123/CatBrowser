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

namespace catbrowser {

// Thin wrapper around the Win32 WebView2 API. The loader is linked statically
// so the final artifact is a single .exe (apart from the WebView2 Runtime).
class WebEngine {
public:
    bool create(HWND host, const std::string& initialUrl);

    void navigate(const std::string& url);
    void reload();
    void focus();
    void resize(int width, int height);
    void setVisible(bool visible);

    // One-shot, offline page translation. No observers or timers are installed.
    void translate(translation::Direction direction);

    void setOnReload(std::function<void()> callback) {
        onReload_ = std::move(callback);
    }
    void setOnFullscreenToggle(std::function<void()> callback) {
        onFullscreenToggle_ = std::move(callback);
    }

private:
    void applySettings();
    void registerAccelerators();

    Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment_;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webView_;

    HWND host_ = nullptr;
    std::string pendingUrl_;
    EventRegistrationToken acceleratorToken_{};

    std::function<void()> onReload_;
    std::function<void()> onFullscreenToggle_;
};

} // namespace catbrowser
