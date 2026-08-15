#pragma once

#include "AYDevice/InputTypes.h"   // KeyCode for TopLevelWindowCallbacks::onKey
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
    // PR-Dock-TearOff: whether the window is shown immediately after
    // creation. Default true; tests and hosts that want to prepare the
    // surface first (e.g. attach a render backend before the first
    // paint) pass false. Off-screen HWNDs are still created with their
    // requested client size (AdjustWindowRect) so a backend can bind.
    bool visible = true;
    // When true: WS_POPUP with no OS caption/menu — the host paints its
    // own chrome (DockCard title bar). Client size equals width×height
    // exactly when resizable=false (no AdjustWindowRect). Default false
    // keeps classic WS_OVERLAPPEDWINDOW for config-file child windows.
    bool borderless = false;
    // Borderless + resizable → WS_POPUP|WS_THICKFRAME so edges/corners
    // resize the HWND (Gallery/Editor tear-off). Ignored when
    // borderless=false (WS_OVERLAPPEDWINDOW already includes a frame).
    bool resizable = false;
};

// D5 — per-window callback map (independent of the main window's
// single-WindowManager callbacks). Each top-level HWND keeps its
// own resize + close-requested lambdas so editor child windows
// close independently of the editor's primary window.
//
// PR-Dock-TearOff: typed input callbacks, Win32 translation stays in
// AYDevice (mouse positions are client-relative floats, wheel delta is
// normalized to notches like the main window path, keys are KeyCode).
// The host forwards them into its own UIManager. Callbacks fire
// outside the s_topLevelMu lock (copy-then-invoke, same as onResize).
struct TopLevelWindowCallbacks {
    std::function<void(int /*width*/, int /*height*/)> onResize;          // fires from WM_SIZE
    std::function<void()>                              onCloseRequested;  // fires from WM_CLOSE

    // ===== PR-Dock-TearOff input routing =====
    // All coordinates are client-relative to this window.
    std::function<void(float /*x*/, float /*y*/)> onMouseMove;   // WM_MOUSEMOVE
    std::function<void()>                         onMouseLeave;  // WM_MOUSELEAVE
    // button: 0=left, 1=right, 2=middle, 3=X1, 4=X2. Return true to
    // request mouse capture (down only; up auto-releases).
    std::function<bool(float /*x*/, float /*y*/, int /*button*/,
                       bool /*pressed*/)>         onMouseButton;
    std::function<void(float /*x*/, float /*y*/,
                       float /*deltaY*/)>         onMouseWheel;  // notches, +up
    // KeyCode is ayt::device::KeyCode (same namespace as this header).
    std::function<void(KeyCode /*key*/, bool /*pressed*/)> onKey;
    // Committed text, UTF-8, surrogate pairs already combined.
    std::function<void(const char* /*utf8*/, int /*byteCount*/)> onChar;
    // WM_SETCURSOR: return true if the host applied a cursor (skip
    // DefWindowProc). Used so promoted DockCard title bars can show
    // Move/Hand hints — Gallery's primary HWND does this itself.
    std::function<bool()> onSetCursor;
};

using WindowCloseCallback = std::function<void()>;
using WindowResizeCallback = std::function<void(int width, int height)>;
using WindowFocusCallback = std::function<void(bool focused)>;
using WindowMessageCallback = std::function<std::intptr_t(unsigned msg, std::uintptr_t wParam,
                                                          std::intptr_t lParam, bool& handled)>;

} // namespace ayt::device
