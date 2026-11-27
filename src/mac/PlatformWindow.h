#pragma once

#include <functional>
#include <string>

namespace astb3ast {

// Cocoa window: 30pt URL bar + GO button + a layer-backed WKWebView container.
// The implementation lives in PlatformWindow.mm (Objective-C++).
class PlatformWindow {
public:
    explicit PlatformWindow(const std::string& initialUrl);
    ~PlatformWindow();

    PlatformWindow(const PlatformWindow&) = delete;
    PlatformWindow& operator=(const PlatformWindow&) = delete;

    bool create();
    void show();
    void toggleFullscreen();

    // NSView* to be used as the WKWebView superview.
    void* webContainer() const;

    void setOnNavigate(std::function<void(const std::string&)> callback);
    void setOnReload(std::function<void()> callback);

private:
    struct Impl;
    Impl* impl_;
};

} // namespace astb3ast
