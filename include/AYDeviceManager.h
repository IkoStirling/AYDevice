#pragma once

#include "AYWindowManager.h"
#include "AYWindowTypes.h"

namespace ayt::device {

struct DeviceConfig {
    WindowCreateInfo window;
    bool enableKeyboard = true;
    bool enableMouse = true;
    bool enableGamepad = true;
    bool enableTouch = false;
    bool enableXR = false;
};

// Phase-1: window + event pump only. Input devices deferred.
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

    void pollEvents();

private:
    bool _initialized = false;
    WindowManager _windowManager;
};

} // namespace ayt::device
