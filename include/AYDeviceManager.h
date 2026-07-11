#pragma once

#include "AYWindowManager.h"
#include "AYWindowTypes.h"
#include "AYKeyboardDevice.h"
#include "AYMouseDevice.h"
#include "AYInputMapping.h"

namespace ayt::device {

struct DeviceConfig {
    WindowCreateInfo window;
    bool enableKeyboard = true;
    bool enableMouse = true;
    bool enableGamepad = true;
    bool enableTouch = false;
    bool enableXR = false;
};

// Phase-2: window + event pump + keyboard/mouse + action/axis mapping.
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

    InputMapping& mapping() { return _mapping; }
    const InputMapping& mapping() const { return _mapping; }

    // Advance per-frame edge state, then drain the platform message pump.
    // Call once per frame (edges are relative to the previous pollEvents()).
    void pollEvents();

    // Convenience action/axis queries (forward to the mapping).
    bool isActionPressed(const char* action) const { return _mapping.isActionPressed(action); }
    float getAxisValue(const char* axis) const { return _mapping.getAxisValue(axis); }

private:
    void wireInputCallbacks();

    bool _initialized = false;
    bool _keyboardEnabled = false;
    bool _mouseEnabled = false;

    WindowManager  _windowManager;
    KeyboardDevice _keyboard;
    MouseDevice    _mouse;
    InputMapping   _mapping;
};

} // namespace ayt::device
