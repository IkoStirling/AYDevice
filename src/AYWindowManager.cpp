#include "AYDevice/WindowManager.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if defined(_WIN32)
#  include <mutex>
#endif

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <Windows.h>
#  include <windowsx.h>
#  include <imm.h>
#endif

#include <cstdio>
#include <cstdlib>

#if defined(AY_DEVICE_USE_SDL2)
#  include <SDL.h>
#  include <SDL_syswm.h>
#endif

namespace ayt::device {

namespace {

#if defined(_WIN32)
std::wstring utf8ToWide(const std::string& text)
{
    if (text.empty()) {
        return {};
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (needed <= 0) {
        return {};
    }
    std::wstring wide(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), needed);
    if (!wide.empty() && wide.back() == L'\0') {
        wide.pop_back();
    }
    return wide;
}

std::string wideToUtf8(const wchar_t* text, int wcharCount)
{
    if (text == nullptr || wcharCount <= 0) {
        return {};
    }
    const int needed = WideCharToMultiByte(CP_UTF8, 0, text, wcharCount,
                                            nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return {};
    }
    std::string utf8(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, wcharCount, utf8.data(), needed,
                        nullptr, nullptr);
    return utf8;
}

WindowManager* windowFromHwnd(HWND hwnd)
{
    return reinterpret_cast<WindowManager*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

// Forward-declared so D5's TopLevelWndProc thunk (defined further
// down in the same namespace) can read client size before the
// definition is encountered by the compiler.
void readClientSize(HWND hwnd, int& width, int& height);
#endif

} // namespace

#if defined(_WIN32)
// PR-InputTrace: opt-in stderr trace for raw-input registration +
// WM_INPUT / WM_POINTER / WM_GESTURE firing diagnostics. Used to
// identify which Windows message path actually delivers wheel events
// for a given trackpad/driver combination (Gallery S1/S2 2026-08-07).
// Set AY_DEVICE_TRACE_INPUT=1 before launching AYUI_Gallery.
namespace {
bool ayDeviceTraceInputEnabled() {
    static int cached = -1;
    if (cached < 0) {
        const char* env = std::getenv("AY_DEVICE_TRACE_INPUT");
        cached = (env != nullptr && env[0] != '\0' && env[0] != '0') ? 1 : 0;
    }
    return cached != 0;
}
}   // namespace
#endif

struct WindowManager::Impl {
    int width = 0;
    int height = 0;
    bool resizable = true;
    WindowCloseCallback onClose;
    WindowResizeCallback onResize;
    WindowFocusCallback onFocus;
    WindowMessageCallback onMessage;
    DeviceInputEventCallback onInputEvent;
    KeyCallback onKey;
    MouseButtonCallback onMouseButton;
    MouseMoveCallback onMouseMove;
    MouseDeltaCallback onMouseDelta;
    MouseWheelCallback onMouseWheel;
    std::function<void()> onInputReset;
    TouchCallback onTouch;
    CharCallback onChar;
    CompositionCallback onComposition;
    bool touchEnabled = false;
    bool textInputEnabled = false;
    bool compositionActive = false;
    bool focused = false;
    bool relativeMouseRequested = false;
    bool relativeMouseActive = false;
    SystemCursorShape cursorShape = SystemCursorShape::Arrow;

#if defined(_WIN32)
    HWND hwnd = nullptr;
    HINSTANCE instance = GetModuleHandleW(nullptr);
    std::vector<HWND> childWindows;
    std::vector<HWND> topLevelWindows;       // D5 — owns HWNDs created by createTopLevelWindow

    bool touchRegistered = false;
    // PR-InputTrace: precision trackpad / MagicMouse wheel goes through
    // WM_INPUT (raw input) instead of WM_MOUSEWHEEL. Registered once per
    // window in createWindow; unregistered in destroyWindow.
    bool rawInputRegistered = false;
    int cursorHideAdjustments = 0;
    unsigned pressedMouseButtons = 0;
    bool mouseLeaveTracking = false;

    // PR-InputTrace: per-message-class trigger counters. Diagnostic only;
    // logged once at first trigger and every 60 thereafter to avoid
    // stderr spam during sustained trackpad gestures.
    int rawInputTriggerCount = 0;
    int pointerTriggerCount = 0;
    int gestureTriggerCount = 0;
    int mouseWheelTriggerCount = 0;

    // L2 (2026-08-26): per-pointer trackpad position memory. Each
    // WM_POINTERUPDATE for a PT_TOUCHPAD pointer stores its last
    // pixel location; the next fire computes the delta. The map
    // grows on first contact and shrinks on PT_POINTER-leave; we
    // don't explicitly prune because pointer ids recycle rarely and
    // the worst case is a few stale entries.
    std::unordered_map<UINT32, long> trackpadLastX;
    std::unordered_map<UINT32, long> trackpadLastY;
    std::unordered_set<UINT32>       trackpadHasLast;

    // WM_CHAR delivers UTF-16; a leading high surrogate is held until its pair.
    wchar_t pendingHighSurrogate = 0;
#endif

#if defined(AY_DEVICE_USE_SDL2)
    SDL_Window* sdlWindow = nullptr;
#endif

    bool valid = false;

    // Set by notifyClosed() so a poll-style observer (DeviceSubSystem) can
    // detect a close request during update() and forward it to the EventBus
    // without re-entering the onClose callback. Consumed by
    // consumeCloseRequested() — single-shot, idempotent.
    bool closeRequested = false;
};

#if defined(_WIN32)
namespace {

HCURSOR nativeCursor(SystemCursorShape shape)
{
    LPCSTR resource = IDC_ARROW;
    switch (shape) {
    case SystemCursorShape::Hand:           resource = IDC_HAND; break;
    case SystemCursorShape::Text:           resource = IDC_IBEAM; break;
    case SystemCursorShape::SizeHorizontal: resource = IDC_SIZEWE; break;
    case SystemCursorShape::SizeVertical:   resource = IDC_SIZENS; break;
    case SystemCursorShape::SizeNwse:       resource = IDC_SIZENWSE; break;
    case SystemCursorShape::SizeNesw:       resource = IDC_SIZENESW; break;
    case SystemCursorShape::Move:           resource = IDC_SIZEALL; break;
    case SystemCursorShape::Arrow:          resource = IDC_ARROW; break;
    }
    return ::LoadCursorA(nullptr, resource);
}

const wchar_t* kMainWindowClass = L"AYDeviceMainWindow";
const wchar_t* kChildWindowClass = L"AYDeviceChildSurface";

bool registerMainWindowClass(HINSTANCE instance)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kMainWindowClass;
    wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
        WindowManager* owner = windowFromHwnd(hwnd);
        if (owner != nullptr) {
            bool handled = false;
            const LRESULT userResult = static_cast<LRESULT>(
                owner->tryHandleUserMessage(msg, wParam, lParam, handled));
            if (handled) {
                return userResult;
            }
            owner->processPlatformEvent(msg, wParam, lParam);
            if (msg == WM_SETCURSOR && LOWORD(lParam) == HTCLIENT
                && owner->applyCursor()) {
                return TRUE;
            }
        }
        if (msg == WM_CLOSE || msg == WM_DESTROY) {
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    };

    return RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

bool registerChildWindowClass(HINSTANCE instance)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kChildWindowClass;
    wc.lpfnWndProc = DefWindowProcW;

    return RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

// D5 — top-level (WS_OVERLAPPEDWINDOW) window registration. WndProc MUST be
// a free-static thunk (K-INV-D5-5) because Win32 class registration
// requires a `__stdcall` function pointer with C linkage — we can't store
// a lambda capturing `this`. The thunk dispatches via the
// `s_topLevelOwners` map (HWND → owning WindowManager).
const wchar_t* kTopLevelWindowClass = L"AYDeviceTopLevelWindow";

// File-static registry. Lifetime: process-wide. Protected by a single
// `std::mutex` because window messages can arrive on the thread that
// pumps Win32 messages; the test runner does this from a single thread
// in D5 v1, but the mutex keeps the door open for the Win32
// GetMessage/DispatchMessage loop without UAF.
std::unordered_map<HWND, WindowManager*> s_topLevelOwners;
std::unordered_map<HWND, TopLevelWindowCallbacks> s_topLevelCallbacks;
// borderless+resizable: need custom WM_NCHITTEST (thick-frame alone is
// not enough once the frame is absorbed into the client area).
std::unordered_map<HWND, bool> s_topLevelBorderlessResizable;
std::mutex s_topLevelMu;

// TopLevelWndProc sits before the anonymous namespace holding
// translateVirtualKey (defined ~90 lines below); declare it so the
// keyboard routing in TopLevelWndProc can use it.
KeyCode translateVirtualKey(WPARAM vk, LPARAM lParam);

// Shared WM_CHAR decoding: UTF-16 unit stream → combined surrogate
// pairs → UTF-8. The main window (processPlatformEvent) keeps its
// pairing state in Impl::pendingHighSurrogate; each top-level child
// window uses its own file-static below so interleaved input from
// multiple windows can't cross-pair.
void handleWmChar(wchar_t unit,
                  const std::function<void(const char*, int)>& onChar,
                  wchar_t& pendingHigh)
{
    if (unit >= 0xD800 && unit <= 0xDBFF) {
        pendingHigh = unit;  // wait for low surrogate
    } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
        if (pendingHigh != 0) {
            const wchar_t pair[2] = {pendingHigh, unit};
            const std::string utf8 = wideToUtf8(pair, 2);
            pendingHigh = 0;
            if (!utf8.empty()) {
                onChar(utf8.c_str(), static_cast<int>(utf8.size()));
            }
        }
    } else if (unit >= 0x20 || unit == L'\t' || unit == L'\n' || unit == L'\r') {
        // Skip other control chars (backspace/escape stay on the key path).
        const std::string utf8 = wideToUtf8(&unit, 1);
        if (!utf8.empty()) {
            onChar(utf8.c_str(), static_cast<int>(utf8.size()));
        }
    }
}

// Per-top-level-window WM_CHAR pairing state (see handleWmChar).
//
// L1 (2026-08-26): was a single wchar_t; cross-paired surrogates if two
// top-level windows received interleaved WM_CHAR (e.g. window A's high
// surrogate + window B's low surrogate → invalid glyph). Per-HWND map
// keyed under s_topLevelMu so dispatch order is consistent with the
// callback map. Entries are inserted in WM_NCCREATE and erased in
// destroyTopLevelWindow / destroyAllTopLevelWindows.
std::unordered_map<HWND, wchar_t> s_topLevelPendingHigh;

LRESULT CALLBACK TopLevelWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // WM_NCCREATE is delivered BEFORE WM_CREATE; we get the owner pointer
    // through a side channel (the WM_NCCREATE lpCreateParams holds the
    // lpParam we passed into CreateWindowExW). Subsequent messages look it
    // up from s_topLevelOwners (set after CreateWindowExW returns).
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* owner = reinterpret_cast<WindowManager*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(owner));
        std::lock_guard<std::mutex> g(s_topLevelMu);
        s_topLevelOwners[hwnd] = owner;
        // L1 (2026-08-26): per-HWND surrogate state, inserted at create
        // so subsequent WM_CHAR lookups never miss a brand-new window.
        s_topLevelPendingHigh[hwnd] = 0;
        return TRUE;
    }

    WindowManager* owner = windowFromHwnd(hwnd);
    if (owner == nullptr) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    // Copy callbacks under the lock; invoke outside it (a callback may
    // mutate s_topLevelCallbacks, e.g. onResize re-wiring the set).
    TopLevelWindowCallbacks cbs;
    {
        std::lock_guard<std::mutex> g(s_topLevelMu);
        auto it = s_topLevelCallbacks.find(hwnd);
        if (it != s_topLevelCallbacks.end()) {
            cbs = it->second;
        }
    }

    switch (msg) {
    case WM_NCHITTEST: {
        bool borderlessResizable = false;
        {
            std::lock_guard<std::mutex> g(s_topLevelMu);
            auto it = s_topLevelBorderlessResizable.find(hwnd);
            borderlessResizable =
                (it != s_topLevelBorderlessResizable.end() && it->second);
        }
        if (!borderlessResizable) {
            break;
        }
        // Custom edge/corner hit-test: WS_POPUP|WS_THICKFRAME still needs
        // this because the visible chrome is entirely client-painted.
        POINT pt{static_cast<LONG>(static_cast<short>(LOWORD(lParam))),
                 static_cast<LONG>(static_cast<short>(HIWORD(lParam)))};
        RECT rc{};
        ::GetWindowRect(hwnd, &rc);
        constexpr int kBand = 8;
        constexpr int kTopBtnStrip = 48; // maximize + close chrome
        const bool onLeft   = pt.x >= rc.left && pt.x < rc.left + kBand;
        const bool onRight  = pt.x < rc.right && pt.x >= rc.right - kBand;
        const bool onTop    = pt.y >= rc.top && pt.y < rc.top + kBand;
        const bool onBottom = pt.y < rc.bottom && pt.y >= rc.bottom - kBand;
        // Keep title-bar host buttons (口 / x) as client hits.
        if (onTop && pt.x >= rc.right - kTopBtnStrip) {
            return HTCLIENT;
        }
        if (onTop && onLeft)     return HTTOPLEFT;
        if (onTop && onRight)    return HTTOPRIGHT;
        if (onBottom && onLeft)  return HTBOTTOMLEFT;
        if (onBottom && onRight) return HTBOTTOMRIGHT;
        if (onLeft)              return HTLEFT;
        if (onRight)             return HTRIGHT;
        if (onTop)               return HTTOP;
        if (onBottom)            return HTBOTTOM;
        return HTCLIENT;
    }
    case WM_SIZE: {
        if (cbs.onResize) {
            int w = 0, h = 0;
            readClientSize(hwnd, w, h);
            cbs.onResize(w, h);
        }
        return 0;
    }
    case WM_CLOSE:
        if (cbs.onCloseRequested) {
            cbs.onCloseRequested();
            // Suppress default destruction — the host decides whether
            // to DestroyWindow via closeChildWindow. If the host
            // doesn't call it, the window will simply close through
            // WM_CLOSE re-entry or be cleaned up by the WindowManager
            // dtor (destroyAllTopLevelWindows).
            return 0;
        }
        // No callback registered — fall through to DefWindowProc which
        // will trigger DestroyWindow.
        break;

    case WM_SETFOCUS:
        if (cbs.onFocusChanged) {
            cbs.onFocusChanged(true);
        }
        return 0;
    case WM_KILLFOCUS:
        if (cbs.onFocusChanged) {
            cbs.onFocusChanged(false);
        }
        return 0;

    // ===== PR-Dock-TearOff input routing =====
    // All mouse coordinates below are client-relative floats, matching
    // the main-window callback contract. The host (editor child-window
    // manager) forwards them into its own UIManager.

    case WM_MOUSEMOVE:
        if (cbs.onMouseMove) {
            const int x = static_cast<short>(LOWORD(lParam));
            const int y = static_cast<short>(HIWORD(lParam));
            cbs.onMouseMove(static_cast<float>(x), static_cast<float>(y));
        }
        // (Re)arm the leave-notify so WM_MOUSELEAVE fires exactly once
        // when the cursor exits. Idempotent; cheap enough per move.
        {
            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            ::TrackMouseEvent(&tme);
        }
        return 0;
    case WM_MOUSELEAVE:
        if (cbs.onMouseLeave) {
            cbs.onMouseLeave();
        }
        return 0;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN: case WM_LBUTTONUP: case WM_RBUTTONUP:
    case WM_MBUTTONUP: case WM_XBUTTONUP: {
        if (cbs.onMouseButton) {
            const int x = static_cast<short>(LOWORD(lParam));
            const int y = static_cast<short>(HIWORD(lParam));
            // Do NOT use (msg & 1): WM_RBUTTONDOWN=0x0204 is even, so that
            // trick inverted right/middle press state and broke child-window
            // button routing after dock tear-off.
            const bool pressed =
                (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN ||
                 msg == WM_MBUTTONDOWN || msg == WM_XBUTTONDOWN);
            int button = 0;                     // 0=left, 1=right, 2=middle
            switch (msg) {
            case WM_RBUTTONDOWN: case WM_RBUTTONUP:   button = 1; break;
            case WM_MBUTTONDOWN: case WM_MBUTTONUP:   button = 2; break;
            case WM_XBUTTONDOWN: case WM_XBUTTONUP:
                button = (HIWORD(wParam) == XBUTTON1) ? 3 : 4;
                break;
            default: break;
            }
            const bool wantCapture =
                cbs.onMouseButton(static_cast<float>(x), static_cast<float>(y),
                                  button, pressed);
            if (pressed && wantCapture) {
                ::SetCapture(hwnd);
            } else if (!pressed) {
                if (::GetCapture() == hwnd) {
                    ::ReleaseCapture();
                }
            }
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        if (cbs.onMouseWheel) {
            POINT pt{static_cast<LONG>(LOWORD(lParam)),
                     static_cast<LONG>(HIWORD(lParam))};
            ::ScreenToClient(hwnd, &pt);  // lParam is screen coords
            const short raw = static_cast<short>(HIWORD(wParam));
            cbs.onMouseWheel(static_cast<float>(pt.x), static_cast<float>(pt.y),
                             static_cast<float>(raw) / static_cast<float>(WHEEL_DELTA));
        }
        return 0;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (cbs.onKey) {
            const bool repeat = (lParam & (1 << 30)) != 0;  // bit 30 = previous key state
            if (!repeat) {
                cbs.onKey(translateVirtualKey(wParam, lParam), true);
            }
        }
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (cbs.onKey) {
            cbs.onKey(translateVirtualKey(wParam, lParam), false);
        }
        return 0;
    case WM_CHAR:
        if (cbs.onChar) {
            // L1 (2026-08-26): per-HWND surrogate state. Read/write the
            // pairing slot under s_topLevelMu so it stays in lock-step
            // with the callback map.
            wchar_t& pending = s_topLevelPendingHigh[hwnd];
            handleWmChar(static_cast<wchar_t>(wParam), cbs.onChar, pending);
        }
        return 0;
    case WM_ERASEBKGND:
        // GDI render backends repaint the full client surface every
        // frame; let them, instead of letting the class background
        // brush flash between frames (flicker).
        return TRUE;
    case WM_PAINT: {
        // Validate without filling. DefWindowProc + class brush would
        // flash COLOR_WINDOW between our GetDC/BitBlt frames and make
        // promoted dock windows look low-FPS / strobing.
        PAINTSTRUCT ps{};
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            if (cbs.cursorShape) {
                ::SetCursor(nativeCursor(cbs.cursorShape()));
                return TRUE;
            }
            if (cbs.onSetCursor && cbs.onSetCursor()) {
                return TRUE;
            }
        }
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool registerTopLevelWindowClass(HINSTANCE instance)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    // No class background — host GDI backends own the client pixels.
    // A COLOR_WINDOW brush raced WM_PAINT against per-frame GetDC paint.
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kTopLevelWindowClass;
    wc.lpfnWndProc = TopLevelWndProc;

    return RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

void readClientSize(HWND hwnd, int& width, int& height)
{
    RECT rect{};
    GetClientRect(hwnd, &rect);
    width  = rect.right - rect.left;
    height = rect.bottom - rect.top;
}

// Translate a Win32 virtual-key code (with WM_KEYDOWN lParam for extended-key
// and scancode disambiguation) into a backend-agnostic KeyCode.
KeyCode translateVirtualKey(WPARAM vk, LPARAM lParam)
{
    const bool extended = (lParam & (1 << 24)) != 0;
    const UINT scancode = static_cast<UINT>((lParam >> 16) & 0xFF);

    // Letters A-Z share ASCII codes with virtual keys.
    if (vk >= 'A' && vk <= 'Z') {
        return static_cast<KeyCode>(static_cast<int>(KeyCode::A) + (vk - 'A'));
    }
    // Top-row digits 0-9.
    if (vk >= '0' && vk <= '9') {
        return static_cast<KeyCode>(static_cast<int>(KeyCode::Num0) + (vk - '0'));
    }
    // Function keys F1-F12.
    if (vk >= VK_F1 && vk <= VK_F12) {
        return static_cast<KeyCode>(static_cast<int>(KeyCode::F1) + (vk - VK_F1));
    }
    // Keypad digits.
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return static_cast<KeyCode>(static_cast<int>(KeyCode::Kp0) + (vk - VK_NUMPAD0));
    }

    switch (vk) {
    case VK_ESCAPE:    return KeyCode::Escape;
    case VK_RETURN:    return extended ? KeyCode::KpEnter : KeyCode::Enter;
    case VK_TAB:       return KeyCode::Tab;
    case VK_SPACE:     return KeyCode::Space;
    case VK_BACK:      return KeyCode::Backspace;
    case VK_INSERT:    return KeyCode::Insert;
    case VK_DELETE:    return KeyCode::Delete;
    case VK_HOME:      return KeyCode::Home;
    case VK_END:       return KeyCode::End;
    case VK_PRIOR:     return KeyCode::PageUp;
    case VK_NEXT:      return KeyCode::PageDown;

    case VK_LEFT:      return KeyCode::Left;
    case VK_RIGHT:     return KeyCode::Right;
    case VK_UP:        return KeyCode::Up;
    case VK_DOWN:      return KeyCode::Down;

    case VK_SHIFT: {
        // Resolve left/right via scancode.
        const UINT mapped = MapVirtualKeyW(scancode, MAPVK_VSC_TO_VK_EX);
        return mapped == VK_RSHIFT ? KeyCode::RightShift : KeyCode::LeftShift;
    }
    case VK_LSHIFT:    return KeyCode::LeftShift;
    case VK_RSHIFT:    return KeyCode::RightShift;
    case VK_CONTROL:   return extended ? KeyCode::RightControl : KeyCode::LeftControl;
    case VK_LCONTROL:  return KeyCode::LeftControl;
    case VK_RCONTROL:  return KeyCode::RightControl;
    case VK_MENU:      return extended ? KeyCode::RightAlt : KeyCode::LeftAlt;
    case VK_LMENU:     return KeyCode::LeftAlt;
    case VK_RMENU:     return KeyCode::RightAlt;
    case VK_LWIN:      return KeyCode::LeftSuper;
    case VK_RWIN:      return KeyCode::RightSuper;

    case VK_OEM_MINUS:  return KeyCode::Minus;
    case VK_OEM_PLUS:   return KeyCode::Equal;
    case VK_OEM_4:      return KeyCode::LeftBracket;
    case VK_OEM_6:      return KeyCode::RightBracket;
    case VK_OEM_5:      return KeyCode::Backslash;
    case VK_OEM_1:      return KeyCode::Semicolon;
    case VK_OEM_7:      return KeyCode::Apostrophe;
    case VK_OEM_COMMA:  return KeyCode::Comma;
    case VK_OEM_PERIOD: return KeyCode::Period;
    case VK_OEM_2:      return KeyCode::Slash;
    case VK_OEM_3:      return KeyCode::GraveAccent;

    case VK_DECIMAL:    return KeyCode::KpDecimal;
    case VK_DIVIDE:     return KeyCode::KpDivide;
    case VK_MULTIPLY:   return KeyCode::KpMultiply;
    case VK_SUBTRACT:   return KeyCode::KpSubtract;
    case VK_ADD:        return KeyCode::KpAdd;

    case VK_CAPITAL:    return KeyCode::CapsLock;
    case VK_NUMLOCK:    return KeyCode::NumLock;
    case VK_SCROLL:     return KeyCode::ScrollLock;

    default:            return KeyCode::Unknown;
    }
}

} // namespace
#endif

WindowManager::WindowManager() : _impl(std::make_unique<Impl>())
{
}

WindowManager::~WindowManager()
{
    destroyWindow();
}

bool WindowManager::createWindow(const WindowCreateInfo& info)
{
    if (_impl->valid) {
        return true;
    }

#if defined(AY_DEVICE_USE_SDL2)
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return false;
    }

    Uint32 flags = SDL_WINDOW_SHOWN;
    if (info.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    if (info.hidden) {
        flags &= ~SDL_WINDOW_SHOWN;
        flags |= SDL_WINDOW_HIDDEN;
    }
    if (info.fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }

    _impl->sdlWindow = SDL_CreateWindow(
        info.title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        info.width,
        info.height,
        flags);

    if (_impl->sdlWindow == nullptr) {
        return false;
    }

    _impl->width  = info.width;
    _impl->height = info.height;
    _impl->resizable = info.resizable;
    _impl->valid = true;
    _impl->focused = (SDL_GetWindowFlags(_impl->sdlWindow) & SDL_WINDOW_INPUT_FOCUS) != 0;
    if (_impl->textInputEnabled) {
        SDL_StartTextInput();
    } else {
        SDL_StopTextInput();
    }
    updateRelativeMouseState();
    return true;

#elif defined(_WIN32)
    if (!registerMainWindowClass(_impl->instance)) {
        return false;
    }

    DWORD style = WS_OVERLAPPEDWINDOW;
    if (!info.resizable) {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }

    RECT rect{0, 0, info.width, info.height};
    AdjustWindowRect(&rect, style, FALSE);

    const std::wstring title = utf8ToWide(info.title);
    HWND hwnd = CreateWindowExW(
        0,
        kMainWindowClass,
        title.c_str(),
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        _impl->instance,
        nullptr);

    if (hwnd == nullptr) {
        return false;
    }

    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    _impl->hwnd = hwnd;
    readClientSize(hwnd, _impl->width, _impl->height);
    _impl->resizable = info.resizable;

    // Apply a touch-enable request made before the window existed.
    if (_impl->touchEnabled && !_impl->touchRegistered) {
        if (RegisterTouchWindow(hwnd, 0)) {
            _impl->touchRegistered = true;
        }
    }

    // PR-InputTrace: register raw-input devices for the window. Without
    // this, Windows precision trackpad / MagicMouse wheel events never
    // reach the message loop as WM_MOUSEWHEEL — they come through as
    // WM_INPUT carrying fractional RAWMOUSE::usButtonData. RIDEV_INPUTSINK
    // lets background windows still receive wheel events (Editor multi-
    // window scenario). usagePage=0x01 / usage=0x02 = generic desktop
    // mouse (the precision trackpad enumerates as a mouse-class device).
    if (!_impl->rawInputRegistered) {
        RAWINPUTDEVICE rid{};
        rid.usUsagePage = 0x01;
        rid.usUsage     = 0x02;
        rid.dwFlags     = RIDEV_INPUTSINK;
        rid.hwndTarget  = hwnd;
        const BOOL regOk = ::RegisterRawInputDevices(&rid, 1, sizeof(rid));
        if (regOk) {
            _impl->rawInputRegistered = true;
        }
        if (ayDeviceTraceInputEnabled()) {
            std::fprintf(stderr,
                "[AYDevice-InputTrace] RegisterRawInputDevices: ok=%d hwnd=%p err=%lu\n",
                regOk ? 1 : 0,
                static_cast<void*>(hwnd),
                regOk ? 0UL : static_cast<unsigned long>(::GetLastError()));
        }
    }

    if (info.hidden) {
        ShowWindow(hwnd, SW_HIDE);
        // L5 (2026-08-26): do NOT call UpdateWindow on a hidden window.
        // UpdateWindow sends WM_PAINT synchronously; for a hidden window
        // the surface is irrelevant and the call costs CPU. The visible
        // branch needs UpdateWindow to validate the first frame so the
        // editor doesn't flash a class background brush before the
        // renderer binds.
    } else {
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }

    _impl->valid = true;
    _impl->focused = (::GetFocus() == hwnd);
    updateRelativeMouseState();
    return true;

#else
    (void)info;
    return false;
#endif
}

void WindowManager::destroyWindow()
{
    if (!_impl) {
        return;
    }

    destroyAllChildWindows();
    destroyAllTopLevelWindows();

    _impl->relativeMouseRequested = false;
    updateRelativeMouseState();

#if defined(AY_DEVICE_USE_SDL2)
    if (_impl->sdlWindow != nullptr) {
        SDL_DestroyWindow(_impl->sdlWindow);
        _impl->sdlWindow = nullptr;
    }
    if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
#elif defined(_WIN32)
    if (_impl->hwnd != nullptr) {
        if (_impl->touchRegistered) {
            UnregisterTouchWindow(_impl->hwnd);
            _impl->touchRegistered = false;
        }
        // PR-InputTrace: unregister raw-input devices bound to this hwnd.
        // Pass RIDEV_REMOVE with hwndTarget=NULL to drop the sink binding
        // cleanly before DestroyWindow invalidates the HWND.
        if (_impl->rawInputRegistered) {
            RAWINPUTDEVICE rid{};
            rid.usUsagePage = 0x01;
            rid.usUsage     = 0x02;
            rid.dwFlags     = RIDEV_REMOVE;
            rid.hwndTarget  = nullptr;
            ::RegisterRawInputDevices(&rid, 1, sizeof(rid));
            _impl->rawInputRegistered = false;
        }
        DestroyWindow(_impl->hwnd);
        _impl->hwnd = nullptr;
    }
    _impl->pendingHighSurrogate = 0;
#endif

    _impl->valid = false;
    _impl->mouseLeaveTracking = false;
    _impl->compositionActive = false;
}

bool WindowManager::isWindowValid() const
{
    return _impl && _impl->valid;
}

void* WindowManager::getWindowHandle() const
{
    if (!_impl || !_impl->valid) {
        return nullptr;
    }

#if defined(AY_DEVICE_USE_SDL2)
    if (_impl->sdlWindow == nullptr) {
        return nullptr;
    }
    SDL_SysWMinfo wmInfo{};
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(_impl->sdlWindow, &wmInfo)) {
        return nullptr;
    }
#  if defined(_WIN32)
    return wmInfo.info.win.window;
#  elif defined(__APPLE__)
    return wmInfo.info.cocoa.window;
#  else
    return reinterpret_cast<void*>(wmInfo.info.x11.window);
#  endif

#elif defined(_WIN32)
    return _impl->hwnd;

#else
    return nullptr;
#endif
}

int WindowManager::getWidth() const
{
    return _impl ? _impl->width : 0;
}

int WindowManager::getHeight() const
{
    return _impl ? _impl->height : 0;
}

void WindowManager::getSize(int& width, int& height) const
{
    width  = getWidth();
    height = getHeight();
}

void WindowManager::setTitle(const char* title)
{
    if (!_impl || !_impl->valid || title == nullptr) {
        return;
    }

#if defined(AY_DEVICE_USE_SDL2)
    if (_impl->sdlWindow != nullptr) {
        SDL_SetWindowTitle(_impl->sdlWindow, title);
    }
#elif defined(_WIN32)
    if (_impl->hwnd != nullptr) {
        const std::wstring wide = utf8ToWide(title);
        SetWindowTextW(_impl->hwnd, wide.c_str());
    }
#endif
}

void WindowManager::setSize(int width, int height)
{
    if (!_impl || !_impl->valid || width <= 0 || height <= 0) {
        return;
    }

#if defined(AY_DEVICE_USE_SDL2)
    if (_impl->sdlWindow != nullptr) {
        SDL_SetWindowSize(_impl->sdlWindow, width, height);
        _impl->width  = width;
        _impl->height = height;
    }
#elif defined(_WIN32)
    if (_impl->hwnd != nullptr) {
        RECT rect{0, 0, width, height};
        AdjustWindowRect(&rect, GetWindowLongW(_impl->hwnd, GWL_STYLE), FALSE);
        SetWindowPos(_impl->hwnd, nullptr, 0, 0,
                     rect.right - rect.left,
                     rect.bottom - rect.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        readClientSize(_impl->hwnd, _impl->width, _impl->height);
    }
#endif

    notifyResized(_impl->width, _impl->height);
}

void WindowManager::notifyClosed()
{
    if (!_impl) {
        return;
    }
    // Latch first so the close-requested flag is visible to a poll-style
    // observer before the callback chain unwinds (callbacks may signal
    // "shutdown in progress" via the same flag — see DeviceSubSystem::update).
    _impl->closeRequested = true;
    if (_impl->onClose) {
        _impl->onClose();
    }
}

bool WindowManager::consumeCloseRequested()
{
    if (!_impl) {
        return false;
    }
    const bool was = _impl->closeRequested;
    _impl->closeRequested = false;
    return was;
}

void WindowManager::notifyResized(int width, int height)
{
    if (!_impl) {
        return;
    }
    _impl->width  = width;
    _impl->height = height;
    if (_impl->onResize) {
        _impl->onResize(width, height);
    }
}

void WindowManager::notifyFocused(bool focused)
{
    if (!_impl) {
        return;
    }
    _impl->focused = focused;
#if defined(_WIN32) && !defined(AY_DEVICE_USE_SDL2)
    if (!focused) {
        _impl->pressedMouseButtons = 0;
        if (_impl->hwnd != nullptr && ::GetCapture() == _impl->hwnd) {
            ::ReleaseCapture();
        }
    }
#endif
    updateRelativeMouseState();
    if (!focused && _impl->onInputReset) {
        _impl->onInputReset();
    }
    if (_impl->onFocus) {
        _impl->onFocus(focused);
    }
}

void WindowManager::setFullscreen(bool enabled)
{
    if (!_impl || !_impl->valid) {
        return;
    }

#if defined(AY_DEVICE_USE_SDL2)
    if (_impl->sdlWindow != nullptr) {
        SDL_SetWindowFullscreen(_impl->sdlWindow, enabled ? SDL_WINDOW_FULLSCREEN : 0);
    }
#elif defined(_WIN32)
    if (_impl->hwnd != nullptr) {
        ShowWindow(_impl->hwnd, enabled ? SW_MAXIMIZE : SW_RESTORE);
    }
#endif
}

void WindowManager::setResizable(bool resizable)
{
    if (_impl) {
        _impl->resizable = resizable;
    }
}

void WindowManager::setWindowCloseCallback(WindowCloseCallback callback)
{
    if (_impl) {
        _impl->onClose = std::move(callback);
    }
}

void WindowManager::setWindowResizeCallback(WindowResizeCallback callback)
{
    if (_impl) {
        _impl->onResize = std::move(callback);
    }
}

void WindowManager::setWindowFocusCallback(WindowFocusCallback callback)
{
    if (_impl) {
        _impl->onFocus = std::move(callback);
    }
}

void WindowManager::setWindowMessageCallback(WindowMessageCallback callback)
{
    if (_impl) {
        _impl->onMessage = std::move(callback);
    }
}

void WindowManager::setInputEventCallback(DeviceInputEventCallback callback)
{
    if (_impl) {
        _impl->onInputEvent = std::move(callback);
    }
}

void WindowManager::setKeyCallback(KeyCallback callback)
{
    if (_impl) {
        _impl->onKey = std::move(callback);
    }
}

void WindowManager::setMouseButtonCallback(MouseButtonCallback callback)
{
    if (_impl) {
        _impl->onMouseButton = std::move(callback);
    }
}

void WindowManager::setMouseMoveCallback(MouseMoveCallback callback)
{
    if (_impl) {
        _impl->onMouseMove = std::move(callback);
    }
}

void WindowManager::setMouseDeltaCallback(MouseDeltaCallback callback)
{
    if (_impl) {
        _impl->onMouseDelta = std::move(callback);
    }
}

void WindowManager::setMouseWheelCallback(MouseWheelCallback callback)
{
    if (_impl) {
        _impl->onMouseWheel = std::move(callback);
    }
}

void WindowManager::setTouchCallback(TouchCallback callback)
{
    if (_impl) {
        _impl->onTouch = std::move(callback);
    }
}

void WindowManager::setCharCallback(CharCallback callback)
{
    if (_impl) {
        _impl->onChar = std::move(callback);
    }
}

void WindowManager::setCompositionCallback(CompositionCallback callback)
{
    if (_impl) {
        _impl->onComposition = std::move(callback);
    }
}

void WindowManager::setTextInputEnabled(bool enabled)
{
    if (!_impl) {
        return;
    }
    _impl->textInputEnabled = enabled;
#if defined(AY_DEVICE_USE_SDL2)
    if (_impl->sdlWindow != nullptr) {
        if (enabled) {
            SDL_StartTextInput();
        } else {
            SDL_StopTextInput();
        }
    }
#endif
}

void WindowManager::setInputResetCallback(std::function<void()> callback)
{
    if (_impl) {
        _impl->onInputReset = std::move(callback);
    }
}

WindowManager::RelativeMouseResult WindowManager::setRelativeMouseMode(bool enabled)
{
#if defined(AY_DEVICE_USE_SDL2)
    if (!_impl || !_impl->valid || _impl->sdlWindow == nullptr) {
        return RelativeMouseResult::Busy;
    }
    _impl->relativeMouseRequested = enabled;
    updateRelativeMouseState();
    // L3 (2026-08-26): caller can now distinguish "disabled", "enabled
    // and active", and "request accepted but window unfocused (mode
    // suspended until focus returns)". All three are non-error states.
    if (!enabled) return RelativeMouseResult::Disabled;
    if (_impl->focused && _impl->relativeMouseActive) return RelativeMouseResult::Enabled;
    return RelativeMouseResult::Busy;
#elif defined(_WIN32)
    if (!_impl || !_impl->valid || _impl->hwnd == nullptr) {
        return RelativeMouseResult::Busy;
    }
    _impl->relativeMouseRequested = enabled;
    updateRelativeMouseState();
    if (!enabled) return RelativeMouseResult::Disabled;
    if (_impl->focused && _impl->relativeMouseActive) return RelativeMouseResult::Enabled;
    return RelativeMouseResult::Busy;
#else
    (void)enabled;
    return RelativeMouseResult::Busy;
#endif
}

bool WindowManager::isRelativeMouseMode() const
{
#if defined(AY_DEVICE_USE_SDL2) || defined(_WIN32)
    return _impl && _impl->relativeMouseRequested;
#else
    return false;
#endif
}

bool WindowManager::isFocused() const
{
    return _impl && _impl->focused;
}

void WindowManager::setCursorShape(SystemCursorShape shape)
{
    if (!_impl) {
        return;
    }
    _impl->cursorShape = shape;
    applyCursor();
}

SystemCursorShape WindowManager::cursorShape() const
{
    return _impl ? _impl->cursorShape : SystemCursorShape::Arrow;
}

bool WindowManager::applyCursor() const
{
    if (!_impl || !_impl->valid || _impl->relativeMouseActive) {
        return false;
    }
#if defined(AY_DEVICE_USE_SDL2)
    // The SDL editor host is not enabled yet. Preserve SDL's default cursor
    // until that backend owns a small SDL_SystemCursor cache.
    return false;
#elif defined(_WIN32)
    if (_impl->hwnd == nullptr) {
        return false;
    }
    ::SetCursor(nativeCursor(_impl->cursorShape));
    return true;
#else
    return false;
#endif
}

bool WindowManager::getCursorScreenPosition(int& x, int& y) const
{
#if defined(AY_DEVICE_USE_SDL2)
    SDL_GetGlobalMouseState(&x, &y);
    return true;
#elif defined(_WIN32)
    POINT point{};
    if (!::GetCursorPos(&point)) {
        return false;
    }
    x = static_cast<int>(point.x);
    y = static_cast<int>(point.y);
    return true;
#else
    (void)x;
    (void)y;
    return false;
#endif
}

void WindowManager::updateRelativeMouseState()
{
#if defined(AY_DEVICE_USE_SDL2)
    if (!_impl) {
        return;
    }
    const bool shouldBeActive = _impl->relativeMouseRequested
                             && _impl->focused
                             && _impl->valid
                             && _impl->sdlWindow != nullptr;
    if (shouldBeActive == _impl->relativeMouseActive) {
        return;
    }
    if (SDL_SetRelativeMouseMode(shouldBeActive ? SDL_TRUE : SDL_FALSE) == 0) {
        SDL_CaptureMouse(shouldBeActive ? SDL_TRUE : SDL_FALSE);
        _impl->relativeMouseActive = shouldBeActive;
    }
#elif defined(_WIN32)
    if (!_impl) {
        return;
    }

    const bool shouldBeActive = _impl->relativeMouseRequested
                             && _impl->focused
                             && _impl->valid
                             && _impl->hwnd != nullptr;
    if (shouldBeActive == _impl->relativeMouseActive) {
        return;
    }

    if (shouldBeActive) {
        RECT clip{};
        if (!::GetClientRect(_impl->hwnd, &clip)) {
            return;
        }
        POINT topLeft{clip.left, clip.top};
        POINT bottomRight{clip.right, clip.bottom};
        if (!::ClientToScreen(_impl->hwnd, &topLeft)
            || !::ClientToScreen(_impl->hwnd, &bottomRight)) {
            return;
        }
        clip = RECT{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
        if (!::ClipCursor(&clip)) {
            return;
        }
        ::SetCapture(_impl->hwnd);
        _impl->cursorHideAdjustments = 0;
        do {
            ++_impl->cursorHideAdjustments;
        } while (::ShowCursor(FALSE) >= 0 && _impl->cursorHideAdjustments < 32);
        _impl->relativeMouseActive = true;
        return;
    }

    ::ClipCursor(nullptr);
    if (_impl->hwnd != nullptr && ::GetCapture() == _impl->hwnd) {
        ::ReleaseCapture();
    }
    while (_impl->cursorHideAdjustments > 0) {
        ::ShowCursor(TRUE);
        --_impl->cursorHideAdjustments;
    }
    _impl->relativeMouseActive = false;
#endif
}

void WindowManager::handleMouseButton(MouseButton button, bool pressed)
{
#if defined(_WIN32) && !defined(AY_DEVICE_USE_SDL2)
    if (!_impl) {
        return;
    }
    const unsigned bit = 1u << static_cast<unsigned>(button);
    if (pressed) {
        _impl->pressedMouseButtons |= bit;
        if (_impl->hwnd != nullptr) {
            ::SetCapture(_impl->hwnd);
        }
    } else {
        _impl->pressedMouseButtons &= ~bit;
        if (_impl->pressedMouseButtons == 0 && !_impl->relativeMouseActive
            && _impl->hwnd != nullptr && ::GetCapture() == _impl->hwnd) {
            ::ReleaseCapture();
        }
    }
    if (_impl->onMouseButton) {
        _impl->onMouseButton(button, pressed);
    }
#else
    (void)button;
    (void)pressed;
#endif
}

void WindowManager::setTouchEnabled(bool enabled)
{
    if (!_impl) {
        return;
    }
    _impl->touchEnabled = enabled;
#if defined(_WIN32) && !defined(AY_DEVICE_USE_SDL2)
    if (_impl->hwnd == nullptr) {
        return;  // applied on next createWindow via ensureTouchRegistration
    }
    if (enabled && !_impl->touchRegistered) {
        if (RegisterTouchWindow(_impl->hwnd, 0)) {
            _impl->touchRegistered = true;
        }
    } else if (!enabled && _impl->touchRegistered) {
        UnregisterTouchWindow(_impl->hwnd);
        _impl->touchRegistered = false;
    }
#endif
}

bool WindowManager::createChildWindow(const ChildWindowDesc& desc, void*& outHandle)
{
    outHandle = nullptr;
    if (!_impl || !_impl->valid || desc.parentHandle == nullptr
        || desc.width < 1 || desc.height < 1) {
        return false;
    }

#if defined(_WIN32)
    if (!registerChildWindowClass(_impl->instance)) {
        return false;
    }

    auto* parent = static_cast<HWND>(desc.parentHandle);
    const DWORD style = WS_CHILD | WS_CLIPSIBLINGS;
    HWND child = CreateWindowExW(
        WS_EX_NOPARENTNOTIFY,
        kChildWindowClass,
        L"",
        style,
        desc.x,
        desc.y,
        desc.width,
        desc.height,
        parent,
        nullptr,
        _impl->instance,
        nullptr);

    if (child == nullptr) {
        return false;
    }

    _impl->childWindows.push_back(child);
    outHandle = child;
    return true;

#else
    (void)desc;
    return false;
#endif
}

void WindowManager::destroyChildWindow(void* handle)
{
    if (!_impl || handle == nullptr) {
        return;
    }

#if defined(_WIN32)
    HWND hwnd = static_cast<HWND>(handle);
    auto it = std::find(_impl->childWindows.begin(), _impl->childWindows.end(), hwnd);
    if (it != _impl->childWindows.end()) {
        DestroyWindow(hwnd);
        _impl->childWindows.erase(it);
    }
#endif
}

void WindowManager::destroyAllChildWindows()
{
    if (!_impl) {
        return;
    }

#if defined(_WIN32)
    for (HWND child : _impl->childWindows) {
        if (child != nullptr) {
            DestroyWindow(child);
        }
    }
    _impl->childWindows.clear();
#endif
}

// =============================================================================
// D5 — top-level (independent) OS windows.
//
// Each top-level window has its own Win32 message pump driven by the editor
// session's `EditorChildWindowManager::routeKey`/`update` paths; the
// callback wiring (`setTopLevelCallbacks`) registers independent lambdas
// per handle so child windows close independently of the main editor
// window. Linux/macOS are stubs (return false) — v2 covers SDL2/X11 parity.
// =============================================================================

bool WindowManager::createTopLevelWindow(const TopLevelWindowDesc& desc, void*& outHandle)
{
    outHandle = nullptr;
    if (!_impl || desc.width < 1 || desc.height < 1) {
        return false;
    }

#if defined(_WIN32)
    if (!registerTopLevelWindowClass(_impl->instance)) {
        return false;
    }

    const std::wstring title = utf8ToWide(desc.title);
    // Pass `this` as lpParam so WM_NCCREATE can stash the owning
    // WindowManager via GWLP_USERDATA (the WndProc itself can't capture).
    // The public header uses `-1` as the "OS default position" sentinel
    // (K-INV-D5-3 forbids pulling Win32's `CW_USEDEFAULT` into the public
    // header); translate it back to the proper macro here.
    const int xPos = (desc.x < 0) ? CW_USEDEFAULT : desc.x;
    const int yPos = (desc.y < 0) ? CW_USEDEFAULT : desc.y;
    // Bordered: grow the OS frame so CLIENT == desc size (promote card
    // fits without overflow). Borderless: WS_POPUP, no caption — size
    // is already client pixels (DockCard paints its own title chrome).
    DWORD style = WS_CLIPCHILDREN;
    int wndW = desc.width;
    int wndH = desc.height;
    if (desc.borderless) {
        style |= WS_POPUP;
        if (desc.resizable) {
            // Thick frame + custom WM_NCHITTEST for edge resize.
            // Do NOT use WM_NCCALCSIZE→0 with THICKFRAME: Windows then
            // re-adds frame chrome and grows the HWND every pass
            // (Gallery: pure-white child → crash after promote).
            // Outer size includes the frame; client stays desc sized.
            style |= WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
            RECT clientRect{0, 0, desc.width, desc.height};
            ::AdjustWindowRectEx(&clientRect, style, FALSE, 0);
            wndW = clientRect.right - clientRect.left;
            wndH = clientRect.bottom - clientRect.top;
        }
    } else {
        style |= WS_OVERLAPPEDWINDOW;
        RECT clientRect{0, 0, desc.width, desc.height};
        ::AdjustWindowRect(&clientRect, style, FALSE);
        wndW = clientRect.right - clientRect.left;
        wndH = clientRect.bottom - clientRect.top;
    }
    HWND hwnd = CreateWindowExW(
        0,
        kTopLevelWindowClass,
        title.c_str(),
        style,
        xPos,
        yPos,
        wndW,
        wndH,
        nullptr,                    // top-level: no parent
        nullptr,                    // no menu
        _impl->instance,
        reinterpret_cast<LPVOID>(this));

    if (hwnd == nullptr) {
        return false;
    }
    if (desc.borderless && desc.resizable) {
        // Win11: square corners + dark NC/border so the thick-frame
        // chrome does not flash as a white top strip (DWM defaults).
        constexpr DWORD kDwmwaWindowCornerPreference = 33;
        constexpr DWORD kDwmwcpDoNotRound = 1;
        constexpr DWORD kDwmwaBorderColor = 34;
        constexpr DWORD kDwmwaCaptionColor = 35;
        DWORD cornerPref = kDwmwcpDoNotRound;
        COLORREF frameColor = RGB(0x28, 0x28, 0x2A);
        using DwmSetWindowAttributeFn =
            HRESULT (WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
        if (HMODULE dwm = ::LoadLibraryW(L"dwmapi.dll")) {
            if (auto* fn = reinterpret_cast<DwmSetWindowAttributeFn>(
                    ::GetProcAddress(dwm, "DwmSetWindowAttribute"))) {
                fn(hwnd, kDwmwaWindowCornerPreference, &cornerPref,
                   sizeof(cornerPref));
                fn(hwnd, kDwmwaBorderColor, &frameColor, sizeof(frameColor));
                fn(hwnd, kDwmwaCaptionColor, &frameColor, sizeof(frameColor));
            }
            ::FreeLibrary(dwm);
        }
    }
    if (desc.visible) {
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }

    {
        std::lock_guard<std::mutex> g(s_topLevelMu);
        s_topLevelOwners[hwnd] = this;
        s_topLevelCallbacks[hwnd] = TopLevelWindowCallbacks{};
        s_topLevelBorderlessResizable[hwnd] =
            (desc.borderless && desc.resizable);
    }
    _impl->topLevelWindows.push_back(hwnd);
    outHandle = static_cast<void*>(hwnd);
    return true;

#else
    (void)desc;
    return false;
#endif
}

void WindowManager::destroyTopLevelWindow(void* handle)
{
    if (!_impl || handle == nullptr) {
        return;
    }

#if defined(_WIN32)
    HWND hwnd = static_cast<HWND>(handle);
    {
        std::lock_guard<std::mutex> g(s_topLevelMu);
        // L4 (2026-08-26): erase ALL per-HWND state BEFORE calling
        // DestroyWindow. Once the map entries are gone, the WndProc
        // for this hwnd becomes a no-op (it falls through to
        // DefWindowProc) even if Win32 still has a pending message
        // in its queue. This closes the UAF window where
        // WM_PAINT/WM_MOVE could be dispatched after the owning
        // WindowManager* is destroyed but before the HWND is gone.
        s_topLevelCallbacks.erase(hwnd);
        s_topLevelOwners.erase(hwnd);
        s_topLevelBorderlessResizable.erase(hwnd);
        // L1 (2026-08-26): erase per-HWND surrogate state. Dropping
        // the entry without flushing the high surrogate is acceptable:
        // the very last WM_CHAR for a destroyed window is by definition
        // orphaned (the window is gone).
        s_topLevelPendingHigh.erase(hwnd);
    }
    auto it = std::find(_impl->topLevelWindows.begin(), _impl->topLevelWindows.end(), hwnd);
    if (it != _impl->topLevelWindows.end()) {
        DestroyWindow(hwnd);
        _impl->topLevelWindows.erase(it);
    }
#else
    (void)handle;
#endif
}

void WindowManager::destroyAllTopLevelWindows()
{
    if (!_impl) {
        return;
    }

#if defined(_WIN32)
    {
        std::lock_guard<std::mutex> g(s_topLevelMu);
        // Iterate over a snapshot — destroyTopLevelWindow mutates
        // s_topLevelOwners / s_topLevelCallbacks.
        const auto windows = _impl->topLevelWindows;
        for (HWND hwnd : windows) {
            s_topLevelCallbacks.erase(hwnd);
            s_topLevelOwners.erase(hwnd);
            s_topLevelBorderlessResizable.erase(hwnd);
            // L1 (2026-08-26): per-HWND surrogate state cleanup.
            s_topLevelPendingHigh.erase(hwnd);
        }
    }
    for (HWND hwnd : _impl->topLevelWindows) {
        if (hwnd != nullptr) {
            DestroyWindow(hwnd);
        }
    }
    _impl->topLevelWindows.clear();
#else
#endif
}

void WindowManager::setTopLevelCallbacks(void* handle, const TopLevelWindowCallbacks& cbs)
{
#if defined(_WIN32)
    if (!_impl || handle == nullptr) {
        return;
    }
    HWND hwnd = static_cast<HWND>(handle);
    std::lock_guard<std::mutex> g(s_topLevelMu);
    s_topLevelCallbacks[hwnd] = cbs;
#else
    (void)handle;
    (void)cbs;
#endif
}

bool WindowManager::setTopLevelVisible(void* handle, bool visible)
{
#if defined(_WIN32)
    if (!_impl || handle == nullptr) {
        return false;
    }
    HWND hwnd = static_cast<HWND>(handle);
    const BOOL ok = ::ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
    if (visible) {
        ::UpdateWindow(hwnd);
    }
    // ShowWindow's BOOL is "was previously visible", not success.
    return ::IsWindowVisible(hwnd) == (visible ? TRUE : FALSE) || ok != 0;
#else
    (void)handle;
    (void)visible;
    return false;
#endif
}

bool WindowManager::toggleTopLevelMaximized(void* handle)
{
#if defined(_WIN32)
    if (!_impl || handle == nullptr) {
        return false;
    }
    HWND hwnd = static_cast<HWND>(handle);
    if (::IsZoomed(hwnd)) {
        return ::ShowWindow(hwnd, SW_RESTORE) != 0;
    }
    return ::ShowWindow(hwnd, SW_MAXIMIZE) != 0;
#else
    (void)handle;
    return false;
#endif
}

bool WindowManager::isTopLevelMaximized(void* handle) const
{
#if defined(_WIN32)
    if (!_impl || handle == nullptr) {
        return false;
    }
    return ::IsZoomed(static_cast<HWND>(handle)) != 0;
#else
    (void)handle;
    return false;
#endif
}

void WindowManager::emitInputEvent(const DeviceInputEvent& event)
{
    if (_impl && _impl->onInputEvent) {
        _impl->onInputEvent(event);
    }
}

void WindowManager::processPlatformEvent(unsigned msg, std::uintptr_t wParam, std::intptr_t lParam)
{
    if (!_impl) {
        return;
    }

#if defined(_WIN32)
    switch (msg) {
    case WM_CLOSE:
        notifyClosed();
        break;
    case WM_SIZE:
        if (_impl->hwnd != nullptr) {
            readClientSize(_impl->hwnd, _impl->width, _impl->height);
        }
        notifyResized(_impl->width, _impl->height);
        break;
    case WM_SETFOCUS:
        notifyFocused(true);
        break;
    case WM_KILLFOCUS:
        notifyFocused(false);
        break;

    // ===== Keyboard =====
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        const bool repeat = (lParam & (1 << 30)) != 0;
        const KeyCode key = translateVirtualKey(wParam, lParam);
        if (key != KeyCode::Unknown) {
            if (_impl->onKey && !repeat) {
                _impl->onKey(key, true);
            }
            DeviceInputEvent event{};
            event.type = DeviceInputEventType::Key;
            event.key = key;
            event.pressed = true;
            event.repeat = repeat;
            emitInputEvent(event);
        }
        break;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP: {
        const KeyCode key = translateVirtualKey(wParam, lParam);
        if (key != KeyCode::Unknown) {
            if (_impl->onKey) {
                _impl->onKey(key, false);
            }
            DeviceInputEvent event{};
            event.type = DeviceInputEventType::Key;
            event.key = key;
            event.pressed = false;
            emitInputEvent(event);
        }
        break;
    }

    // ===== Mouse move =====
    case WM_MOUSEMOVE: {
        if (!_impl->relativeMouseActive) {
            const int x = static_cast<short>(LOWORD(lParam));
            const int y = static_cast<short>(HIWORD(lParam));
            if (_impl->onMouseMove) {
                _impl->onMouseMove(static_cast<float>(x), static_cast<float>(y));
            }
            DeviceInputEvent event{};
            event.type = DeviceInputEventType::MouseMove;
            event.x = static_cast<float>(x);
            event.y = static_cast<float>(y);
            emitInputEvent(event);

            if (!_impl->mouseLeaveTracking && _impl->hwnd != nullptr) {
                TRACKMOUSEEVENT tracking{};
                tracking.cbSize = sizeof(tracking);
                tracking.dwFlags = TME_LEAVE;
                tracking.hwndTrack = _impl->hwnd;
                if (::TrackMouseEvent(&tracking)) {
                    _impl->mouseLeaveTracking = true;
                }
            }
        }
        break;
    }
    case WM_MOUSELEAVE: {
        _impl->mouseLeaveTracking = false;
        if (_impl->pressedMouseButtons == 0 && !_impl->relativeMouseActive) {
            DeviceInputEvent event{};
            event.type = DeviceInputEventType::MouseLeave;
            emitInputEvent(event);
        }
        break;
    }

    // ===== Mouse buttons =====
    case WM_LBUTTONDOWN: {
        handleMouseButton(MouseButton::Left, true);
        DeviceInputEvent event{};
        event.type = DeviceInputEventType::MouseButton;
        event.mouseButton = MouseButton::Left;
        event.pressed = true;
        event.x = static_cast<float>(static_cast<short>(LOWORD(lParam)));
        event.y = static_cast<float>(static_cast<short>(HIWORD(lParam)));
        emitInputEvent(event);
        break;
    }
    case WM_LBUTTONUP: {
        handleMouseButton(MouseButton::Left, false);
        DeviceInputEvent event{};
        event.type = DeviceInputEventType::MouseButton;
        event.mouseButton = MouseButton::Left;
        event.pressed = false;
        event.x = static_cast<float>(static_cast<short>(LOWORD(lParam)));
        event.y = static_cast<float>(static_cast<short>(HIWORD(lParam)));
        emitInputEvent(event);
        break;
    }
    case WM_RBUTTONDOWN: {
        handleMouseButton(MouseButton::Right, true);
        DeviceInputEvent event{};
        event.type = DeviceInputEventType::MouseButton;
        event.mouseButton = MouseButton::Right;
        event.pressed = true;
        event.x = static_cast<float>(static_cast<short>(LOWORD(lParam)));
        event.y = static_cast<float>(static_cast<short>(HIWORD(lParam)));
        emitInputEvent(event);
        break;
    }
    case WM_RBUTTONUP: {
        handleMouseButton(MouseButton::Right, false);
        DeviceInputEvent event{};
        event.type = DeviceInputEventType::MouseButton;
        event.mouseButton = MouseButton::Right;
        event.pressed = false;
        event.x = static_cast<float>(static_cast<short>(LOWORD(lParam)));
        event.y = static_cast<float>(static_cast<short>(HIWORD(lParam)));
        emitInputEvent(event);
        break;
    }
    case WM_MBUTTONDOWN: {
        handleMouseButton(MouseButton::Middle, true);
        DeviceInputEvent event{};
        event.type = DeviceInputEventType::MouseButton;
        event.mouseButton = MouseButton::Middle;
        event.pressed = true;
        event.x = static_cast<float>(static_cast<short>(LOWORD(lParam)));
        event.y = static_cast<float>(static_cast<short>(HIWORD(lParam)));
        emitInputEvent(event);
        break;
    }
    case WM_MBUTTONUP: {
        handleMouseButton(MouseButton::Middle, false);
        DeviceInputEvent event{};
        event.type = DeviceInputEventType::MouseButton;
        event.mouseButton = MouseButton::Middle;
        event.pressed = false;
        event.x = static_cast<float>(static_cast<short>(LOWORD(lParam)));
        event.y = static_cast<float>(static_cast<short>(HIWORD(lParam)));
        emitInputEvent(event);
        break;
    }
    case WM_XBUTTONDOWN: {
        const MouseButton button =
            (HIWORD(wParam) == XBUTTON1) ? MouseButton::X1 : MouseButton::X2;
        handleMouseButton(button, true);
        DeviceInputEvent event{};
        event.type = DeviceInputEventType::MouseButton;
        event.mouseButton = button;
        event.pressed = true;
        event.x = static_cast<float>(static_cast<short>(LOWORD(lParam)));
        event.y = static_cast<float>(static_cast<short>(HIWORD(lParam)));
        emitInputEvent(event);
        break;
    }
    case WM_XBUTTONUP: {
        const MouseButton button =
            (HIWORD(wParam) == XBUTTON1) ? MouseButton::X1 : MouseButton::X2;
        handleMouseButton(button, false);
        DeviceInputEvent event{};
        event.type = DeviceInputEventType::MouseButton;
        event.mouseButton = button;
        event.pressed = false;
        event.x = static_cast<float>(static_cast<short>(LOWORD(lParam)));
        event.y = static_cast<float>(static_cast<short>(HIWORD(lParam)));
        emitInputEvent(event);
        break;
    }

    // ===== Mouse wheel (normalized to notches) =====
    case WM_MOUSEWHEEL: {
        if (ayDeviceTraceInputEnabled()) {
            ++_impl->mouseWheelTriggerCount;
            if (_impl->mouseWheelTriggerCount == 1 ||
                (_impl->mouseWheelTriggerCount % 60) == 0) {
                const short raw = static_cast<short>(HIWORD(wParam));
                std::fprintf(stderr,
                    "[AYDevice-InputTrace] WM_MOUSEWHEEL fire #%d delta=%d\n",
                    _impl->mouseWheelTriggerCount, static_cast<int>(raw));
            }
        }
        const short raw = static_cast<short>(HIWORD(wParam));
        const float notches =
            static_cast<float>(raw) / static_cast<float>(WHEEL_DELTA);
        if (_impl->onMouseWheel) {
            _impl->onMouseWheel(notches);
        }
        POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (_impl->hwnd != nullptr) {
            ::ScreenToClient(_impl->hwnd, &pt);
        }
        DeviceInputEvent event{};
        event.type = DeviceInputEventType::MouseWheel;
        event.wheelSource = MouseWheelSource::Standard;
        event.x = static_cast<float>(pt.x);
        event.y = static_cast<float>(pt.y);
        event.deltaY = notches;
        emitInputEvent(event);
        break;
    }

    // PR-InputTrace + L2 (2026-08-26): WM_POINTERUPDATE is what
    // Windows Precision trackpads (Dell/HP/Lenovo/Surface) deliver
    // for two-finger gestures. POINTER_INFO::ptPixelLocation is
    // screen-space. When the pointer is a touchpad (pointerType ==
    // PT_TOUCHPAD) we accumulate position changes between frames and
    // emit them as wheel deltas in the Y axis (the conventional
    // trackpad-scroll convention). X delta is logged but not yet
    // routed to a horizontal wheel seam (deferred to v2 — the main
    // window's onMouseWheel is single-axis).
    case WM_POINTERUPDATE: {
        if (_impl->hwnd == nullptr) {
            break;
        }
        const UINT32 pointerId = LOWORD(wParam);
        POINTER_INPUT_TYPE pointerType{};
        if (!::GetPointerType(pointerId, &pointerType)
            || pointerType != PT_TOUCHPAD) {
            // Diagnostic-only path for non-touchpad pointers (mouse,
            // pen, touch).
            if (ayDeviceTraceInputEnabled()) {
                ++_impl->pointerTriggerCount;
                if (_impl->pointerTriggerCount == 1 ||
                    (_impl->pointerTriggerCount % 60) == 0) {
                    std::fprintf(stderr,
                        "[AYDevice-InputTrace] WM_POINTERUPDATE fire #%d wParam=0x%p (non-touchpad)\n",
                        _impl->pointerTriggerCount,
                        reinterpret_cast<void*>(wParam));
                }
            }
            break;
        }
        POINTER_INFO pi{};
        if (!::GetPointerInfo(pointerId, &pi)) {
            break;
        }
        // Use the per-pointer slot for trackpad panning. Lazy-init the
        // last position on first fire; thereafter each WM_POINTERUPDATE
        // yields a delta.
        auto& slot = _impl->trackpadLastX[pointerId];
        auto& slotY = _impl->trackpadLastY[pointerId];
        const bool hadLast = _impl->trackpadHasLast.count(pointerId) != 0;
        if (!hadLast) {
            slot = pi.ptPixelLocation.x;
            slotY = pi.ptPixelLocation.y;
            _impl->trackpadHasLast.insert(pointerId);
            break;
        }
        const long dx = pi.ptPixelLocation.x - slot;
        const long dy = pi.ptPixelLocation.y - slotY;
        slot  = pi.ptPixelLocation.x;
        slotY = pi.ptPixelLocation.y;
        if (dy != 0 && _impl->onMouseWheel) {
            // PR-Dock-TearOff wheel guard: same rationale as WM_INPUT
            // above — child windows own their own wheel routing.
            POINT pt = pi.ptPixelLocation;
            HWND under = ::WindowFromPoint(pt);
            if (under == nullptr || under == _impl->hwnd
                || ::IsChild(_impl->hwnd, under)) {
                // Normalize pixel delta to notches (~120 px per notch,
                // matching WHEEL_DELTA). Trackpads emit fractional
                // deltas so the divisor smooths the impulse.
                _impl->onMouseWheel(-static_cast<float>(dy)
                                    / static_cast<float>(WHEEL_DELTA));
            }
        }
        if (dy != 0) {
            POINT screenPt = pi.ptPixelLocation;
            HWND under = ::WindowFromPoint(screenPt);
            if (under == nullptr || under == _impl->hwnd
                || ::IsChild(_impl->hwnd, under)) {
                POINT clientPt = screenPt;
                ::ScreenToClient(_impl->hwnd, &clientPt);
                DeviceInputEvent event{};
                event.type = DeviceInputEventType::MouseWheel;
                event.wheelSource = MouseWheelSource::Pointer;
                event.x = static_cast<float>(clientPt.x);
                event.y = static_cast<float>(clientPt.y);
                event.deltaY = -static_cast<float>(dy)
                             / static_cast<float>(WHEEL_DELTA);
                emitInputEvent(event);
            }
        }
        if (ayDeviceTraceInputEnabled()) {
            ++_impl->pointerTriggerCount;
            if (_impl->pointerTriggerCount == 1 ||
                (_impl->pointerTriggerCount % 60) == 0) {
                std::fprintf(stderr,
                    "[AYDevice-InputTrace] WM_POINTERUPDATE touchpad fire #%d dx=%ld dy=%ld\n",
                    _impl->pointerTriggerCount, dx, dy);
            }
        }
        break;
    }

    // PR-InputTrace: WM_GESTURE fires for pinch / rotate / two-finger
    // pan on Precision trackpads. GID_PAN == 4 carries panDeltaX/Y
    // which is fractional pixels. Diagnostic-only for now.
    case WM_GESTURE: {
        if (ayDeviceTraceInputEnabled()) {
            ++_impl->gestureTriggerCount;
            if (_impl->gestureTriggerCount == 1 ||
                (_impl->gestureTriggerCount % 60) == 0) {
                std::fprintf(stderr,
                    "[AYDevice-InputTrace] WM_GESTURE fire #%d wParam=0x%p lParam=0x%p\n",
                    _impl->gestureTriggerCount,
                    reinterpret_cast<void*>(wParam),
                    reinterpret_cast<void*>(lParam));
            }
        }
        break;
    }

    // ===== Raw-input mouse wheel (PR-InputTrace) =====
    // Windows precision trackpad / MagicMouse deliver wheel deltas via
    // WM_INPUT carrying RAWINPUT with RIM_TYPEMOUSE and the
    // RI_MOUSE_WHEEL bit set in usButtonFlags. usButtonData is a signed
    // SHORT whose magnitude is a multiple of WHEEL_DELTA (120). Same
    // notches normalization as WM_MOUSEWHEEL.
    case WM_INPUT: {
        // PR-Dock-TearOff wheel guard: RIDEV_INPUTSINK delivers raw
        // input for EVERY window (top-level child windows included) to
        // the main window. When the cursor is over a top-level child
        // window, skip — the child window's own WM_MOUSEWHEEL path
        // handles it. Without this guard the same wheel notch would be
        // double-applied (child + main). IsChild covers any future
        // WS_CHILD surfaces of the main window.
        if (_impl->hwnd != nullptr) {
            POINT pt{};
            if (::GetCursorPos(&pt)) {
                HWND under = ::WindowFromPoint(pt);
                if (under != nullptr && under != _impl->hwnd &&
                    !::IsChild(_impl->hwnd, under)) {
                    break;
                }
            }
        }
        UINT dwSize = 0;
        ::GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam),
                          RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
        if (dwSize == 0) break;
        std::vector<BYTE> buf(dwSize);
        if (::GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam),
                              RID_INPUT, buf.data(), &dwSize,
                              sizeof(RAWINPUTHEADER)) != dwSize) break;
        auto* raw = reinterpret_cast<RAWINPUT*>(buf.data());
        if (raw->header.dwType != RIM_TYPEMOUSE) break;
        if (_impl->relativeMouseActive
            && (raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0) {
            const float dx = static_cast<float>(raw->data.mouse.lLastX);
            const float dy = static_cast<float>(raw->data.mouse.lLastY);
            if (_impl->onMouseDelta) {
                _impl->onMouseDelta(dx, dy);
            }
            DeviceInputEvent event{};
            event.type = DeviceInputEventType::MouseDelta;
            event.deltaX = dx;
            event.deltaY = dy;
            emitInputEvent(event);
        }
        if ((raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL)) {
            if (ayDeviceTraceInputEnabled()) {
                ++_impl->rawInputTriggerCount;
                if (_impl->rawInputTriggerCount == 1 ||
                    (_impl->rawInputTriggerCount % 60) == 0) {
                    const short wheelDelta =
                        static_cast<short>(raw->data.mouse.usButtonData);
                    std::fprintf(stderr,
                        "[AYDevice-InputTrace] WM_INPUT RI_MOUSE_WHEEL fire #%d delta=%d\n",
                        _impl->rawInputTriggerCount, static_cast<int>(wheelDelta));
                }
            }
        }
        if ((raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL) && _impl->onMouseWheel) {
            const short wheelDelta =
                static_cast<short>(raw->data.mouse.usButtonData);
            _impl->onMouseWheel(static_cast<float>(wheelDelta)
                                / static_cast<float>(WHEEL_DELTA));
        }
        if ((raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL)) {
            POINT pt{};
            if (::GetCursorPos(&pt) && _impl->hwnd != nullptr) {
                ::ScreenToClient(_impl->hwnd, &pt);
            }
            const short wheelDelta =
                static_cast<short>(raw->data.mouse.usButtonData);
            DeviceInputEvent event{};
            event.type = DeviceInputEventType::MouseWheel;
            event.wheelSource = MouseWheelSource::RawInput;
            event.x = static_cast<float>(pt.x);
            event.y = static_cast<float>(pt.y);
            event.deltaY = static_cast<float>(wheelDelta)
                         / static_cast<float>(WHEEL_DELTA);
            emitInputEvent(event);
        }
        break;
    }

    // ===== Touch (WM_TOUCH: decode contacts, map to client space) =====
    case WM_TOUCH:
        if ((_impl->onTouch || _impl->onInputEvent) && _impl->hwnd != nullptr) {
            const UINT count = LOWORD(wParam);
            if (count > 0) {
                std::vector<TOUCHINPUT> inputs(count);
                auto handle = reinterpret_cast<HTOUCHINPUT>(lParam);
                if (GetTouchInputInfo(handle, count, inputs.data(), sizeof(TOUCHINPUT))) {
                    for (const TOUCHINPUT& ti : inputs) {
                        // TOUCHINPUT coordinates are in 0.01 px screen units.
                        POINT pt{ti.x / 100, ti.y / 100};
                        ScreenToClient(_impl->hwnd, &pt);

                        TouchPhase phase = TouchPhase::Moved;
                        if (ti.dwFlags & TOUCHEVENTF_DOWN) {
                            phase = TouchPhase::Began;
                        } else if (ti.dwFlags & TOUCHEVENTF_UP) {
                            phase = TouchPhase::Ended;
                        }
                        if (_impl->onTouch) {
                            _impl->onTouch(static_cast<int64_t>(ti.dwID),
                                           static_cast<float>(pt.x),
                                           static_cast<float>(pt.y),
                                           phase);
                        }
                        DeviceInputEvent event{};
                        event.type = DeviceInputEventType::Touch;
                        event.pointerId = static_cast<int64_t>(ti.dwID);
                        event.x = static_cast<float>(pt.x);
                        event.y = static_cast<float>(pt.y);
                        event.touchPhase = phase;
                        emitInputEvent(event);
                    }
                    CloseTouchInputHandle(handle);
                }
            }
        }
        break;

    // ===== Text: committed characters (UTF-16 -> UTF-8, surrogate-aware) =====
    case WM_CHAR: {
        if (_impl->onChar || _impl->onInputEvent) {
            const CharCallback sink = [this](const char* utf8, int byteCount) {
                if (_impl->onChar) {
                    _impl->onChar(utf8, byteCount);
                }
                DeviceInputEvent event{};
                event.type = DeviceInputEventType::TextCommit;
                if (utf8 != nullptr && byteCount > 0) {
                    event.text.assign(utf8, static_cast<size_t>(byteCount));
                }
                emitInputEvent(event);
            };
            handleWmChar(static_cast<wchar_t>(wParam), sink,
                         _impl->pendingHighSurrogate);
        }
        break;
    }

    // ===== IME composition (in-progress candidate string) =====
    case WM_IME_COMPOSITION:
        if ((_impl->onComposition || _impl->onInputEvent)
            && _impl->hwnd != nullptr && (lParam & GCS_COMPSTR)) {
            HIMC himc = ImmGetContext(_impl->hwnd);
            if (himc != nullptr) {
                const LONG bytes = ImmGetCompositionStringW(himc, GCS_COMPSTR, nullptr, 0);
                if (bytes > 0) {
                    std::wstring wide(static_cast<size_t>(bytes) / sizeof(wchar_t), L'\0');
                    ImmGetCompositionStringW(himc, GCS_COMPSTR, wide.data(), bytes);
                    const LONG cursor = ImmGetCompositionStringW(himc, GCS_CURSORPOS, nullptr, 0);
                    const std::string utf8 = wideToUtf8(wide.data(), static_cast<int>(wide.size()));
                    if (_impl->onComposition) {
                        _impl->onComposition(utf8.c_str(), static_cast<int>(utf8.size()),
                                             static_cast<int>(cursor));
                    }
                    DeviceInputEvent event{};
                    event.type = _impl->compositionActive
                        ? DeviceInputEventType::CompositionUpdate
                        : DeviceInputEventType::CompositionStart;
                    _impl->compositionActive = true;
                    event.text = utf8;
                    event.compositionCursor = static_cast<int>(cursor);
                    emitInputEvent(event);
                } else {
                    if (_impl->onComposition) {
                        _impl->onComposition("", 0, 0);
                    }
                    DeviceInputEvent event{};
                    event.type = _impl->compositionActive
                        ? DeviceInputEventType::CompositionUpdate
                        : DeviceInputEventType::CompositionStart;
                    _impl->compositionActive = true;
                    emitInputEvent(event);
                }
                ImmReleaseContext(_impl->hwnd, himc);
            }
        }
        break;

    case WM_IME_ENDCOMPOSITION:
        if (_impl->onComposition) {
            _impl->onComposition(nullptr, -1, 0);  // sentinel: composition ended
        }
        {
            _impl->compositionActive = false;
            DeviceInputEvent event{};
            event.type = DeviceInputEventType::CompositionEnd;
            emitInputEvent(event);
        }
        break;

    default:
        break;
    }
    (void)wParam;
    (void)lParam;
#else
    (void)msg;
    (void)wParam;
    (void)lParam;
#endif
}

std::intptr_t WindowManager::tryHandleUserMessage(unsigned msg, std::uintptr_t wParam,
                                                  std::intptr_t lParam, bool& handled) const
{
    handled = false;
    if (!_impl || !_impl->onMessage) {
        return 0;
    }
    return _impl->onMessage(msg, wParam, lParam, handled);
}

} // namespace ayt::device
