#include "PlatformWindow.h"

#include "../common/Search.h"
#include "../common/Text.h"
#include "../common/UrlUtils.h"

#include <algorithm>
#include <commctrl.h>
#include <cwctype>
#include <iterator>

namespace astb3ast {
namespace {

constexpr int kBarHeight = 24;
constexpr int kButtonWidth = 26;
constexpr int kSearchButtonWidth = 30;
constexpr int kRevealZone = 10;
constexpr int kKeepVisibleZone = 24;

constexpr int kOverlayMaxWidth = 560;
constexpr int kOverlayMinWidth = 320;
constexpr int kOverlayHeight = 176;
constexpr int kOverlayMargin = 8;
constexpr int kOverlayRowHeight = 24;
constexpr int kOverlayGap = 4;
constexpr int kOverlayIconWidth = 24;

constexpr int kSearchButtonId = 100;
constexpr int kBackButtonId = 101;
constexpr int kForwardButtonId = 102;
constexpr int kReloadButtonId = 103;
constexpr int kFullscreenButtonId = 104;
constexpr int kTranslateButtonId = 105;
constexpr int kDirectionButtonId = 106;
constexpr int kThemeButtonId = 107;

constexpr int kOverlayPanelId = 199;
constexpr int kOverlayEditId = 200;
constexpr int kOverlayModeId = 201;
constexpr int kOverlayEngineId = 202;
constexpr int kOverlayListId = 203;
constexpr int kOverlayCloseId = 204;

// MDL2 glyphs. Keeping them as literals avoids an icon resource file.
constexpr const wchar_t* kGlyphSearch = L"\uE094";
constexpr const wchar_t* kGlyphBack = L"\uE0C6";
constexpr const wchar_t* kGlyphForward = L"\uE0C7";
constexpr const wchar_t* kGlyphReload = L"\uE072";
constexpr const wchar_t* kGlyphFullscreen = L"\uE1D8";
constexpr const wchar_t* kGlyphTranslate = L"\uE28F";
constexpr const wchar_t* kGlyphGlobe = L"\uE12B";
constexpr const wchar_t* kGlyphTheme = L"\uE708";
constexpr const wchar_t* kGlyphClose = L"\uE8BB";

// One launcher window per process; shared by the edit/list subclasses.
PlatformWindow* g_activeWindow = nullptr;

std::wstring className() {
    return L"AstB3astWindowClass";
}

HWND createButton(HINSTANCE instance, HWND parent, const wchar_t* label,
                  int id, bool visible) {
    DWORD style = WS_CHILD | WS_TABSTOP | BS_OWNERDRAW;
    if (visible) {
        style |= WS_VISIBLE;
    }
    return CreateWindowExW(
        0, L"BUTTON", label, style,
        0, 0, 0, 0, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        instance, nullptr);
}

std::wstring editText(HWND edit) {
    if (edit == nullptr) {
        return {};
    }
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

std::wstring engineGlyph(search::Engine engine) {
    switch (engine) {
    case search::Engine::DuckDuckGo: return L"D";
    case search::Engine::Google: return L"G";
    case search::Engine::Bing: return L"B";
    }
    return L"?";
}

bool usesIconFont(int controlId) {
    switch (controlId) {
    case kSearchButtonId:
    case kBackButtonId:
    case kForwardButtonId:
    case kReloadButtonId:
    case kFullscreenButtonId:
    case kTranslateButtonId:
    case kThemeButtonId:
    case kOverlayModeId:
    case kOverlayCloseId:
        return true;
    default:
        return false;
    }
}

int signedLowWord(LPARAM value) {
    return static_cast<short>(LOWORD(value));
}

int signedHighWord(LPARAM value) {
    return static_cast<short>(HIWORD(value));
}

} // namespace

PlatformWindow::PlatformWindow(HINSTANCE instance)
    : instance_(instance != nullptr ? instance : GetModuleHandleW(nullptr)) {}

PlatformWindow::~PlatformWindow() {
    if (overlayEdit_ != nullptr && originalEditProc_ != nullptr) {
        SetWindowLongPtrW(overlayEdit_, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(originalEditProc_));
    }
    if (overlayList_ != nullptr && originalListProc_ != nullptr) {
        SetWindowLongPtrW(overlayList_, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(originalListProc_));
    }
    if (uiFont_ != nullptr) {
        DeleteObject(uiFont_);
    }
    if (iconFont_ != nullptr) {
        DeleteObject(iconFont_);
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
        0, className().c_str(), L"AstB3ast",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
        nullptr, nullptr, instance_, this);
    if (hwnd_ == nullptr) {
        return false;
    }

    g_activeWindow = this;

    // WebView2 renders in child HWNDs owned by other processes, so mouse
    // movement over the page never reaches this window. Raw input is delivered
    // to our own HWND from the device event, with no polling or timers, which
    // lets the thin bar hide/reveal even while the pointer is over the page.
    RAWINPUTDEVICE mouse{};
    mouse.usUsagePage = 1;   // Generic Desktop Controls
    mouse.usUsage = 2;       // Mouse
    mouse.dwFlags = RIDEV_INPUTSINK;
    mouse.hwndTarget = hwnd_;
    RegisterRawInputDevices(&mouse, 1, sizeof(mouse));

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    return true;
}

int PlatformWindow::messageLoop() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        // WebView2 receives mouse messages in its own child HWND, so the
        // parent's WM_MOUSEMOVE alone cannot reveal a hidden bar. Inspecting
        // the thread queue keeps auto-hide working without timers or hooks.
        if (message.message == WM_MOUSEMOVE && hwnd_ != nullptr) {
            POINT point = message.pt;
            if (ScreenToClient(hwnd_, &point)) {
                if (point.y <= kRevealZone) {
                    revealBar();
                } else if (point.y > kBarHeight + kKeepVisibleZone) {
                    maybeAutoHideBar(point.y);
                }
            }
        }
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

void PlatformWindow::openSearch() {
    openOverlay();
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
        for (HWND button : {searchButton_, backButton_, forwardButton_,
                            reloadButton_, fullscreenButton_, translateButton_,
                            directionButton_, themeButton_, overlayMode_,
                            overlayEngine_, overlayClose_}) {
            if (button != nullptr) {
                InvalidateRect(button, nullptr, TRUE);
            }
        }
    }
}

void PlatformWindow::setHistory(std::vector<std::wstring> history) {
    history_ = std::move(history);
    historyIndex_ = -1;
    historyDraft_.clear();
    refreshHistoryList();
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
    if (directionButton_ != nullptr) {
        SetWindowTextW(directionButton_, translateToJapanese_ ? L"JA" : L"EN");
    }
    if (overlayMode_ != nullptr) {
        SetWindowTextW(overlayMode_, searchMode_ ? kGlyphSearch : kGlyphGlobe);
    }
    if (overlayEngine_ != nullptr) {
        SetWindowTextW(overlayEngine_, engineGlyph(engine_).c_str());
    }
    if (overlayEdit_ != nullptr) {
        SendMessageW(overlayEdit_, EM_SETCUEBANNER, TRUE,
                     reinterpret_cast<LPARAM>(searchMode_
                         ? L"検索語を入力 (Enter で検索)"
                         : L"URL を入力 (http / https)"));
    }
    for (HWND button : {overlayMode_, overlayEngine_, directionButton_}) {
        if (button != nullptr) {
            InvalidateRect(button, nullptr, TRUE);
        }
    }
}

void PlatformWindow::refreshHistoryList() {
    if (overlayList_ == nullptr) {
        return;
    }
    SendMessageW(overlayList_, LB_RESETCONTENT, 0, 0);
    for (const std::wstring& entry : history_) {
        SendMessageW(overlayList_, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(entry.c_str()));
    }
}

void PlatformWindow::selectHistoryEntry() {
    if (overlayList_ == nullptr || overlayEdit_ == nullptr) {
        return;
    }
    const int selection = static_cast<int>(
        SendMessageW(overlayList_, LB_GETCURSEL, 0, 0));
    if (selection == LB_ERR) {
        return;
    }
    const int length = static_cast<int>(
        SendMessageW(overlayList_, LB_GETTEXTLEN, selection, 0));
    if (length == LB_ERR || length <= 0) {
        return;
    }
    std::wstring value(static_cast<size_t>(length), L'\0');
    SendMessageW(overlayList_, LB_GETTEXT, selection,
                 reinterpret_cast<LPARAM>(value.data()));
    SetWindowTextW(overlayEdit_, value.c_str());
    SendMessageW(overlayEdit_, EM_SETSEL, length, length);
}

void PlatformWindow::browseHistory(bool older) {
    if (overlayEdit_ == nullptr || history_.empty()) {
        return;
    }
    if (older) {
        if (historyIndex_ < 0) {
            historyDraft_ = editText(overlayEdit_);
            historyIndex_ = 0;
        } else if (historyIndex_ + 1 < static_cast<int>(history_.size())) {
            ++historyIndex_;
        } else {
            return;
        }
    } else {
        if (historyIndex_ <= 0) {
            historyIndex_ = -1;
            SetWindowTextW(overlayEdit_, historyDraft_.c_str());
        } else {
            --historyIndex_;
        }
    }
    const std::wstring& value =
        historyIndex_ >= 0 ? history_[static_cast<size_t>(historyIndex_)]
                           : historyDraft_;
    SetWindowTextW(overlayEdit_, value.c_str());
    const int length = GetWindowTextLengthW(overlayEdit_);
    SendMessageW(overlayEdit_, EM_SETSEL, length, length);
}

void PlatformWindow::handleFunctionKey(int virtualKey) {
    if (virtualKey == VK_F5) {
        if (onReload_) {
            onReload_();
        }
    } else if (virtualKey == VK_F11) {
        toggleFullscreen();
    }
}

void PlatformWindow::applyLayout() {
    if (hwnd_ == nullptr || webHost_ == nullptr) {
        return;
    }

    RECT client{};
    GetClientRect(hwnd_, &client);
    const int width = std::max(1, static_cast<int>(client.right));
    const int height = std::max(1, static_cast<int>(client.bottom));
    const int barTop = barVisible_ ? kBarHeight : 0;
    const int webHeight = std::max(1, height - barTop);

    if (barVisible_) {
        int x = 0;
        const struct BarButton { HWND hwnd; int width; } buttons[] = {
            {searchButton_, kSearchButtonWidth},
            {backButton_, kButtonWidth},
            {forwardButton_, kButtonWidth},
            {reloadButton_, kButtonWidth},
            {fullscreenButton_, kButtonWidth},
            {translateButton_, kButtonWidth},
            {directionButton_, kButtonWidth},
            {themeButton_, kButtonWidth},
        };
        for (const BarButton& button : buttons) {
            if (button.hwnd != nullptr) {
                SetWindowPos(button.hwnd, HWND_TOP, x, 0, button.width,
                             kBarHeight, SWP_NOACTIVATE);
            }
            x += button.width;
        }
    }

    // Move/resize the WebView host and notify the engine only when the bounds
    // really change (bar visibility toggle or window resize).
    if (width != lastHostWidth_ || barTop != lastHostTop_ ||
        webHeight != lastHostHeight_) {
        SetWindowPos(webHost_, HWND_BOTTOM, 0, barTop, width, webHeight,
                     SWP_NOACTIVATE);
        lastHostWidth_ = width;
        lastHostTop_ = barTop;
        lastHostHeight_ = webHeight;
        if (onResize_) {
            onResize_(width, webHeight);
        }
    }

    positionOverlay();
}

void PlatformWindow::positionOverlay() {
    if (!overlayCreated_ || hwnd_ == nullptr) {
        return;
    }
    RECT client{};
    GetClientRect(hwnd_, &client);
    const int width = std::max(1, static_cast<int>(client.right));
    const int overlayWidth = std::clamp(width - 2 * kOverlayMargin,
                                        kOverlayMinWidth, kOverlayMaxWidth);
    const int overlayHeight = kOverlayHeight;
    const int x = std::max(0, (width - overlayWidth) / 2);
    const int y = kBarHeight;

    SetWindowPos(overlayPanel_, HWND_TOP, x, y, overlayWidth, overlayHeight,
                 SWP_NOACTIVATE);

    int cx = x + kOverlayMargin;
    const int rowY = y + kOverlayMargin;
    SetWindowPos(overlayMode_, HWND_TOP, cx, rowY, kOverlayIconWidth,
                 kOverlayRowHeight, SWP_NOACTIVATE);
    cx += kOverlayIconWidth + kOverlayGap;
    SetWindowPos(overlayEngine_, HWND_TOP, cx, rowY, kOverlayIconWidth,
                 kOverlayRowHeight, SWP_NOACTIVATE);
    cx += kOverlayIconWidth + kOverlayGap;

    const int closeX = x + overlayWidth - kOverlayMargin - kOverlayIconWidth;
    const int editWidth = std::max(1, closeX - kOverlayGap - cx);
    SetWindowPos(overlayEdit_, HWND_TOP, cx, rowY, editWidth,
                 kOverlayRowHeight, SWP_NOACTIVATE);
    SetWindowPos(overlayClose_, HWND_TOP, closeX, rowY, kOverlayIconWidth,
                 kOverlayRowHeight, SWP_NOACTIVATE);

    const int listY = rowY + kOverlayRowHeight + kOverlayGap;
    const int listHeight = std::max(
        1, overlayHeight - 2 * kOverlayMargin - kOverlayRowHeight - kOverlayGap);
    SetWindowPos(overlayList_, HWND_TOP, x + kOverlayMargin, listY,
                 overlayWidth - 2 * kOverlayMargin, listHeight,
                 SWP_NOACTIVATE);
}

void PlatformWindow::setBarVisible(bool visible) {
    if (barVisible_ == visible) {
        return;
    }
    barVisible_ = visible;
    for (HWND button : {searchButton_, backButton_, forwardButton_,
                        reloadButton_, fullscreenButton_, translateButton_,
                        directionButton_, themeButton_}) {
        if (button != nullptr) {
            ShowWindow(button, visible ? SW_SHOW : SW_HIDE);
        }
    }
    applyLayout();
}

void PlatformWindow::revealBar() {
    if (!barVisible_) {
        setBarVisible(true);
    }
}

void PlatformWindow::maybeAutoHideBar(int clientY) {
    if (!barVisible_ || overlayVisible_ || editFocused_ || hwnd_ == nullptr) {
        return;
    }
    if (clientY < 0) {
        POINT point{};
        if (!GetCursorPos(&point) || !ScreenToClient(hwnd_, &point)) {
            return;
        }
        clientY = point.y;
    }
    if (clientY > kBarHeight + kKeepVisibleZone) {
        setBarVisible(false);
    }
}

void PlatformWindow::ensureOverlay() {
    if (overlayCreated_ || hwnd_ == nullptr) {
        return;
    }
    overlayCreated_ = true;

    overlayPanel_ = CreateWindowExW(
        0, L"STATIC", nullptr,
        WS_CHILD | WS_CLIPSIBLINGS | SS_NOTIFY,
        0, 0, 0, 0, hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kOverlayPanelId)),
        instance_, nullptr);
    overlayEdit_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0, hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kOverlayEditId)),
        instance_, nullptr);
    overlayMode_ = createButton(instance_, hwnd_, kGlyphGlobe,
                                kOverlayModeId, false);
    overlayEngine_ = createButton(instance_, hwnd_, L"D",
                                  kOverlayEngineId, false);
    overlayClose_ = createButton(instance_, hwnd_, kGlyphClose,
                                 kOverlayCloseId, false);
    overlayList_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
        WS_CHILD | WS_TABSTOP | WS_VSCROLL | WS_CLIPSIBLINGS |
            LBS_NOTIFY | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
        0, 0, 0, 0, hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kOverlayListId)),
        instance_, nullptr);

    const HFONT font = uiFont_ != nullptr
                           ? uiFont_
                           : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    const HFONT icon = iconFont_ != nullptr ? iconFont_ : font;
    SendMessageW(overlayEdit_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(overlayList_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(overlayMode_, WM_SETFONT, reinterpret_cast<WPARAM>(icon), TRUE);
    SendMessageW(overlayEngine_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(overlayClose_, WM_SETFONT, reinterpret_cast<WPARAM>(icon), TRUE);
    SendMessageW(overlayEdit_, EM_SETMARGINS, EC_LEFTMARGIN, MAKELPARAM(6, 0));

    originalEditProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        overlayEdit_, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&PlatformWindow::editProc)));
    originalListProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        overlayList_, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&PlatformWindow::listProc)));

    refreshHistoryList();
    refreshButtonText();
    positionOverlay();
}

void PlatformWindow::showOverlayControls(bool visible) {
    const int command = visible ? SW_SHOW : SW_HIDE;
    for (HWND control : {overlayPanel_, overlayEdit_, overlayMode_,
                         overlayEngine_, overlayClose_, overlayList_}) {
        if (control != nullptr) {
            ShowWindow(control, command);
        }
    }
}

void PlatformWindow::openOverlay() {
    if (hwnd_ == nullptr) {
        return;
    }
    ensureOverlay();
    revealBar();
    overlayVisible_ = true;
    showOverlayControls(true);
    refreshHistoryList();
    positionOverlay();
    SetFocus(overlayEdit_);
    SendMessageW(overlayEdit_, EM_SETSEL, 0, -1);
}

void PlatformWindow::closeOverlay(bool focusWeb) {
    if (!overlayVisible_) {
        return;
    }
    overlayVisible_ = false;
    editFocused_ = false;
    historyIndex_ = -1;
    historyDraft_.clear();
    showOverlayControls(false);
    maybeAutoHideBar();
    if (focusWeb && onFocusWeb_) {
        onFocusWeb_();
    }
}

void PlatformWindow::submitUrl() {
    if (overlayEdit_ == nullptr) {
        return;
    }

    const std::wstring raw = trim(editText(overlayEdit_));
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
    closeOverlay(false);
    if (onNavigate_) {
        onNavigate_(url);
    }
}

bool PlatformWindow::isCtrlL(WPARAM virtualKey) {
    return virtualKey == 'L' && (GetKeyState(VK_CONTROL) & 0x8000) != 0;
}

bool PlatformWindow::handleControlKey(PlatformWindow* self, WPARAM virtualKey) {
    if (self == nullptr) {
        return false;
    }
    if (isCtrlL(virtualKey)) {
        self->openOverlay();
        return true;
    }
    if (virtualKey == VK_ESCAPE) {
        if (self->overlayVisible_) {
            self->closeOverlay(true);
        } else {
            self->setBarVisible(false);
        }
        return true;
    }
    if (virtualKey == VK_F5 || virtualKey == VK_F11) {
        self->handleFunctionKey(static_cast<int>(virtualKey));
        return true;
    }
    return false;
}

LRESULT CALLBACK PlatformWindow::editProc(HWND hwnd, UINT message,
                                          WPARAM wParam, LPARAM lParam) {
    if (g_activeWindow != nullptr) {
        if (message == WM_SETFOCUS) {
            g_activeWindow->editFocused_ = true;
            g_activeWindow->revealBar();
        } else if (message == WM_KILLFOCUS) {
            g_activeWindow->editFocused_ = false;
            g_activeWindow->maybeAutoHideBar();
        } else if (message == WM_KEYDOWN) {
            if (wParam == VK_RETURN) {
                g_activeWindow->submitUrl();
                return 0;
            }
            if (wParam == VK_UP || wParam == VK_DOWN) {
                g_activeWindow->browseHistory(wParam == VK_UP);
                return 0;
            }
            if (handleControlKey(g_activeWindow, wParam)) {
                return 0;
            }
        }
    }
    const WNDPROC original = g_activeWindow != nullptr
                                 ? g_activeWindow->originalEditProc_
                                 : nullptr;
    return CallWindowProcW(original != nullptr ? original : DefWindowProcW,
                           hwnd, message, wParam, lParam);
}

LRESULT CALLBACK PlatformWindow::listProc(HWND hwnd, UINT message,
                                          WPARAM wParam, LPARAM lParam) {
    if (g_activeWindow != nullptr && message == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            g_activeWindow->selectHistoryEntry();
            g_activeWindow->submitUrl();
            return 0;
        }
        if (handleControlKey(g_activeWindow, wParam)) {
            return 0;
        }
    }
    const WNDPROC original = g_activeWindow != nullptr
                                 ? g_activeWindow->originalListProc_
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
            self->searchButton_ = createButton(self->instance_, hwnd,
                                               kGlyphSearch,
                                               kSearchButtonId, true);
            self->backButton_ = createButton(self->instance_, hwnd,
                                             kGlyphBack, kBackButtonId, true);
            self->forwardButton_ = createButton(self->instance_, hwnd,
                                                kGlyphForward,
                                                kForwardButtonId, true);
            self->reloadButton_ = createButton(self->instance_, hwnd,
                                               kGlyphReload,
                                               kReloadButtonId, true);
            self->fullscreenButton_ = createButton(self->instance_, hwnd,
                                                   kGlyphFullscreen,
                                                   kFullscreenButtonId, true);
            self->translateButton_ = createButton(self->instance_, hwnd,
                                                  kGlyphTranslate,
                                                  kTranslateButtonId, true);
            self->directionButton_ = createButton(self->instance_, hwnd,
                                                  L"JA", kDirectionButtonId,
                                                  true);
            self->themeButton_ = createButton(self->instance_, hwnd,
                                              kGlyphTheme, kThemeButtonId,
                                              true);
            // A plain STATIC child is enough of a parent for WebView2.
            self->webHost_ = CreateWindowExW(
                0, L"STATIC", nullptr,
                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                0, 0, 0, 0, hwnd, nullptr, self->instance_, nullptr);

            // Segoe UI Variable for text, MDL2 Assets for the icon glyphs.
            self->uiFont_ = CreateFontW(
                -11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                L"Segoe UI Variable Text");
            self->iconFont_ = CreateFontW(
                -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                L"Segoe MDL2 Assets");
            const HFONT textFont = self->uiFont_ != nullptr
                                       ? self->uiFont_
                                       : static_cast<HFONT>(GetStockObject(
                                             DEFAULT_GUI_FONT));
            const HFONT glyphFont = self->iconFont_ != nullptr
                                        ? self->iconFont_
                                        : textFont;
            for (HWND control : {self->searchButton_, self->backButton_,
                                 self->forwardButton_, self->reloadButton_,
                                 self->fullscreenButton_,
                                 self->translateButton_, self->themeButton_}) {
                SendMessageW(control, WM_SETFONT,
                             reinterpret_cast<WPARAM>(glyphFont), TRUE);
            }
            SendMessageW(self->directionButton_, WM_SETFONT,
                         reinterpret_cast<WPARAM>(textFont), TRUE);

            self->refreshButtonText();
            self->setNavigationState(false, false);
            SetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND,
                             reinterpret_cast<LONG_PTR>(self->controlBrush()));
            self->applyLayout();
            SetFocus(self->webHost_);
            return 0;
        }

        case WM_SIZE:
            self->applyLayout();
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
            if (LOWORD(wParam) != WA_INACTIVE) {
                self->revealBar();
            }
            return 0;

        case WM_MOUSEMOVE: {
            const int y = signedHighWord(lParam);
            if (y <= kRevealZone) {
                self->revealBar();
            } else if (y > kBarHeight + kKeepVisibleZone) {
                self->maybeAutoHideBar(y);
            }
            break;
        }

        case WM_INPUT: {
            RAWINPUT raw{};
            UINT rawSize = sizeof(raw);
            const UINT copied = GetRawInputData(
                reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw,
                &rawSize, sizeof(RAWINPUTHEADER));
            if (copied == sizeof(raw) && raw.header.dwType == RIM_TYPEMOUSE &&
                GetForegroundWindow() == hwnd) {
                POINT point{};
                if (GetCursorPos(&point) && ScreenToClient(hwnd, &point)) {
                    if (point.y <= kRevealZone) {
                        self->revealBar();
                    } else if (point.y > kBarHeight + kKeepVisibleZone) {
                        self->maybeAutoHideBar(point.y);
                    }
                }
            }
            return 0;
        }

        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED) {
                switch (LOWORD(wParam)) {
                case kSearchButtonId:
                    self->openOverlay();
                    break;
                case kBackButtonId:
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
                case kTranslateButtonId:
                    if (self->onTranslate_) {
                        self->onTranslate_(self->translateToJapanese_);
                    }
                    break;
                case kDirectionButtonId:
                    self->translateToJapanese_ = !self->translateToJapanese_;
                    self->refreshButtonText();
                    break;
                case kThemeButtonId:
                    self->setTheme(!self->darkTheme_);
                    if (self->onThemeChanged_) {
                        self->onThemeChanged_(self->darkTheme_);
                    }
                    break;
                case kOverlayModeId:
                    self->searchMode_ = !self->searchMode_;
                    self->refreshButtonText();
                    break;
                case kOverlayEngineId: {
                    const int next = (static_cast<int>(self->engine_) + 1) % 3;
                    self->engine_ = static_cast<search::Engine>(next);
                    self->refreshButtonText();
                    if (self->onEngineChanged_) {
                        self->onEngineChanged_(self->engine_);
                    }
                    break;
                }
                case kOverlayCloseId:
                    self->closeOverlay(true);
                    break;
                default:
                    break;
                }
            } else if (HIWORD(wParam) == LBN_SELCHANGE &&
                       LOWORD(wParam) == kOverlayListId) {
                self->selectHistoryEntry();
            } else if (HIWORD(wParam) == LBN_DBLCLK &&
                       LOWORD(wParam) == kOverlayListId) {
                self->selectHistoryEntry();
                self->submitUrl();
            }
            return 0;

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORLISTBOX: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetTextColor(hdc, self->darkTheme_ ? RGB(238, 238, 238)
                                               : RGB(16, 16, 16));
            SetBkColor(hdc, self->darkTheme_ ? RGB(32, 32, 32)
                                             : RGB(255, 255, 255));
            return reinterpret_cast<LRESULT>(self->controlBrush());
        }

        case WM_DRAWITEM: {
            auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            wchar_t label[64]{};
            GetWindowTextW(draw->hwndItem, label,
                           static_cast<int>(std::size(label)));
            bool accent = false;
            COLORREF accentColor = RGB(88, 92, 110);
            switch (draw->CtlID) {
            case kSearchButtonId:
                accent = true;
                accentColor = RGB(0, 120, 215);
                break;
            case kTranslateButtonId:
                accent = true;
                accentColor = RGB(0, 145, 105);
                break;
            case kOverlayModeId:
                accent = true;
                accentColor = RGB(20, 115, 140);
                break;
            case kOverlayEngineId:
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
            const HFONT font = usesIconFont(draw->CtlID)
                                   ? (self->iconFont_ != nullptr
                                          ? self->iconFont_
                                          : self->uiFont_)
                                   : self->uiFont_;
            HFONT previous = font != nullptr
                                 ? static_cast<HFONT>(SelectObject(draw->hDC,
                                                                   font))
                                 : nullptr;
            DrawTextW(draw->hDC, label, -1, &draw->rcItem,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            if (previous != nullptr) {
                SelectObject(draw->hDC, previous);
            }
            return TRUE;
        }

        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize.x = 480;
            mmi->ptMinTrackSize.y = 240;
            return 0;
        }

        case WM_KEYDOWN:
            if (handleControlKey(self, wParam)) {
                return 0;
            }
            break;

        case WM_CHAR: {
            const bool altDown = (GetKeyState(VK_MENU) & 0x8000) != 0;
            const bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (!self->overlayVisible_ && !ctrlDown && !altDown &&
                wParam >= 0x20 && wParam <= 0x7e) {
                self->openOverlay();
                const wchar_t initial = static_cast<wchar_t>(wParam);
                SetWindowTextW(self->overlayEdit_, std::wstring(1, initial).c_str());
                SendMessageW(self->overlayEdit_, EM_SETSEL, 1, 1);
                return 0;
            }
            break;
        }

        case WM_DESTROY: {
            RAWINPUTDEVICE mouse{};
            mouse.usUsagePage = 1;
            mouse.usUsage = 2;
            mouse.dwFlags = RIDEV_REMOVE;
            mouse.hwndTarget = nullptr;
            RegisterRawInputDevices(&mouse, 1, sizeof(mouse));

            if (self->overlayEdit_ != nullptr &&
                self->originalEditProc_ != nullptr) {
                SetWindowLongPtrW(self->overlayEdit_, GWLP_WNDPROC,
                                  reinterpret_cast<LONG_PTR>(
                                      self->originalEditProc_));
                self->originalEditProc_ = nullptr;
            }
            if (self->overlayList_ != nullptr &&
                self->originalListProc_ != nullptr) {
                SetWindowLongPtrW(self->overlayList_, GWLP_WNDPROC,
                                  reinterpret_cast<LONG_PTR>(
                                      self->originalListProc_));
                self->originalListProc_ = nullptr;
            }
            if (self->uiFont_ != nullptr) {
                DeleteObject(self->uiFont_);
                self->uiFont_ = nullptr;
            }
            if (self->iconFont_ != nullptr) {
                DeleteObject(self->iconFont_);
                self->iconFont_ = nullptr;
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
        }

        default:
            break;
        }
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace astb3ast
