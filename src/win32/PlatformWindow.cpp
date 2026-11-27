#include "PlatformWindow.h"

#include "../common/Search.h"
#include "../common/Text.h"
#include "../common/UrlUtils.h"

#include <algorithm>
#include <commctrl.h>
#include <cwctype>
#include <iterator>

namespace catbrowser {
namespace {

constexpr int kUrlBarHeight = 32;
constexpr int kNavButtonWidth = 28;
constexpr int kModeButtonWidth = 48;
constexpr int kEngineButtonWidth = 52;
constexpr int kThemeButtonWidth = 30;
constexpr int kTranslateButtonWidth = 32;
constexpr int kDirectionButtonWidth = 48;
constexpr int kGoButtonWidth = 52;

constexpr int kEditControlId = 100;
constexpr int kGoButtonId = 101;
constexpr int kTranslateButtonId = 102;
constexpr int kDirectionButtonId = 103;
constexpr int kBackButtonId = 104;
constexpr int kForwardButtonId = 105;
constexpr int kReloadButtonId = 106;
constexpr int kFullscreenButtonId = 107;
constexpr int kModeButtonId = 108;
constexpr int kEngineButtonId = 109;
constexpr int kThemeButtonId = 110;

// One launcher window per process; used only by the edit control subclass.
PlatformWindow* g_activeWindow = nullptr;

std::wstring className() {
    return L"CatBrowserWindowClass";
}

HWND createButton(HINSTANCE instance, HWND parent, const wchar_t* label, int id) {
    return CreateWindowExW(
        0, L"BUTTON", label,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        0, 0, 0, 0, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        instance, nullptr);
}

std::wstring editText(HWND edit) {
    const int length = GetWindowTextLengthW(edit);
    std::wstring text(static_cast<size_t>(length), L'\0');
    if (length > 0) {
        GetWindowTextW(edit, text.data(), length + 1);
    }
    return text;
}

std::wstring trim(std::wstring value) {
    const auto isSpace = [](wchar_t c) { return std::iswspace(c) != 0; };
    const auto first = std::find_if_not(value.begin(), value.end(), isSpace);
    if (first == value.end()) {
        return {};
    }
    const auto last = std::find_if_not(value.rbegin(), value.rend(), isSpace).base();
    return std::wstring(first, last);
}

} // namespace

PlatformWindow::PlatformWindow(HINSTANCE instance)
    : instance_(instance != nullptr ? instance : GetModuleHandleW(nullptr)) {}

PlatformWindow::~PlatformWindow() {
    if (urlEdit_ != nullptr && originalEditProc_ != nullptr) {
        SetWindowLongPtrW(urlEdit_, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(originalEditProc_));
    }
    if (uiFont_ != nullptr) {
        DeleteObject(uiFont_);
    }
    if (darkBrush_ != nullptr) {
        DeleteObject(darkBrush_);
    }
    if (lightBrush_ != nullptr) {
        DeleteObject(lightBrush_);
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

    darkBrush_ = CreateSolidBrush(RGB(32, 32, 32));
    lightBrush_ = CreateSolidBrush(RGB(255, 255, 255));

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

void PlatformWindow::setEngine(search::Engine engine) {
    engine_ = engine;
    refreshButtonText();
}

void PlatformWindow::setTheme(bool dark) {
    darkTheme_ = dark;
    refreshButtonText();
    if (hwnd_ != nullptr) {
        SetClassLongPtrW(hwnd_, GCLP_HBRBACKGROUND,
                         reinterpret_cast<LONG_PTR>(controlBrush()));
        InvalidateRect(hwnd_, nullptr, TRUE);
    }
}

void PlatformWindow::setNavigationState(bool canBack, bool canForward) {
    if (backButton_ != nullptr) {
        EnableWindow(backButton_, canBack ? TRUE : FALSE);
    }
    if (forwardButton_ != nullptr) {
        EnableWindow(forwardButton_, canForward ? TRUE : FALSE);
    }
}

void PlatformWindow::refreshButtonText() {
    if (modeButton_ != nullptr) {
        SetWindowTextW(modeButton_, searchMode_ ? L"検索" : L"URL");
    }
    if (engineButton_ != nullptr) {
        SetWindowTextW(engineButton_, search::engineLabel(engine_));
    }
    if (themeButton_ != nullptr) {
        SetWindowTextW(themeButton_, darkTheme_ ? L"夜" : L"昼");
    }
    if (directionButton_ != nullptr) {
        SetWindowTextW(directionButton_, translateToJapanese_ ? L"→JA" : L"→EN");
    }
    if (urlEdit_ != nullptr) {
        SendMessageW(urlEdit_, EM_SETCUEBANNER, TRUE,
                     reinterpret_cast<LPARAM>(searchMode_
                         ? L"検索語を入力 (Enter で検索)"
                         : L"URL を入力 (http / https)"));
    }
    for (HWND button : {modeButton_, engineButton_, themeButton_, directionButton_}) {
        if (button != nullptr) {
            InvalidateRect(button, nullptr, TRUE);
        }
    }
}

void PlatformWindow::browseHistory(bool older) {
    if (urlEdit_ == nullptr || history_.empty()) {
        return;
    }
    if (older) {
        if (historyIndex_ < 0) {
            historyDraft_ = editText(urlEdit_);
            historyIndex_ = 0;
        } else if (historyIndex_ + 1 < static_cast<int>(history_.size())) {
            ++historyIndex_;
        } else {
            return;
        }
    } else {
        if (historyIndex_ <= 0) {
            historyIndex_ = -1;
            SetWindowTextW(urlEdit_, historyDraft_.c_str());
        } else {
            --historyIndex_;
        }
    }
    const std::wstring& value =
        historyIndex_ >= 0 ? history_[static_cast<size_t>(historyIndex_)]
                           : historyDraft_;
    SetWindowTextW(urlEdit_, value.c_str());
    const int length = GetWindowTextLengthW(urlEdit_);
    SendMessageW(urlEdit_, EM_SETSEL, length, length);
}

void PlatformWindow::layout() {
    RECT client{};
    GetClientRect(hwnd_, &client);

    const int width = client.right;
    const int height = client.bottom;
    const int fixedWidth =
        4 * kNavButtonWidth + kModeButtonWidth + kEngineButtonWidth +
        kThemeButtonWidth + kTranslateButtonWidth + kDirectionButtonWidth +
        kGoButtonWidth;
    const int barWidth = std::max(1, width - fixedWidth);
    const int webHeight = std::max(1, height - kUrlBarHeight);

    MoveWindow(urlEdit_, 0, 0, barWidth, kUrlBarHeight, TRUE);
    int x = barWidth;
    struct Button { HWND hwnd; int width; };
    const Button buttons[] = {
        {backButton_, kNavButtonWidth},
        {forwardButton_, kNavButtonWidth},
        {reloadButton_, kNavButtonWidth},
        {fullscreenButton_, kNavButtonWidth},
        {modeButton_, kModeButtonWidth},
        {engineButton_, kEngineButtonWidth},
        {themeButton_, kThemeButtonWidth},
        {translateButton_, kTranslateButtonWidth},
        {directionButton_, kDirectionButtonWidth},
        {goButton_, kGoButtonWidth},
    };
    for (const Button& button : buttons) {
        MoveWindow(button.hwnd, x, 0, button.width, kUrlBarHeight, TRUE);
        x += button.width;
    }
    MoveWindow(webHost_, 0, kUrlBarHeight, std::max(1, width), webHeight, TRUE);

    if (onResize_) {
        onResize_(width, webHeight);
    }
}

void PlatformWindow::submitUrl() {
    if (urlEdit_ == nullptr) {
        return;
    }

    const std::wstring raw = trim(editText(urlEdit_));
    if (raw.empty()) {
        MessageBeep(MB_ICONWARNING);
        return;
    }
    const std::string utf8 = text::wideToUtf8(raw);
    const std::string url = searchMode_
                                ? search::buildUrl(engine_, utf8)
                                : url::normalize(utf8);
    if (url.empty()) {
        MessageBeep(MB_ICONWARNING);
        return;
    }
    historyIndex_ = -1;
    historyDraft_.clear();
    if (onHistoryCommitted_) {
        onHistoryCommitted_(utf8);
    }
    if (onNavigate_) {
        onNavigate_(url);
    }
}

LRESULT CALLBACK PlatformWindow::editProc(HWND hwnd, UINT message,
                                          WPARAM wParam, LPARAM lParam) {
    if (g_activeWindow != nullptr && message == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            g_activeWindow->submitUrl();
            return 0;
        }
        if (wParam == VK_UP || wParam == VK_DOWN) {
            g_activeWindow->browseHistory(wParam == VK_UP);
            return 0;
        }
        if (wParam == VK_F5 && g_activeWindow->onReload_) {
            g_activeWindow->onReload_();
            return 0;
        }
        if (wParam == VK_F11) {
            g_activeWindow->toggleFullscreen();
            return 0;
        }
    }
    const WNDPROC original = g_activeWindow != nullptr
                                 ? g_activeWindow->originalEditProc_
                                 : nullptr;
    return CallWindowProcW(original != nullptr ? original : DefWindowProcW,
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
            self->backButton_ = createButton(self->instance_, hwnd, L"←", kBackButtonId);
            self->forwardButton_ = createButton(self->instance_, hwnd, L"→", kForwardButtonId);
            self->reloadButton_ = createButton(self->instance_, hwnd, L"再", kReloadButtonId);
            self->fullscreenButton_ = createButton(self->instance_, hwnd, L"全", kFullscreenButtonId);
            self->modeButton_ = createButton(self->instance_, hwnd, L"URL", kModeButtonId);
            self->engineButton_ = createButton(self->instance_, hwnd,
                                              search::engineLabel(self->engine_),
                                              kEngineButtonId);
            self->themeButton_ = createButton(self->instance_, hwnd,
                                             self->darkTheme_ ? L"夜" : L"昼",
                                             kThemeButtonId);
            self->translateButton_ = createButton(self->instance_, hwnd, L"訳", kTranslateButtonId);
            self->directionButton_ = createButton(self->instance_, hwnd, L"→JA", kDirectionButtonId);
            self->goButton_ = createButton(self->instance_, hwnd, L"GO", kGoButtonId);
            // A plain STATIC child is enough of a parent for WebView2.
            self->webHost_ = CreateWindowExW(
                0, L"STATIC", nullptr,
                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                0, 0, 0, 0, hwnd, nullptr, self->instance_, nullptr);

            // Variable font on Windows 11, with Segoe UI as the automatic
            // fallback on older systems. ClearType keeps small text crisp.
            self->uiFont_ = CreateFontW(
                -10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                L"Segoe UI Variable Text");
            const HFONT font = self->uiFont_ != nullptr
                                   ? self->uiFont_
                                   : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            for (HWND control : {self->urlEdit_, self->backButton_,
                                 self->forwardButton_, self->reloadButton_,
                                 self->fullscreenButton_, self->modeButton_,
                                 self->engineButton_, self->themeButton_,
                                 self->translateButton_, self->directionButton_,
                                 self->goButton_}) {
                SendMessageW(control, WM_SETFONT,
                             reinterpret_cast<WPARAM>(font), TRUE);
            }
            SendMessageW(self->urlEdit_, EM_SETMARGINS, EC_LEFTMARGIN,
                         MAKELPARAM(8, 0));
            self->refreshButtonText();
            self->setNavigationState(false, false);
            SetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND,
                             reinterpret_cast<LONG_PTR>(self->controlBrush()));

            self->originalEditProc_ = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(self->urlEdit_, GWLP_WNDPROC,
                                  reinterpret_cast<LONG_PTR>(&PlatformWindow::editProc)));

            self->layout();
            SetFocus(self->urlEdit_);
            return 0;
        }

        case WM_SIZE:
            self->layout();
            // Idle rule: no rendering at all while minimized.
            if (self->onVisibility_) {
                if (wParam == SIZE_MINIMIZED) {
                    self->onVisibility_(false);
                } else if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED) {
                    self->onVisibility_(true);
                }
            }
            return 0;

        case WM_ACTIVATE:
            // Also stop rendering while another window is in front.
            if (self->onVisibility_) {
                self->onVisibility_(LOWORD(wParam) != WA_INACTIVE &&
                                    !IsIconic(hwnd));
            }
            return 0;

        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED) {
                switch (LOWORD(wParam)) {
                case kGoButtonId:
                    self->submitUrl();
                    break;
                case kBackButtonId:
                    // Navigation is handled by the engine's back/forward API;
                    // wired by App_win through a dedicated callback below.
                    if (self->onBack_) {
                        self->onBack_();
                    }
                    break;
                case kForwardButtonId:
                    if (self->onForward_) {
                        self->onForward_();
                    }
                    break;
                case kReloadButtonId:
                    if (self->onReload_) {
                        self->onReload_();
                    }
                    break;
                case kFullscreenButtonId:
                    self->toggleFullscreen();
                    break;
                case kModeButtonId:
                    self->searchMode_ = !self->searchMode_;
                    self->refreshButtonText();
                    break;
                case kEngineButtonId: {
                    const int next = (static_cast<int>(self->engine_) + 1) % 3;
                    self->engine_ = static_cast<search::Engine>(next);
                    self->refreshButtonText();
                    if (self->onEngineChanged_) {
                        self->onEngineChanged_(self->engine_);
                    }
                    break;
                }
                case kThemeButtonId:
                    self->setTheme(!self->darkTheme_);
                    if (self->onThemeChanged_) {
                        self->onThemeChanged_(self->darkTheme_);
                    }
                    break;
                case kTranslateButtonId:
                    if (self->onTranslate_) {
                        self->onTranslate_(self->translateToJapanese_);
                    }
                    break;
                case kDirectionButtonId:
                    self->translateToJapanese_ = !self->translateToJapanese_;
                    self->refreshButtonText();
                    break;
                default:
                    break;
                }
            }
            return 0;

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetTextColor(hdc, self->darkTheme_ ? RGB(238, 238, 238) : RGB(16, 16, 16));
            SetBkColor(hdc, self->darkTheme_ ? RGB(32, 32, 32) : RGB(255, 255, 255));
            return reinterpret_cast<LRESULT>(self->controlBrush());
        }

        case WM_DRAWITEM: {
            auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            wchar_t label[64]{};
            GetWindowTextW(draw->hwndItem, label, static_cast<int>(std::size(label)));
            bool accent = false;
            COLORREF accentColor = RGB(88, 92, 110);
            switch (draw->CtlID) {
            case kGoButtonId:
                accent = true;
                accentColor = RGB(0, 120, 215);
                break;
            case kTranslateButtonId:
                accent = true;
                accentColor = RGB(0, 145, 105);
                break;
            case kModeButtonId:
                accent = true;
                accentColor = RGB(20, 115, 140);
                break;
            case kEngineButtonId:
                accent = true;
                accentColor = RGB(105, 80, 165);
                break;
            default:
                break;
            }
            COLORREF background = accent
                                      ? accentColor
                                      : (self->darkTheme_ ? RGB(52, 52, 52)
                                                          : RGB(226, 226, 226));
            COLORREF foreground = accent
                                      ? RGB(255, 255, 255)
                                      : (self->darkTheme_ ? RGB(238, 238, 238)
                                                          : RGB(16, 16, 16));
            if ((draw->itemState & ODS_SELECTED) != 0) {
                background = RGB(GetRValue(background) / 2,
                                 GetGValue(background) / 2,
                                 GetBValue(background) / 2);
            } else if ((draw->itemState & ODS_DISABLED) != 0) {
                foreground = self->darkTheme_ ? RGB(110, 110, 110)
                                              : RGB(150, 150, 150);
            }
            HBRUSH brush = CreateSolidBrush(background);
            FillRect(draw->hDC, &draw->rcItem, brush);
            DeleteObject(brush);
            SetBkMode(draw->hDC, TRANSPARENT);
            SetTextColor(draw->hDC, foreground);
            DrawTextW(draw->hDC, label, -1, &draw->rcItem,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }

        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize.x = 480;
            mmi->ptMinTrackSize.y = 240;
            return 0;
        }

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
            if (self->uiFont_ != nullptr) {
                DeleteObject(self->uiFont_);
                self->uiFont_ = nullptr;
            }
            if (self->darkBrush_ != nullptr) {
                DeleteObject(self->darkBrush_);
                self->darkBrush_ = nullptr;
            }
            if (self->lightBrush_ != nullptr) {
                DeleteObject(self->lightBrush_);
                self->lightBrush_ = nullptr;
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
