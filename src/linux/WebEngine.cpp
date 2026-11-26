#include "WebEngine.h"

#include "../common/UrlUtils.h"

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

namespace catbrowser {

struct WebEngine::Impl {
    WebKitWebContext* context = nullptr;
    WebKitSettings* settings = nullptr;
    WebKitWebView* webView = nullptr;
};

} // namespace catbrowser

namespace catbrowser {

WebEngine::WebEngine() : impl_(new Impl()) {}

WebEngine::~WebEngine() {
    if (impl_ != nullptr) {
        // Widgets are owned by the GTK container; GObjects need unref.
        if (impl_->settings != nullptr) {
            g_object_unref(impl_->settings);
        }
        if (impl_->context != nullptr) {
            g_object_unref(impl_->context);
        }
        delete impl_;
        impl_ = nullptr;
    }
}

bool WebEngine::create(void* container, const std::string& initialUrl) {
    GtkWidget* parent = static_cast<GtkWidget*>(container);
    if (parent == nullptr) {
        return false;
    }

    // Ephemeral context => in-memory WebKitWebsiteDataManager: no disk
    // cache, no cookies on disk, no history growth.
    impl_->context = webkit_web_context_new_ephemeral();
    webkit_web_context_set_cache_model(impl_->context,
                                       WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);

    impl_->settings = webkit_settings_new();
    g_object_set(impl_->settings,
                 "enable-webgl", TRUE,
                 "enable-accelerated-2d-canvas", TRUE,
                 "hardware-acceleration-policy",
                 WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS,
                 "enable-javascript", TRUE,
                 "enable-javascript-can-open-windows-automatically", FALSE,
                 nullptr);

    GtkWidget* view = webkit_web_view_new_with_context(impl_->context);
    impl_->webView = WEBKIT_WEB_VIEW(view);
    webkit_web_view_set_settings(impl_->webView, impl_->settings);

    gtk_box_pack_start(GTK_BOX(parent), view, TRUE, TRUE, 0);

    const std::string url = url::normalize(initialUrl);
    if (!url.empty()) {
        navigate(url);
    }
    return true;
}

void WebEngine::navigate(const std::string& url) {
    if (impl_->webView == nullptr) {
        return;
    }
    const std::string normalized = url::normalize(url);
    if (!normalized.empty()) {
        webkit_web_view_load_uri(impl_->webView, normalized.c_str());
    }
}

void WebEngine::reload() {
    if (impl_->webView != nullptr) {
        webkit_web_view_reload_bypass_cache(impl_->webView);
    }
}

void WebEngine::focus() {
    if (impl_->webView != nullptr) {
        gtk_widget_grab_focus(GTK_WIDGET(impl_->webView));
    }
}

} // namespace catbrowser
