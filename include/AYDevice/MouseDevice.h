#pragma once
// AYDevice/MouseDevice.h - Mouse position, buttons, and wheel with per-frame edges

#include "AYDevice/InputDevice.h"
#include "AYDevice/InputTypes.h"

#include <array>

namespace ayt::device {

// Tracks cursor position (window client space), button state, and wheel.
//
// Event flow per frame:
//   1. newFrame()             -> snapshot buttons, clear per-frame deltas
//   2. onMove/onButton/onWheel-> platform feeds this frame's events
//   3. queries                -> position / delta / isButtonPressed / ...
class MouseDevice final : public IInputDevice {
public:
    MouseDevice();

    // ===== IInputDevice =====
    void newFrame() override;
    bool isConnected() const override { return true; }
    const char* deviceType() const override { return "Mouse"; }

    // ===== Event feed (called by the platform layer) =====
    void onMove(float x, float y);
    void onRelativeMove(float deltaX, float deltaY);
    void onButtonDown(MouseButton button);
    void onButtonUp(MouseButton button);
    void onWheel(float delta);
    // Release held buttons and discard motion continuity while retaining the
    // previous-frame snapshot for just-released edges.
    void releaseAllButtons();
    void reset();

    // ===== Queries =====
    Vector2 getPosition() const { return _position; }
    Vector2 getDelta() const { return _delta; }        // movement this frame
    float getWheelDelta() const { return _wheelDelta; } // scroll this frame

    bool isButtonPressed(MouseButton button) const;
    bool isButtonJustPressed(MouseButton button) const;
    bool isButtonJustReleased(MouseButton button) const;

private:
    static int index(MouseButton button);

    Vector2 _position{};
    Vector2 _delta{};
    float   _wheelDelta = 0.0f;
    bool    _hasLastPosition = false;

    std::array<bool, kMouseButtonCount> _current{};
    std::array<bool, kMouseButtonCount> _previous{};
};

} // namespace ayt::device
