#pragma once
// AYGamepadDevice.h - Gamepad state via XInput (per-frame edge detection)

#include "AYInputDevice.h"
#include "AYInputTypes.h"

#include <array>
#include <cstdint>

namespace ayt::device {

// A single gamepad slot. On Windows the state is read from XInput in poll();
// the event-feed setters exist for tests and for non-XInput backends.
//
// Frame flow (driven by DeviceManager):
//   1. newFrame() -> snapshot buttons as "previous"
//   2. poll()     -> refresh connected / buttons / axes from XInput
//   3. queries    -> stick/trigger/button state and edges
class GamepadDevice final : public IInputDevice {
public:
    explicit GamepadDevice(int slot = 0);

    // ===== IInputDevice =====
    void newFrame() override;
    bool isConnected() const override { return _connected; }
    const char* deviceType() const override { return "Gamepad"; }

    int slot() const { return _slot; }

    // Refresh state from the platform (XInput on Windows). No-op elsewhere.
    void poll();

    // ===== Analog queries =====
    Vector2 getLeftStick() const { return {_axes[axisIndex(GamepadAxis::LeftX)], _axes[axisIndex(GamepadAxis::LeftY)]}; }
    Vector2 getRightStick() const { return {_axes[axisIndex(GamepadAxis::RightX)], _axes[axisIndex(GamepadAxis::RightY)]}; }
    float getLeftTrigger() const { return _axes[axisIndex(GamepadAxis::LeftTrigger)]; }
    float getRightTrigger() const { return _axes[axisIndex(GamepadAxis::RightTrigger)]; }
    float getAxis(GamepadAxis axis) const { return _axes[axisIndex(axis)]; }

    // ===== Button queries =====
    bool isButtonPressed(GamepadButton button) const;
    bool isButtonJustPressed(GamepadButton button) const;
    bool isButtonJustReleased(GamepadButton button) const;

    // ===== Vibration (0..1 intensities; no-op when disconnected) =====
    void setVibration(float leftMotor, float rightMotor);
    void stopVibration() { setVibration(0.0f, 0.0f); }

    // ===== Event feed (tests / non-XInput backends) =====
    void setConnected(bool connected);
    void onButtonDown(GamepadButton button);
    void onButtonUp(GamepadButton button);
    void setAxis(GamepadAxis axis, float value);
    void reset();

private:
    static int buttonIndex(GamepadButton button);
    static int axisIndex(GamepadAxis axis);

    int  _slot = 0;
    bool _connected = false;

    std::array<bool, kGamepadButtonCount> _current{};
    std::array<bool, kGamepadButtonCount> _previous{};
    std::array<float, kGamepadAxisCount>  _axes{};
};

} // namespace ayt::device
