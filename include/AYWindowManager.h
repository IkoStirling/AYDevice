#pragma once

#include "AYWindowTypes.h"
#include "AYInputTypes.h"

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

    // Raw input callbacks. DeviceManager wires these into keyboard/mouse devices.
    void setKeyCallback(KeyCallback callback);
    void setMouseButtonCallback(MouseButtonCallback callback);
    void setMouseMoveCallback(MouseMoveCallback callback);
    void setMouseWheelCallback(MouseWheelCallback callback);

    // Explicit notification (SDL bridge / tests without native message pump).
    void notifyClosed();
    void notifyResized(int width, int height);
    void notifyFocused(bool focused);

    // Interim editor viewport (E2-interim child HWND). Returns opaque native handle.
    bool createChildWindow(const ChildWindowDesc& desc, void*& outHandle);
    void destroyChildWindow(void* handle);
    void destroyAllChildWindows();

    // Platform message hook (Win32 wndproc / SDL window events).
    void processPlatformEvent(unsigned msg, std::uintptr_t wParam, std::intptr_t lParam);
    std::intptr_t tryHandleUserMessage(unsigned msg, std::uintptr_t wParam, std::intptr_t lParam,
                                       bool& handled) const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace ayt::device
