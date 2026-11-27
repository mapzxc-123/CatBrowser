#pragma once

#include <functional>
#include <string>

namespace astb3ast {

// GTK3 window: 30px URL bar + GO button + a vertical slot for WebKitWebView.
class PlatformWindow {
public:
    explicit PlatformWindow(const std::string& initialUrl);
    ~PlatformWindow();

    PlatformWindow(const PlatformWindow&) = delete;
    PlatformWindow& operator=(const PlatformWindow&) = delete;

    bool create();
    void show();
    void toggleFullscreen();

    // GtkWidget* container for the WebKitWebView.
    void* webContainer() const;

    void setOnNavigate(std::function<void(const std::string&)> callback);
    void setOnReload(std::function<void()> callback);

private:
    struct Impl;
    Impl* impl_;
};

} // namespace astb3ast
