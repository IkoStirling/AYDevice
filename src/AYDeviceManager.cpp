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

    _keyboardEnabled = config.enableKeyboard;
    _mouseEnabled = config.enableMouse;
    _gamepadEnabled = config.enableGamepad;

    _mapping.setKeyboard(_keyboardEnabled ? &_keyboard : nullptr);
    _mapping.setMouse(_mouseEnabled ? &_mouse : nullptr);
    _mapping.setGamepad(_gamepadEnabled ? &_gamepads[0] : nullptr);

    wireInputCallbacks();

    _initialized = true;
    return true;
}

void DeviceManager::wireInputCallbacks()
{
    if (_keyboardEnabled) {
        _windowManager.setKeyCallback([this](KeyCode key, bool pressed) {
            if (pressed) {
                _keyboard.onKeyDown(key);
            } else {
                _keyboard.onKeyUp(key);
            }
        });
    }

    if (_mouseEnabled) {
        _windowManager.setMouseButtonCallback([this](MouseButton button, bool pressed) {
            if (pressed) {
                _mouse.onButtonDown(button);
            } else {
                _mouse.onButtonUp(button);
            }
        });
        _windowManager.setMouseMoveCallback([this](float x, float y) {
            _mouse.onMove(x, y);
        });
        _windowManager.setMouseWheelCallback([this](float delta) {
            _mouse.onWheel(delta);
        });
    }
}

GamepadDevice* DeviceManager::gamepad(int slot)
{
    if (!_gamepadEnabled || slot < 0 || slot >= kMaxGamepads) {
        return nullptr;
    }
    return &_gamepads[slot];
}

const GamepadDevice* DeviceManager::gamepad(int slot) const
{
    if (!_gamepadEnabled || slot < 0 || slot >= kMaxGamepads) {
        return nullptr;
    }
    return &_gamepads[slot];
}

void DeviceManager::shutdown()
{
    if (!_initialized) {
        return;
    }

    _windowManager.destroyWindow();
    _keyboard.reset();
    _mouse.reset();
    for (GamepadDevice& pad : _gamepads) {
        pad.reset();
    }
    _keyboardEnabled = false;
    _mouseEnabled = false;
    _gamepadEnabled = false;
    _initialized = false;
}

void DeviceManager::pollEvents()
{
    if (!_initialized) {
        return;
    }

    // Advance edge state before draining events so just-pressed / deltas are
    // measured relative to the previous frame.
    if (_keyboardEnabled) {
        _keyboard.newFrame();
    }
    if (_mouseEnabled) {
        _mouse.newFrame();
    }
    if (_gamepadEnabled) {
        for (GamepadDevice& pad : _gamepads) {
            pad.newFrame();
        }
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

    // Gamepads are polled (XInput), not event-driven.
    if (_gamepadEnabled) {
        for (GamepadDevice& pad : _gamepads) {
            pad.poll();
        }
    }
}

} // namespace ayt::device
