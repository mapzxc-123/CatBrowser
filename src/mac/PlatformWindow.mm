#import <AppKit/AppKit.h>

#include "PlatformWindow.h"
#include "../common/UrlUtils.h"

#include <utility>

@class CBAppBridge;

namespace catbrowser {

struct PlatformWindow::Impl {
    __strong CBAppBridge* bridge = nil;
    __strong NSWindow* window = nil;
    __strong NSTextField* urlField = nil;
    __strong NSView* webContainer = nil;

    NSRect savedFrame = NSZeroRect;
    NSWindowStyleMask savedStyle = NSWindowStyleMaskBorderless;
    bool fullscreen = false;
    std::string initialUrl;

    std::function<void(const std::string&)> onNavigate;
    std::function<void()> onReload;
};

} // namespace catbrowser

@interface CBAppBridge : NSObject <NSWindowDelegate>
@property (nonatomic, assign) void* impl;
@property (nonatomic, weak) NSTextField* urlField;
- (void)submit:(id)sender;
- (void)reloadPressed:(id)sender;
@end

@implementation CBAppBridge

- (void)submit:(id)sender {
    auto* impl = static_cast<catbrowser::PlatformWindow::Impl*>(self.impl);
    if (impl == nil) {
        return;
    }
    NSString* value = self.urlField.stringValue;
    const char* utf8 = value != nil ? [value UTF8String] : nullptr;
    const std::string url = catbrowser::url::normalize(utf8 != nullptr ? utf8 : "");
    if (url.empty()) {
        NSBeep();
        return;
    }
    if (impl->onNavigate) {
        impl->onNavigate(url);
    }
}

- (void)reloadPressed:(id)sender {
    auto* impl = static_cast<catbrowser::PlatformWindow::Impl*>(self.impl);
    if (impl != nil && impl->onReload) {
        impl->onReload();
    }
}

@end

namespace catbrowser {

PlatformWindow::PlatformWindow(const std::string& initialUrl)
    : impl_(new Impl()) {
    impl_->initialUrl = initialUrl;
}

PlatformWindow::~PlatformWindow() {
    if (impl_ != nullptr) {
        impl_->bridge = nil;
        impl_->urlField = nil;
        impl_->webContainer = nil;
        impl_->window = nil;
        delete impl_;
        impl_ = nullptr;
    }
}

bool PlatformWindow::create() {
    @autoreleasepool {
        impl_->bridge = [[CBAppBridge alloc] init];
        impl_->bridge.impl = impl_;

        constexpr CGFloat kBarHeight = 30.0;
        constexpr CGFloat kButtonWidth = 70.0;
        const NSRect frame = NSMakeRect(0.0, 0.0, 1280.0, 720.0);

        const NSWindowStyleMask style =
            NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
            NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;

        NSWindow* window =
            [[NSWindow alloc] initWithContentRect:frame
                                        styleMask:style
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
        if (window == nil) {
            return false;
        }
        window.title = @"CatBrowser";
        window.delegate = impl_->bridge;
        [window center];
        impl_->window = window;

        NSView* root = window.contentView;
        const CGFloat width = frame.size.width;
        const CGFloat height = frame.size.height;

        NSTextField* field = [[NSTextField alloc]
            initWithFrame:NSMakeRect(0.0, height - kBarHeight,
                                     width - kButtonWidth, kBarHeight)];
        field.placeholderString = @"https://example.com";
        field.font = [NSFont systemFontOfSize:13.0];
        field.bezelStyle = NSTextFieldRoundedBezel;
        field.autoresizingMask = NSViewWidthSizable | NSViewMaxYMargin;
        field.target = impl_->bridge;
        field.action = @selector(submit:);
        [root addSubview:field];
        impl_->urlField = field;
        impl_->bridge.urlField = field;

        NSButton* button = [[NSButton alloc]
            initWithFrame:NSMakeRect(width - kButtonWidth,
                                     height - kBarHeight,
                                     kButtonWidth, kBarHeight)];
        button.title = @"GO";
        button.bezelStyle = NSBezelStyleRounded;
        button.font = [NSFont systemFontOfSize:13.0];
        button.autoresizingMask = NSViewMinXMargin | NSViewMaxYMargin;
        button.target = impl_->bridge;
        button.action = @selector(submit:);
        [root addSubview:button];

        NSView* container = [[NSView alloc]
            initWithFrame:NSMakeRect(0.0, 0.0, width, height - kBarHeight)];
        container.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        container.wantsLayer = YES; // Core Animation / Metal-backed compositing
        [root addSubview:container];
        impl_->webContainer = container;

        if (!impl_->initialUrl.empty()) {
            NSString* initial =
                [NSString stringWithUTF8String:impl_->initialUrl.c_str()];
            if (initial != nil) {
                field.stringValue = initial;
            }
        }
        return true;
    }
}

void PlatformWindow::show() {
    [impl_->window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

void PlatformWindow::toggleFullscreen() {
    NSWindow* window = impl_->window;
    if (window == nil) {
        return;
    }

    if (!impl_->fullscreen) {
        impl_->savedFrame = window.frame;
        impl_->savedStyle = window.styleMask;
        window.styleMask = NSWindowStyleMaskBorderless;
        [window setFrame:[[NSScreen mainScreen] frame] display:YES animate:NO];
        impl_->fullscreen = true;
    } else {
        window.styleMask = impl_->savedStyle;
        [window setFrame:impl_->savedFrame display:YES animate:NO];
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

} // namespace catbrowser
