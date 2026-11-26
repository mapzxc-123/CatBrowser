#import <AppKit/AppKit.h>
#import <WebKit/WebKit.h>

#include "WebEngine.h"
#include "../common/UrlUtils.h"

namespace catbrowser {

struct WebEngine::Impl {
    __strong WKWebView* webView = nil;
};

} // namespace catbrowser

namespace catbrowser {

WebEngine::WebEngine() : impl_(new Impl()) {}

WebEngine::~WebEngine() {
    if (impl_ != nullptr) {
        impl_->webView = nil;
        delete impl_;
        impl_ = nullptr;
    }
}

bool WebEngine::create(void* container, const std::string& initialUrl) {
    @autoreleasepool {
        NSView* parent = static_cast<NSView*>(container);
        if (parent == nil) {
            return false;
        }

        WKWebViewConfiguration* configuration = [[WKWebViewConfiguration alloc] init];
        // No disk cache, cookies, history or local storage on disk.
        configuration.websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
        configuration.suppressesIncrementalRendering = NO;

        // WebKit exposes these renderer/compositor switches only as private
        // (underscore-prefixed) KVC keys. Fail softly if Apple renames them.
        WKPreferences* preferences = configuration.preferences;
        auto setPrivateFlag = ^(NSString* key, BOOL value) {
            @try {
                [preferences setValue:@(value) forKey:key];
            } @catch (NSException* exception) {
                (void)exception;
            }
        };
        setPrivateFlag(@"_acceleratedDrawingEnabled", YES);
        setPrivateFlag(@"_webGLEnabled", YES);
        setPrivateFlag(@"_canvasUsesAcceleratedDrawing", YES);
        setPrivateFlag(@"_domTimersThrottlingEnabled", NO);

        WKWebView* view =
            [[WKWebView alloc] initWithFrame:parent.bounds
                               configuration:configuration];
        view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        view.wantsLayer = YES;
        [parent addSubview:view];
        impl_->webView = view;

        const std::string url = url::normalize(initialUrl);
        if (!url.empty()) {
            navigate(url);
        }
        return true;
    }
}

void WebEngine::navigate(const std::string& url) {
    if (impl_->webView == nil) {
        return;
    }

    @autoreleasepool {
        const std::string normalized = url::normalize(url);
        if (normalized.empty()) {
            return;
        }
        NSString* text = [NSString stringWithUTF8String:normalized.c_str()];
        NSURL* target = text != nil ? [NSURL URLWithString:text] : nil;
        if (target == nil) {
            return;
        }
        NSURLRequest* request =
            [NSURLRequest requestWithURL:target
                            cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                        timeoutInterval:30.0];
        [impl_->webView loadRequest:request];
    }
}

void WebEngine::reload() {
    if (impl_->webView == nil) {
        return;
    }
    @autoreleasepool {
        NSURL* current = impl_->webView.URL;
        if (current == nil) {
            return;
        }
        NSURLRequest* request =
            [NSURLRequest requestWithURL:current
                            cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                        timeoutInterval:30.0];
        [impl_->webView loadRequest:request];
    }
}

void WebEngine::focus() {
    [impl_->webView becomeFirstResponder];
}

} // namespace catbrowser
