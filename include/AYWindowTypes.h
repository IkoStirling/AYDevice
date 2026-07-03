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

using WindowCloseCallback = std::function<void()>;
using WindowResizeCallback = std::function<void(int width, int height)>;
using WindowFocusCallback = std::function<void(bool focused)>;
using WindowMessageCallback = std::function<std::intptr_t(unsigned msg, std::uintptr_t wParam,
                                                          std::intptr_t lParam, bool& handled)>;

} // namespace ayt::device
