#include "PlatformWindow.h"

#include "../common/UrlUtils.h"

#include <gtk/gtk.h>

#include <utility>

namespace astb3ast {

struct PlatformWindow::Impl {
    GtkWidget* window = nullptr;
    GtkWidget* entry = nullptr;
    GtkWidget* goButton = nullptr;
    GtkWidget* webContainer = nullptr;
    std::string initialUrl;
    std::function<void(const std::string&)> onNavigate;
    std::function<void()> onReload;
    guint keySnooper = 0;
    bool fullscreen = false;
};

namespace {

Impl* g_impl = nullptr;

void submitUrl() {
    if (g_impl == nullptr || g_impl->entry == nullptr) {
        return;
    }
    const gchar* text = gtk_entry_get_text(GTK_ENTRY(g_impl->entry));
    const std::string url = url::normalize(text != nullptr ? text : "");
    if (url.empty()) {
        gdk_beep();
        return;
    }
    if (g_impl->onNavigate) {
        g_impl->onNavigate(url);
    }
}

void onEntryActivate(GtkEntry*, gpointer) {
    submitUrl();
}

void onGoClicked(GtkButton*, gpointer) {
    submitUrl();
}

// GTK3 key snooper: runs before WebKit gets a chance to consume F5/F11.
gboolean onKeySnoop(GtkWidget*, GdkEventKey* event, gpointer) {
    if (g_impl == nullptr || event == nullptr) {
        return FALSE;
    }
    if (event->keyval == GDK_KEY_F5) {
        if (g_impl->onReload) {
            g_impl->onReload();
        }
        return TRUE;
    }
    if (event->keyval == GDK_KEY_F11) {
        if (g_impl->window != nullptr) {
            auto* self = g_impl;
            if (!self->fullscreen) {
                gtk_window_set_decorated(GTK_WINDOW(self->window), FALSE);
                gtk_window_fullscreen(GTK_WINDOW(self->window));
                self->fullscreen = true;
            } else {
                gtk_window_unfullscreen(GTK_WINDOW(self->window));
                gtk_window_set_decorated(GTK_WINDOW(self->window), TRUE);
                self->fullscreen = false;
            }
        }
        return TRUE;
    }
    return FALSE;
}

} // namespace

PlatformWindow::PlatformWindow(const std::string& initialUrl)
    : impl_(new Impl()) {
    impl_->initialUrl = initialUrl;
}

PlatformWindow::~PlatformWindow() {
    if (impl_ != nullptr) {
        if (impl_->keySnooper != 0) {
            gtk_key_snooper_remove(impl_->keySnooper);
        }
        if (g_impl == impl_) {
            g_impl = nullptr;
        }
        delete impl_;
        impl_ = nullptr;
    }
}

bool PlatformWindow::create() {
    impl_->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    if (impl_->window == nullptr) {
        return false;
    }
    gtk_window_set_title(GTK_WINDOW(impl_->window), "AstB3ast");
    gtk_window_set_default_size(GTK_WINDOW(impl_->window), 1280, 720);
    g_signal_connect(impl_->window, "destroy",
                     G_CALLBACK(gtk_main_quit), nullptr);

    GtkWidget* bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    impl_->entry = gtk_entry_new();
    gtk_widget_set_size_request(impl_->entry, -1, 30);
    gtk_entry_set_text(GTK_ENTRY(impl_->entry), impl_->initialUrl.c_str());
    gtk_box_pack_start(GTK_BOX(bar), impl_->entry, TRUE, TRUE, 0);

    impl_->goButton = gtk_button_new_with_label("GO");
    gtk_widget_set_size_request(impl_->goButton, 64, 30);
    gtk_box_pack_start(GTK_BOX(bar), impl_->goButton, FALSE, FALSE, 0);

    GtkWidget* rootBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(rootBox), bar, FALSE, FALSE, 0);

    impl_->webContainer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(rootBox), impl_->webContainer, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(impl_->window), rootBox);

    g_signal_connect(impl_->entry, "activate",
                     G_CALLBACK(onEntryActivate), nullptr);
    g_signal_connect(impl_->goButton, "clicked",
                     G_CALLBACK(onGoClicked), nullptr);

    g_impl = impl_;
    impl_->keySnooper = gtk_key_snooper_install(onKeySnoop, nullptr);
    return true;
}

void PlatformWindow::show() {
    gtk_widget_show_all(impl_->window);
    gtk_window_present(GTK_WINDOW(impl_->window));
}

void PlatformWindow::toggleFullscreen() {
    if (impl_->window == nullptr) {
        return;
    }
    if (!impl_->fullscreen) {
        gtk_window_set_decorated(GTK_WINDOW(impl_->window), FALSE);
        gtk_window_fullscreen(GTK_WINDOW(impl_->window));
        impl_->fullscreen = true;
    } else {
        gtk_window_unfullscreen(GTK_WINDOW(impl_->window));
        gtk_window_set_decorated(GTK_WINDOW(impl_->window), TRUE);
        impl_->fullscreen = false;
    }
}

void* PlatformWindow::webContainer() const {
    return static_cast<void*>(impl_->webContainer);
}

void PlatformWindow::setOnNavigate(
    std::function<void(const std::string&)> callback) {
    impl_->onNavigate = std::move(callback);
}

void PlatformWindow::setOnReload(std::function<void()> callback) {
    impl_->onReload = std::move(callback);
}

} // namespace astb3ast
