#pragma once

#include <string>

namespace astb3ast {

// WebKitGTK wrapper: ephemeral website data manager + forced GPU rendering.
class WebEngine {
public:
    WebEngine();
    ~WebEngine();

    WebEngine(const WebEngine&) = delete;
    WebEngine& operator=(const WebEngine&) = delete;

    // container is a GtkWidget* (GtkBox).
    bool create(void* container, const std::string& initialUrl);

    void navigate(const std::string& url);
    void reload();
    void focus();

private:
    struct Impl;
    Impl* impl_;
};

} // namespace astb3ast
