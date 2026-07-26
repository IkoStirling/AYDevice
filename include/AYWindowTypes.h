#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace ayt::device {

struct WindowCreateInfo {
    std::string title = "AY Engine";
    int         width = 1280;
    int         height = 720;
    bool        fullscreen = false;
    bool        resizable = true;
    int         minWidth = 320;
    int         minHeight = 240;
    bool        hidden = false;
};

struct ChildWindowDesc {
    void* parentHandle = nullptr;
    int   x = 0;
    int   y = 0;
    int   width = 100;
    int   height = 100;
};

// D5 — top-level (independent) window descriptor. Distinct from
// ChildWindowDesc which is for embedded WS_CHILD surfaces; this
// produces a WS_OVERLAPPEDWINDOW that lives in the taskbar and
// owns its own message pump. Win32-only this iteration —
// CreateWindowXxx on Linux/X11 is v2.
//
// `x`/`y` = `-1` means "OS default position" (Win32's `CW_USEDEFAULT`).
// We keep the sentinel as a plain integer rather than `<Windows.h>`'s
// `CW_USEDEFAULT` macro because K-INV-D5-3 forbids including the
// Win32 header in public AYDevice headers.
struct TopLevelWindowDesc {
    std::string title  = "AYEngine Child";   // utf8 → wide internally
    int  x = -1;
    int  y = -1;
    int  width  = 1024;
    int  height = 720;
};

// D5 — per-window callback map (independent of the main window's
// single-WindowManager callbacks). Each top-level HWND keeps its
// own resize + close-requested lambdas so editor child windows
// close independently of the editor's primary window.
struct TopLevelWindowCallbacks {
    std::function<void(int /*width*/, int /*height*/)> onResize;          // fires from WM_SIZE
    std::function<void()>                              onCloseRequested;  // fires from WM_CLOSE
};

using WindowCloseCallback = std::function<void()>;
using WindowResizeCallback = std::function<void(int width, int height)>;
using WindowFocusCallback = std::function<void(bool focused)>;
using WindowMessageCallback = std::function<std::intptr_t(unsigned msg, std::uintptr_t wParam,
                                                          std::intptr_t lParam, bool& handled)>;

} // namespace ayt::device
