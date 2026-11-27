#pragma once

#include <string>

namespace astb3ast {

// WKWebView wrapper using an in-memory-only website data store.
class WebEngine {
public:
    WebEngine();
    ~WebEngine();

    WebEngine(const WebEngine&) = delete;
    WebEngine& operator=(const WebEngine&) = delete;

    // container is an NSView*.
    bool create(void* container, const std::string& initialUrl);

    void navigate(const std::string& url);
    void reload();
    void focus();

private:
    struct Impl;
    Impl* impl_;
};

} // namespace astb3ast
