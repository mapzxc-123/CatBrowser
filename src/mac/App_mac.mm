#import <AppKit/AppKit.h>

#include "../App.h"
#include "PlatformWindow.h"
#include "WebEngine.h"

@interface CBAppDelegate : NSObject <NSApplicationDelegate>
@property (nonatomic, assign) void* windowPtr;
@property (nonatomic, assign) void* enginePtr;
@property (nonatomic, strong) id<NSObject> activity;
@property (nonatomic, strong) id keyMonitor;
@end

@implementation CBAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;

    auto* window = static_cast<astb3ast::PlatformWindow*>(self.windowPtr);
    auto* engine = static_cast<astb3ast::WebEngine*>(self.enginePtr);

    // No nib: create a minimal menu so the app can be quit from the Dock.
    NSMenu* mainMenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];
    NSMenuItem* appItem = [[NSMenuItem alloc] initWithTitle:@"AstB3ast"
                                                     action:nil
                                              keyEquivalent:@""];
    NSMenu* appMenu = [[NSMenu alloc] initWithTitle:@"AstB3ast"];
    [appMenu addItemWithTitle:@"Quit AstB3ast"
                       action:@selector(terminate:)
                keyEquivalent:@"q"];
    [appItem setSubmenu:appMenu];
    [mainMenu addItem:appItem];
    [NSApp setMainMenu:mainMenu];

    [window show];

    // Function keys can be swallowed by WKWebView; a local monitor sees them
    // first regardless of which control currently owns first responder.
    self.keyMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown
        handler:^NSEvent*(NSEvent* event) {
            if ((event.modifierFlags & NSEventModifierFlagCommand) != 0) {
                return event;
            }
            // macOS virtual keycodes: F5 = 96, F11 = 103.
            if (event.keyCode == 96) {
                engine->reload();
                return nil;
            }
            if (event.keyCode == 103) {
                window->toggleFullscreen();
                return nil;
            }
            return event;
        }];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    (void)sender;
    return YES;
}

- (void)applicationWillTerminate:(NSNotification*)notification {
    (void)notification;
    if (self.keyMonitor != nil) {
        [NSEvent removeMonitor:self.keyMonitor];
        self.keyMonitor = nil;
    }
}

@end

namespace astb3ast {

int runPlatform(const AppConfig& config) {
    @autoreleasepool {
        NSApplication* application = [NSApplication sharedApplication];
        [application setActivationPolicy:NSApplicationActivationPolicyRegular];

        // Disable App Nap / timer coalescing for the whole process lifetime.
        const NSActivityOptions options =
            NSActivityUserInitiated | NSActivityLatencyCritical;
        id<NSObject> activity =
            [[NSProcessInfo processInfo] beginActivityWithOptions:options
                                                           reason:@"AstB3ast latency-critical rendering"];

        PlatformWindow window(config.initialUrl);
        WebEngine engine;

        window.setOnNavigate([&](const std::string& url) {
            engine.navigate(url);
            engine.focus(); // Enter moves focus into WKWebView immediately.
        });
        window.setOnReload([&] { engine.reload(); });

        window.create();
        engine.create(window.webContainer(), config.initialUrl);

        CBAppDelegate* delegate = [[CBAppDelegate alloc] init];
        delegate.windowPtr = &window;
        delegate.enginePtr = &engine;
        delegate.activity = activity;
        application.delegate = delegate;

        [application run];

        [[NSProcessInfo processInfo] endActivity:activity];
        return 0;
    }
}

} // namespace astb3ast
