#include "AYWindowManager.h"

#include <algorithm>
#include <string>
#include <unordered_map>
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
#  include <imm.h>
#endif

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

struct WindowManager::Impl {
#if defined(_WIN32)
    HWND hwnd = nullptr;
    HINSTANCE instance = GetModuleHandleW(nullptr);
    int width = 0;
    int height = 0;
    bool resizable = true;
    std::vector<HWND> childWindows;
    std::vector<HWND> topLevelWindows;       // D5 — owns HWNDs created by createTopLevelWindow

    WindowCloseCallback onClose;
    WindowResizeCallback onResize;
    WindowFocusCallback onFocus;
    WindowMessageCallback onMessage;

    KeyCallback onKey;
    MouseButtonCallback onMouseButton;
    MouseMoveCallback onMouseMove;
    MouseWheelCallback onMouseWheel;

    TouchCallback onTouch;
    CharCallback onChar;
    CompositionCallback onComposition;
    bool touchEnabled = false;
    bool touchRegistered = false;
    // PR-InputTrace: precision trackpad / MagicMouse wheel goes through
    // WM_INPUT (raw input) instead of WM_MOUSEWHEEL. Registered once per
    // window in createWindow; unregistered in destroyWindow.
    bool rawInputRegistered = false;

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
std::mutex s_topLevelMu;

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
        return TRUE;
    }

    WindowManager* owner = windowFromHwnd(hwnd);
    if (owner != nullptr) {
        if (msg == WM_SIZE) {
            // Read new client size; invoke callbacks from a map guarded by
            // the same mutex as the owners map (to avoid iterator
            // invalidation if a callback mutates s_topLevelCallbacks).
            TopLevelWindowCallbacks cbs;
            {
                std::lock_guard<std::mutex> g(s_topLevelMu);
                auto it = s_topLevelCallbacks.find(hwnd);
                if (it != s_topLevelCallbacks.end()) {
                    cbs = it->second;
                }
            }
            if (cbs.onResize) {
                int w = 0, h = 0;
                readClientSize(hwnd, w, h);
                cbs.onResize(w, h);
            }
            return 0;
        }
        if (msg == WM_CLOSE) {
            TopLevelWindowCallbacks cbs;
            {
                std::lock_guard<std::mutex> g(s_topLevelMu);
                auto it = s_topLevelCallbacks.find(hwnd);
                if (it != s_topLevelCallbacks.end()) {
                    cbs = it->second;
                }
            }
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
        }
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
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
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
        if (::RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
            _impl->rawInputRegistered = true;
        }
    }

    if (info.hidden) {
        ShowWindow(hwnd, SW_HIDE);
    } else {
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }

    _impl->valid = true;
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
    if (_impl && _impl->onFocus) {
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

void WindowManager::setKeyCallback(KeyCallback callback)
{
#if defined(_WIN32)
    if (_impl) {
        _impl->onKey = std::move(callback);
    }
#else
    (void)callback;
#endif
}

void WindowManager::setMouseButtonCallback(MouseButtonCallback callback)
{
#if defined(_WIN32)
    if (_impl) {
        _impl->onMouseButton = std::move(callback);
    }
#else
    (void)callback;
#endif
}

void WindowManager::setMouseMoveCallback(MouseMoveCallback callback)
{
#if defined(_WIN32)
    if (_impl) {
        _impl->onMouseMove = std::move(callback);
    }
#else
    (void)callback;
#endif
}

void WindowManager::setMouseWheelCallback(MouseWheelCallback callback)
{
#if defined(_WIN32)
    if (_impl) {
        _impl->onMouseWheel = std::move(callback);
    }
#else
    (void)callback;
#endif
}

void WindowManager::setTouchCallback(TouchCallback callback)
{
#if defined(_WIN32)
    if (_impl) {
        _impl->onTouch = std::move(callback);
    }
#else
    (void)callback;
#endif
}

void WindowManager::setCharCallback(CharCallback callback)
{
#if defined(_WIN32)
    if (_impl) {
        _impl->onChar = std::move(callback);
    }
#else
    (void)callback;
#endif
}

void WindowManager::setCompositionCallback(CompositionCallback callback)
{
#if defined(_WIN32)
    if (_impl) {
        _impl->onComposition = std::move(callback);
    }
#else
    (void)callback;
#endif
}

void WindowManager::setTouchEnabled(bool enabled)
{
#if defined(_WIN32)
    if (!_impl) {
        return;
    }
    _impl->touchEnabled = enabled;
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
#else
    (void)enabled;
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
    HWND hwnd = CreateWindowExW(
        0,
        kTopLevelWindowClass,
        title.c_str(),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        xPos,
        yPos,
        desc.width,
        desc.height,
        nullptr,                    // top-level: no parent
        nullptr,                    // no menu
        _impl->instance,
        reinterpret_cast<LPVOID>(this));

    if (hwnd == nullptr) {
        return false;
    }

    {
        std::lock_guard<std::mutex> g(s_topLevelMu);
        s_topLevelOwners[hwnd] = this;
        s_topLevelCallbacks[hwnd] = TopLevelWindowCallbacks{};
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
        s_topLevelCallbacks.erase(hwnd);
        s_topLevelOwners.erase(hwnd);
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
    case WM_SYSKEYDOWN:
        if (_impl->onKey) {
            const bool repeat = (lParam & (1 << 30)) != 0;  // bit 30 = previous key state
            if (!repeat) {
                _impl->onKey(translateVirtualKey(wParam, lParam), true);
            }
        }
        break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (_impl->onKey) {
            _impl->onKey(translateVirtualKey(wParam, lParam), false);
        }
        break;

    // ===== Mouse move =====
    case WM_MOUSEMOVE:
        if (_impl->onMouseMove) {
            const int x = static_cast<short>(LOWORD(lParam));
            const int y = static_cast<short>(HIWORD(lParam));
            _impl->onMouseMove(static_cast<float>(x), static_cast<float>(y));
        }
        break;

    // ===== Mouse buttons =====
    case WM_LBUTTONDOWN:
        if (_impl->onMouseButton) { _impl->onMouseButton(MouseButton::Left, true); }
        break;
    case WM_LBUTTONUP:
        if (_impl->onMouseButton) { _impl->onMouseButton(MouseButton::Left, false); }
        break;
    case WM_RBUTTONDOWN:
        if (_impl->onMouseButton) { _impl->onMouseButton(MouseButton::Right, true); }
        break;
    case WM_RBUTTONUP:
        if (_impl->onMouseButton) { _impl->onMouseButton(MouseButton::Right, false); }
        break;
    case WM_MBUTTONDOWN:
        if (_impl->onMouseButton) { _impl->onMouseButton(MouseButton::Middle, true); }
        break;
    case WM_MBUTTONUP:
        if (_impl->onMouseButton) { _impl->onMouseButton(MouseButton::Middle, false); }
        break;
    case WM_XBUTTONDOWN:
        if (_impl->onMouseButton) {
            const MouseButton btn = (HIWORD(wParam) == XBUTTON1) ? MouseButton::X1 : MouseButton::X2;
            _impl->onMouseButton(btn, true);
        }
        break;
    case WM_XBUTTONUP:
        if (_impl->onMouseButton) {
            const MouseButton btn = (HIWORD(wParam) == XBUTTON1) ? MouseButton::X1 : MouseButton::X2;
            _impl->onMouseButton(btn, false);
        }
        break;

    // ===== Mouse wheel (normalized to notches) =====
    case WM_MOUSEWHEEL:
        if (_impl->onMouseWheel) {
            const short raw = static_cast<short>(HIWORD(wParam));
            _impl->onMouseWheel(static_cast<float>(raw) / static_cast<float>(WHEEL_DELTA));
        }
        break;

    // ===== Raw-input mouse wheel (PR-InputTrace) =====
    // Windows precision trackpad / MagicMouse deliver wheel deltas via
    // WM_INPUT carrying RAWINPUT with RIM_TYPEMOUSE and the
    // RI_MOUSE_WHEEL bit set in usButtonFlags. usButtonData is a signed
    // SHORT whose magnitude is a multiple of WHEEL_DELTA (120). Same
    // notches normalization as WM_MOUSEWHEEL.
    case WM_INPUT: {
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
        if ((raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL) && _impl->onMouseWheel) {
            const short wheelDelta =
                static_cast<short>(raw->data.mouse.usButtonData);
            _impl->onMouseWheel(static_cast<float>(wheelDelta)
                                / static_cast<float>(WHEEL_DELTA));
        }
        break;
    }

    // ===== Touch (WM_TOUCH: decode contacts, map to client space) =====
    case WM_TOUCH:
        if (_impl->onTouch && _impl->hwnd != nullptr) {
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
                        _impl->onTouch(static_cast<int64_t>(ti.dwID),
                                       static_cast<float>(pt.x),
                                       static_cast<float>(pt.y),
                                       phase);
                    }
                    CloseTouchInputHandle(handle);
                }
            }
        }
        break;

    // ===== Text: committed characters (UTF-16 -> UTF-8, surrogate-aware) =====
    case WM_CHAR:
        if (_impl->onChar) {
            const wchar_t unit = static_cast<wchar_t>(wParam);
            if (unit >= 0xD800 && unit <= 0xDBFF) {
                _impl->pendingHighSurrogate = unit;  // wait for low surrogate
            } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
                if (_impl->pendingHighSurrogate != 0) {
                    const wchar_t pair[2] = {_impl->pendingHighSurrogate, unit};
                    const std::string utf8 = wideToUtf8(pair, 2);
                    _impl->pendingHighSurrogate = 0;
                    if (!utf8.empty()) {
                        _impl->onChar(utf8.c_str(), static_cast<int>(utf8.size()));
                    }
                }
            } else if (unit >= 0x20 || unit == L'\t' || unit == L'\n' || unit == L'\r') {
                // Skip other control chars (backspace/escape stay on the key path).
                const std::string utf8 = wideToUtf8(&unit, 1);
                if (!utf8.empty()) {
                    _impl->onChar(utf8.c_str(), static_cast<int>(utf8.size()));
                }
            }
        }
        break;

    // ===== IME composition (in-progress candidate string) =====
    case WM_IME_COMPOSITION:
        if (_impl->onComposition && _impl->hwnd != nullptr && (lParam & GCS_COMPSTR)) {
            HIMC himc = ImmGetContext(_impl->hwnd);
            if (himc != nullptr) {
                const LONG bytes = ImmGetCompositionStringW(himc, GCS_COMPSTR, nullptr, 0);
                if (bytes > 0) {
                    std::wstring wide(static_cast<size_t>(bytes) / sizeof(wchar_t), L'\0');
                    ImmGetCompositionStringW(himc, GCS_COMPSTR, wide.data(), bytes);
                    const LONG cursor = ImmGetCompositionStringW(himc, GCS_CURSORPOS, nullptr, 0);
                    const std::string utf8 = wideToUtf8(wide.data(), static_cast<int>(wide.size()));
                    _impl->onComposition(utf8.c_str(), static_cast<int>(utf8.size()),
                                         static_cast<int>(cursor));
                } else {
                    _impl->onComposition("", 0, 0);
                }
                ImmReleaseContext(_impl->hwnd, himc);
            }
        }
        break;

    case WM_IME_ENDCOMPOSITION:
        if (_impl->onComposition) {
            _impl->onComposition(nullptr, -1, 0);  // sentinel: composition ended
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
