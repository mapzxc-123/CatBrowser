#include "PlatformWindow.h"

#include "../common/Text.h"
#include "../common/UrlUtils.h"

#include <algorithm>

namespace catbrowser {
namespace {

constexpr int kUrlBarHeight = 30;
constexpr int kGoButtonWidth = 64;
constexpr int kEditControlId = 100;
constexpr int kGoButtonId = 101;

// One launcher window per process; used only by the edit control subclass.
PlatformWindow* g_activeWindow = nullptr;

std::wstring className() {
    return L"CatBrowserWindowClass";
}

} // namespace

PlatformWindow::PlatformWindow(HINSTANCE instance)
    : instance_(instance != nullptr ? instance : GetModuleHandleW(nullptr)) {}

PlatformWindow::~PlatformWindow() {
    if (urlEdit_ != nullptr && originalEditProc_ != nullptr) {
        SetWindowLongPtrW(urlEdit_, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(originalEditProc_));
    }
    if (g_activeWindow == this) {
        g_activeWindow = nullptr;
    }
}

bool PlatformWindow::create() {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &PlatformWindow::windowProc;
        wc.hInstance = instance_;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = className().c_str();
        if (RegisterClassExW(&wc) == 0) {
            return false;
        }
        registered = true;
    }

    hwnd_ = CreateWindowExW(
        0, className().c_str(), L"CatBrowser",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
        nullptr, nullptr, instance_, this);
    if (hwnd_ == nullptr) {
        return false;
    }

    g_activeWindow = this;
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    return true;
}

int PlatformWindow::messageLoop() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

void PlatformWindow::toggleFullscreen() {
    if (hwnd_ == nullptr) {
        return;
    }

    if (!fullscreen_) {
        GetWindowRect(hwnd_, &savedRect_);
        savedStyle_ = GetWindowLongPtrW(hwnd_, GWL_STYLE);

        const LONG_PTR borderless =
            (savedStyle_ & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX |
                             WS_MAXIMIZEBOX)) |
            WS_POPUP;
        SetWindowLongPtrW(hwnd_, GWL_STYLE, borderless);

        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST),
                        &monitorInfo);
        const RECT& area = monitorInfo.rcMonitor;
        SetWindowPos(hwnd_, HWND_TOP, area.left, area.top,
                     area.right - area.left, area.bottom - area.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        fullscreen_ = true;
    } else {
        SetWindowLongPtrW(hwnd_, GWL_STYLE, savedStyle_);
        SetWindowPos(hwnd_, nullptr, savedRect_.left, savedRect_.top,
                     savedRect_.right - savedRect_.left,
                     savedRect_.bottom - savedRect_.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        fullscreen_ = false;
    }
}

void PlatformWindow::layout() {
    RECT client{};
    GetClientRect(hwnd_, &client);

    const int width = client.right;
    const int height = client.bottom;
    const int barWidth = std::max(1, width - kGoButtonWidth);
    const int webHeight = std::max(1, height - kUrlBarHeight);

    MoveWindow(urlEdit_, 0, 0, barWidth, kUrlBarHeight, TRUE);
    MoveWindow(goButton_, width - kGoButtonWidth, 0,
               kGoButtonWidth, kUrlBarHeight, TRUE);
    MoveWindow(webHost_, 0, kUrlBarHeight, std::max(1, width), webHeight, TRUE);

    if (onResize_) {
        onResize_(width, webHeight);
    }
}

void PlatformWindow::submitUrl() {
    if (urlEdit_ == nullptr) {
        return;
    }

    const int length = GetWindowTextLengthW(urlEdit_);
    std::wstring raw(static_cast<size_t>(length) + 1, L'\0');
    if (length > 0) {
        GetWindowTextW(urlEdit_, raw.data(), static_cast<int>(raw.size()));
        raw.resize(static_cast<size_t>(length));
    } else {
        raw.clear();
    }

    const std::string url = url::normalize(text::wideToUtf8(raw));
    if (url.empty()) {
        MessageBeep(MB_ICONWARNING);
        return;
    }
    if (onNavigate_) {
        onNavigate_(url);
    }
}

LRESULT CALLBACK PlatformWindow::editProc(HWND hwnd, UINT message,
                                          WPARAM wParam, LPARAM lParam) {
    if (message == WM_KEYDOWN && wParam == VK_RETURN &&
        g_activeWindow != nullptr) {
        g_activeWindow->submitUrl();
        return 0;
    }
    if (message == WM_KEYDOWN && wParam == VK_F5 &&
        g_activeWindow != nullptr && g_activeWindow->onReload_) {
        g_activeWindow->onReload_();
        return 0;
    }
    if (message == WM_KEYDOWN && wParam == VK_F11 &&
        g_activeWindow != nullptr) {
        g_activeWindow->toggleFullscreen();
        return 0;
    }
    return CallWindowProcW(g_activeWindow != nullptr
                               ? g_activeWindow->originalEditProc_
                               : DefWindowProcW,
                           hwnd, message, wParam, lParam);
}

LRESULT CALLBACK PlatformWindow::windowProc(HWND hwnd, UINT message,
                                            WPARAM wParam, LPARAM lParam) {
    PlatformWindow* self = nullptr;
    if (message == WM_CREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<PlatformWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<PlatformWindow*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self != nullptr) {
        switch (message) {
        case WM_CREATE: {
            self->urlEdit_ = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                0, 0, 0, 0, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kEditControlId)),
                self->instance_, nullptr);
            self->goButton_ = CreateWindowExW(
                0, L"BUTTON", L"GO",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                0, 0, 0, 0, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kGoButtonId)),
                self->instance_, nullptr);
            // A plain STATIC child is enough of a parent for WebView2.
            self->webHost_ = CreateWindowExW(
                0, L"STATIC", nullptr,
                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                0, 0, 0, 0, hwnd, nullptr, self->instance_, nullptr);

            const HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            SendMessageW(self->urlEdit_, WM_SETFONT,
                         reinterpret_cast<WPARAM>(font), TRUE);
            SendMessageW(self->goButton_, WM_SETFONT,
                         reinterpret_cast<WPARAM>(font), TRUE);

            self->originalEditProc_ = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(self->urlEdit_, GWLP_WNDPROC,
                                  reinterpret_cast<LONG_PTR>(&PlatformWindow::editProc)));

            self->layout();
            SetFocus(self->urlEdit_);
            return 0;
        }

        case WM_SIZE:
            self->layout();
            return 0;

        case WM_COMMAND:
            if (LOWORD(wParam) == kGoButtonId && HIWORD(wParam) == BN_CLICKED) {
                self->submitUrl();
            }
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_F5) {
                if (self->onReload_) {
                    self->onReload_();
                }
                return 0;
            }
            if (wParam == VK_F11) {
                self->toggleFullscreen();
                return 0;
            }
            break;

        case WM_DESTROY:
            if (self->urlEdit_ != nullptr && self->originalEditProc_ != nullptr) {
                SetWindowLongPtrW(self->urlEdit_, GWLP_WNDPROC,
                                  reinterpret_cast<LONG_PTR>(self->originalEditProc_));
                self->originalEditProc_ = nullptr;
            }
            if (g_activeWindow == self) {
                g_activeWindow = nullptr;
            }
            PostQuitMessage(0);
            return 0;

        default:
            break;
        }
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace catbrowser
