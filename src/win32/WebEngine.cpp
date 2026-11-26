#include "WebEngine.h"

#include "../common/Log.h"
#include "../common/Text.h"
#include "../common/Translation.h"
#include "../common/UrlUtils.h"

#include <algorithm>
#include <shlwapi.h>
#include <utility>

// Newer WebView2 SDKs ship a header-only CoreWebView2EnvironmentOptions class;
// older ones expose CreateCoreWebView2EnvironmentOptions() instead.
#if defined(__has_include)
#if __has_include(<WebView2EnvironmentOptions.h>)
#include <WebView2EnvironmentOptions.h>
#define CATBROWSER_WV2_MODERN_OPTIONS 1
#endif
#endif

namespace catbrowser {
namespace {

// Keep only the switches that actually help: GPU acceleration plus removal of
// background work we never use. Deliberately NOT set:
//   --disable-frame-rate-limit       (pegs the GPU/CPU even while idle)
//   --disable-background-timer-throttling / --disable-renderer-backgrounding
//   --process-per-site               (extra browser processes)
//   --disable-dev-shm-usage          (forces disk I/O instead of shared memory)
// Browser flags are not a long-term supported WebView2 API and may be ignored
// by the runtime; they are injected via the documented AdditionalBrowserArguments.
constexpr wchar_t kBrowserArguments[] =
    L"--ignore-gpu-blocklist "
    L"--enable-gpu-rasterization "
    L"--use-angle=d3d11 "
    L"--disable-background-networking "
    L"--disable-component-update "
    L"--disable-domain-reliability "
    L"--disable-sync "
    L"--disable-default-apps "
    L"--no-first-run "
    L"--metrics-recording-only "
    L"--disable-features=Translate,BackForwardCache,OptimizationHints,MediaRouter";

std::wstring userDataFolder() {
    wchar_t temp[MAX_PATH]{};
    const DWORD length = GetTempPathW(MAX_PATH, temp);
    if (length == 0 || length >= MAX_PATH) {
        return L"C:\\CatBrowserWebView2";
    }
    return std::wstring(temp) + L"CatBrowser\\WebView2";
}

} // namespace

bool WebEngine::create(HWND host, const std::string& initialUrl) {
    host_ = host;
    pendingUrl_ = url::normalize(initialUrl);

#ifdef CATBROWSER_WV2_MODERN_OPTIONS
    auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    if (!options) {
        log::warn("Could not create CoreWebView2EnvironmentOptions");
        return false;
    }
#else
    Microsoft::WRL::ComPtr<ICoreWebView2EnvironmentOptions> options;
    if (FAILED(CreateCoreWebView2EnvironmentOptions(options.GetAddressOf()))) {
        log::warn("CreateCoreWebView2EnvironmentOptions failed");
        return false;
    }
#endif
    options->put_AdditionalBrowserArguments(kBrowserArguments);

    const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userDataFolder().c_str(), options.Get(),
        Microsoft::WRL::Callback<
            ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                if (FAILED(result) || environment == nullptr) {
                    log::warn("WebView2 environment creation failed");
                    MessageBoxW(nullptr,
                                L"This launcher needs the Microsoft Edge WebView2 Runtime.\n"
                                L"Please install the Evergreen Runtime and restart.",
                                L"CatBrowser", MB_ICONERROR | MB_OK);
                    return result;
                }

                environment_ = environment;
                return environment->CreateCoreWebView2Controller(
                    host_,
                    Microsoft::WRL::Callback<
                        ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT controllerResult,
                               ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(controllerResult) || controller == nullptr) {
                                log::warn("WebView2 controller creation failed");
                                return controllerResult;
                            }

                            controller_ = controller;
                            controller_->get_CoreWebView2(webView_.GetAddressOf());
                            applySettings();
                            registerAccelerators();

                            RECT bounds{};
                            GetClientRect(host_, &bounds);
                            controller_->put_Bounds(bounds);
                            controller_->put_IsVisible(TRUE);

                            if (!pendingUrl_.empty()) {
                                navigate(pendingUrl_);
                                pendingUrl_.clear();
                            }
                            return S_OK;
                        }).Get());
            }).Get());

    if (FAILED(hr)) {
        log::warn("Failed to start WebView2 environment creation");
        return false;
    }
    return true;
}

void WebEngine::applySettings() {
    Microsoft::WRL::ComPtr<ICoreWebView2Settings> settings;
    if (FAILED(webView_->get_Settings(settings.GetAddressOf()))) {
        return;
    }

    settings->put_IsScriptEnabled(TRUE);
    settings->put_IsWebMessageEnabled(FALSE);
    settings->put_AreDefaultContextMenusEnabled(FALSE); // no right-click menu
    settings->put_IsStatusBarEnabled(FALSE);            // no status bar
    settings->put_IsZoomControlEnabled(FALSE);          // no zoom UI
    settings->put_AreDevToolsEnabled(FALSE);
    settings->put_IsBuiltInErrorPageEnabled(FALSE);

    Microsoft::WRL::ComPtr<ICoreWebView2Settings3> settings3;
    if (SUCCEEDED(settings.As(&settings3))) {
        settings3->put_AreBrowserAcceleratorKeysEnabled(FALSE);
    }
    Microsoft::WRL::ComPtr<ICoreWebView2Settings4> settings4;
    if (SUCCEEDED(settings.As(&settings4))) {
        settings4->put_IsPasswordAutosaveEnabled(FALSE);
        settings4->put_IsGeneralAutofillEnabled(FALSE);
    }
    Microsoft::WRL::ComPtr<ICoreWebView2Settings5> settings5;
    if (SUCCEEDED(settings.As(&settings5))) {
        settings5->put_IsPinchZoomEnabled(FALSE);
    }
    Microsoft::WRL::ComPtr<ICoreWebView2Settings6> settings6;
    if (SUCCEEDED(settings.As(&settings6))) {
        settings6->put_IsSwipeNavigationEnabled(FALSE);
    }
}

void WebEngine::registerAccelerators() {
    // F5/F11 must work even while WebView2 itself has keyboard focus.
    controller_->add_AcceleratorKeyPressed(
        Microsoft::WRL::Callback<
            ICoreWebView2AcceleratorKeyPressedEventHandler>(
            [this](ICoreWebView2Controller*,
                   ICoreWebView2AcceleratorKeyPressedEventArgs* args) -> HRESULT {
                COREWEBVIEW2_KEY_EVENT_KIND kind{};
                UINT virtualKey = 0;
                if (FAILED(args->get_KeyEventKind(&kind)) ||
                    FAILED(args->get_VirtualKey(&virtualKey))) {
                    return S_OK;
                }
                const bool isDown =
                    kind == COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN ||
                    kind == COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN;
                if (!isDown) {
                    return S_OK;
                }

                // Set Handled before making any outgoing COM calls: in
                // windowed mode WebView2 blocks cross-process calls until the
                // accelerator event is handled.
                if (virtualKey == VK_F5) {
                    args->put_Handled(TRUE);
                    if (onReload_) {
                        onReload_();
                    }
                } else if (virtualKey == VK_F11) {
                    args->put_Handled(TRUE);
                    if (onFullscreenToggle_) {
                        onFullscreenToggle_();
                    }
                }
                return S_OK;
            }).Get(),
        &acceleratorToken_);
}

void WebEngine::navigate(const std::string& url) {
    const std::string normalized = url::normalize(url);
    if (normalized.empty()) {
        return;
    }
    if (!webView_) {
        pendingUrl_ = normalized;
        return;
    }
    webView_->Navigate(text::utf8ToWide(normalized).c_str());
}

void WebEngine::reload() {
    if (!webView_) {
        return;
    }
    // CDP is used because ICoreWebView2::Reload() has no "ignore cache" option.
    webView_->CallDevToolsProtocolMethod(
        L"Page.reload", L"{\"ignoreCache\":true}",
        Microsoft::WRL::Callback<
            ICoreWebView2CallDevToolsProtocolMethodCompletedHandler>(
            [](HRESULT, LPCWSTR) -> HRESULT { return S_OK; }).Get());
}

void WebEngine::focus() {
    if (host_ != nullptr) {
        SetFocus(host_);
    }
    if (controller_) {
        controller_->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    }
}

void WebEngine::resize(int width, int height) {
    if (!controller_) {
        return;
    }
    RECT bounds{};
    bounds.left = 0;
    bounds.top = 0;
    bounds.right = std::max(1, width);
    bounds.bottom = std::max(1, height);
    controller_->put_Bounds(bounds);
}

void WebEngine::setVisible(bool visible) {
    if (controller_) {
        controller_->put_IsVisible(visible ? TRUE : FALSE);
    }
}

void WebEngine::translate(translation::Direction direction) {
    if (!webView_) {
        return;
    }
    const std::wstring script =
        text::utf8ToWide(translation::buildTranslateScript(direction));
    if (script.empty()) {
        return;
    }
    webView_->ExecuteScriptAsync(
        script.c_str(),
        Microsoft::WRL::Callback<
            ICoreWebView2ExecuteScriptCompletedHandler>(
            [](HRESULT, LPCWSTR) -> HRESULT { return S_OK; }).Get());
}

} // namespace catbrowser
