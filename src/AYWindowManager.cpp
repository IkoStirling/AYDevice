#include "AYWindowManager.h"

#include <algorithm>
#include <string>
#include <vector>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <Windows.h>
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

WindowManager* windowFromHwnd(HWND hwnd)
{
    return reinterpret_cast<WindowManager*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}
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

    WindowCloseCallback onClose;
    WindowResizeCallback onResize;
    WindowFocusCallback onFocus;
    WindowMessageCallback onMessage;
#endif

#if defined(AY_DEVICE_USE_SDL2)
    SDL_Window* sdlWindow = nullptr;
#endif

    bool valid = false;
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

void readClientSize(HWND hwnd, int& width, int& height)
{
    RECT rect{};
    GetClientRect(hwnd, &rect);
    width  = rect.right - rect.left;
    height = rect.bottom - rect.top;
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
        DestroyWindow(_impl->hwnd);
        _impl->hwnd = nullptr;
    }
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
    if (_impl && _impl->onClose) {
        _impl->onClose();
    }
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
