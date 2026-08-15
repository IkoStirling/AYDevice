#pragma once

#include "AYDevice/WindowManager.h"
#include "AYDevice/WindowTypes.h"
#include "AYDevice/KeyboardDevice.h"
#include "AYDevice/MouseDevice.h"
#include "AYDevice/GamepadDevice.h"
#include "AYDevice/TouchDevice.h"
#include "AYDevice/TextInput.h"
#include "AYDevice/InputMapping.h"

#include <array>

namespace ayt::device {

struct DeviceConfig {
    WindowCreateInfo window;
    bool enableKeyboard = true;
    bool enableMouse = true;
    bool enableGamepad = true;
    bool enableTouch = false;
    bool enableXR = false;
};

// Phase-2: keyboard/mouse + action/axis mapping.
// Phase-3: gamepads (XInput), touch (WM_TOUCH), and text/IME (WM_CHAR / IME).
class DeviceManager {
public:
    DeviceManager();
    ~DeviceManager();

    DeviceManager(const DeviceManager&) = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;

    bool initialize(const DeviceConfig& config);
    void shutdown();
    bool isInitialized() const { return _initialized; }

    WindowManager& window() { return _windowManager; }
    const WindowManager& window() const { return _windowManager; }

    // Input devices (null when disabled in DeviceConfig).
    KeyboardDevice* keyboard() { return _keyboardEnabled ? &_keyboard : nullptr; }
    const KeyboardDevice* keyboard() const { return _keyboardEnabled ? &_keyboard : nullptr; }
    MouseDevice* mouse() { return _mouseEnabled ? &_mouse : nullptr; }
    const MouseDevice* mouse() const { return _mouseEnabled ? &_mouse : nullptr; }

    // Gamepad by slot (0..kMaxGamepads-1). Returns the slot even when
    // disconnected; check isConnected(). Null when gamepads are disabled or
    // the slot is out of range.
    GamepadDevice* gamepad(int slot = 0);
    const GamepadDevice* gamepad(int slot = 0) const;

    // Touch device (null when enableTouch was false).
    TouchDevice* touch() { return _touchEnabled ? &_touch : nullptr; }
    const TouchDevice* touch() const { return _touchEnabled ? &_touch : nullptr; }

    // Text/IME input. Always present; disabled by default — call
    // textInput().setEnabled(true) when a text field takes focus.
    TextInput& textInput() { return _textInput; }
    const TextInput& textInput() const { return _textInput; }

    InputMapping& mapping() { return _mapping; }
    const InputMapping& mapping() const { return _mapping; }

    // Advance per-frame edge state, drain the platform message pump, then poll
    // gamepads. Call once per frame (edges are relative to the previous call).
    void pollEvents();

    // Convenience action/axis queries (forward to the mapping).
    bool isActionPressed(const char* action) const { return _mapping.isActionPressed(action); }
    float getAxisValue(const char* axis) const { return _mapping.getAxisValue(axis); }

private:
    void wireInputCallbacks();

    bool _initialized = false;
    bool _keyboardEnabled = false;
    bool _mouseEnabled = false;
    bool _gamepadEnabled = false;
    bool _touchEnabled = false;

    WindowManager  _windowManager;
    KeyboardDevice _keyboard;
    MouseDevice    _mouse;
    std::array<GamepadDevice, kMaxGamepads> _gamepads{
        GamepadDevice{0}, GamepadDevice{1}, GamepadDevice{2}, GamepadDevice{3}};
    TouchDevice    _touch;
    TextInput      _textInput;
    InputMapping   _mapping;
};

} // namespace ayt::device
