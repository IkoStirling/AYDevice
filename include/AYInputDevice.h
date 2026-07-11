#pragma once
// AYInputDevice.h - Base interface for pollable input devices

namespace ayt::device {

// Common contract for keyboard / mouse / gamepad / etc.
// Devices are event-driven: the platform layer feeds raw events in, and
// newFrame() advances per-frame edge state (just-pressed / just-released,
// deltas) once at the start of every frame.
class IInputDevice {
public:
    virtual ~IInputDevice() = default;

    // Advance edge state for a new frame. Called once per frame before the
    // platform message pump delivers this frame's events.
    virtual void newFrame() = 0;

    virtual bool isConnected() const = 0;
    virtual const char* deviceType() const = 0;
};

} // namespace ayt::device
