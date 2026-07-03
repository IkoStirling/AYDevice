#include "AYDeviceManager.h"

#if defined(AY_DEVICE_USE_SDL2)
#  include <SDL.h>
#endif

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <Windows.h>
#endif

namespace ayt::device {

DeviceManager::DeviceManager() = default;

DeviceManager::~DeviceManager()
{
    shutdown();
}

bool DeviceManager::initialize(const DeviceConfig& config)
{
    if (_initialized) {
        return true;
    }

    if (!_windowManager.createWindow(config.window)) {
        return false;
    }

    _initialized = true;
    return true;
}

void DeviceManager::shutdown()
{
    if (!_initialized) {
        return;
    }

    _windowManager.destroyWindow();
    _initialized = false;
}

void DeviceManager::pollEvents()
{
    if (!_initialized) {
        return;
    }

#if defined(AY_DEVICE_USE_SDL2)
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            _windowManager.notifyClosed();
            break;
        case SDL_WINDOWEVENT:
            switch (event.window.event) {
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                _windowManager.notifyResized(event.window.data1, event.window.data2);
                break;
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                _windowManager.notifyFocused(true);
                break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
                _windowManager.notifyFocused(false);
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    }

#elif defined(_WIN32)
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
#endif
}

} // namespace ayt::device
