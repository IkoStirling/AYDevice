#pragma once

#include "AYDevice/WindowTypes.h"
#include "AYDevice/InputTypes.h"
#include "AYDevice/DeviceInputEvent.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace ayt::device {

// Native window lifecycle. Win32 backend in Phase-1; SDL2 optional via CMake.
class WindowManager {
public:
    WindowManager();
    ~WindowManager();

    WindowManager(const WindowManager&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;

    bool createWindow(const WindowCreateInfo& info);
    void destroyWindow();

    bool isWindowValid() const;

    void* getWindowHandle() const;
    int getWidth() const;
    int getHeight() const;
    void getSize(int& width, int& height) const;

    void setTitle(const char* title);
    void setSize(int width, int height);
    void setFullscreen(bool enabled);
    void setResizable(bool resizable);

    void setWindowCloseCallback(WindowCloseCallback callback);
    void setWindowResizeCallback(WindowResizeCallback callback);
    void setWindowFocusCallback(WindowFocusCallback callback);
    void setWindowMessageCallback(WindowMessageCallback callback);

    // Internal typed event seam used by DeviceManager. Application/UI code
    // should subscribe through DeviceManager::addInputListener so installing a
    // consumer never replaces keyboard/mouse state maintenance.
    void setInputEventCallback(DeviceInputEventCallback callback);

    // Raw input callbacks. DeviceManager wires these into keyboard/mouse devices.
    void setKeyCallback(KeyCallback callback);
    void setMouseButtonCallback(MouseButtonCallback callback);
    void setMouseMoveCallback(MouseMoveCallback callback);
    void setMouseDeltaCallback(MouseDeltaCallback callback);
    void setMouseWheelCallback(MouseWheelCallback callback);

    // Relative mode uses raw mouse deltas and owns cursor capture/visibility.
    // A requested mode is suspended while unfocused and restored on focus.
    enum class RelativeMouseResult {
        Disabled,           // requested false; cursor released, mode inactive
        Enabled,            // requested true; cursor captured + hidden
        Busy,               // window is unfocused; request accepted but suspended
    };
    RelativeMouseResult setRelativeMouseMode(bool enabled);
    bool isRelativeMouseMode() const;
    bool isFocused() const;

    // Platform-owned cursor presentation for the main window. This keeps UI
    // hosts free of Win32 LoadCursor/SetCursor calls.
    void setCursorShape(SystemCursorShape shape);
    SystemCursorShape cursorShape() const;
    bool applyCursor() const;

    // Desktop-space pointer position used by multi-window UI hosts for
    // tear-off movement and redock hit testing. Platform APIs stay inside
    // AYDevice; callers never query Win32/SDL input state directly.
    bool getCursorScreenPosition(int& x, int& y) const;

    // Touch + text/IME callbacks. Touch requires enableTouch (registers the
    // window for WM_TOUCH); text is always available once wired.
    void setTouchCallback(TouchCallback callback);
    void setCharCallback(CharCallback callback);
    void setCompositionCallback(CompositionCallback callback);
    void setTextInputEnabled(bool enabled);

    // DeviceManager uses this independent callback to clear transient input
    // when focus is lost without occupying the public focus callback slot.
    void setInputResetCallback(std::function<void()> callback);

    // Enable WM_TOUCH delivery for the main window (Win32 RegisterTouchWindow).
    void setTouchEnabled(bool enabled);

    // Explicit notification (SDL bridge / tests without native message pump).
    void notifyClosed();
    void notifyResized(int width, int height);
    void notifyFocused(bool focused);

    // Poll-style close-requested flag. Set when `notifyClosed()` is invoked
    // (typically by the SDL bridge or a Win32 WM_CLOSE handler); consumed by
    // `consumeCloseRequested()` which returns true once and resets the flag.
    // Lets DeviceSubSystem detect a close request during update() and emit
    // a WindowCloseEvent onto the EventBus without having to re-enter the
    // callback chain.
    bool consumeCloseRequested();

    // Interim editor viewport (E2-interim child HWND). Returns opaque native handle.
    bool createChildWindow(const ChildWindowDesc& desc, void*& outHandle);
    void destroyChildWindow(void* handle);
    void destroyAllChildWindows();

    // D5 — top-level (independent) OS window hosting a child DockArea. The
    // handle is opaque (`void*` — never include `<Windows.h>` in headers
    // per K-INV-D5-3). Pair with `setTopLevelCallbacks` for resize/close
    // plumbing before the user interacts with the window.
    //
    // L27 (2026-08-26): Win32-only path. The SDL2 backend
    // (AY_DEVICE_USE_SDL2) intentionally does NOT implement these
    // entry points — the SDL_Window that backs a "main" surface
    // is the only SDL2 top-level the editor uses. Promoting a child
    // DockArea to its own OS-level top-level window is a Win32
    // editor-only feature (D5 was scoped for the Gallery layout
    // manager). If a future platform needs the same feature, prefer
    // adding it as a separate code path rather than generalizing
    // createTopLevelWindow: keeping the SDL branch stub-no-op
    // avoids forcing SDL2 to learn about per-HWND surrogate
    // pairing, per-HWND ownership tables, and the multi-window
    // destroy ordering that the Win32 path implements.
    bool createTopLevelWindow(const TopLevelWindowDesc& desc, void*& outHandle);
    void destroyTopLevelWindow(void* handle);
    void destroyAllTopLevelWindows();
    void setTopLevelCallbacks(void* handle, const TopLevelWindowCallbacks& cbs);
    // Show/hide after create. Promote hosts create with visible=false,
    // paint the first GDI frame, then show — avoids a white flash.
    bool setTopLevelVisible(void* handle, bool visible);

    // Borderless+resizable child hosts: toggle OS maximize / restore.
    bool minimizeTopLevelWindow(void* handle);
    bool toggleTopLevelMaximized(void* handle);
    bool isTopLevelMaximized(void* handle) const;

    // Platform message hook (Win32 wndproc / SDL window events).
    void processPlatformEvent(unsigned msg, std::uintptr_t wParam, std::intptr_t lParam);
    std::intptr_t tryHandleUserMessage(unsigned msg, std::uintptr_t wParam, std::intptr_t lParam,
                                       bool& handled) const;

private:
    void emitInputEvent(const DeviceInputEvent& event);
    void updateRelativeMouseState();
    void handleMouseButton(MouseButton button, bool pressed);

    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace ayt::device
