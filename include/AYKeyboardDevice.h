#pragma once
// AYKeyboardDevice.h - Keyboard state with per-frame edge detection

#include "AYInputDevice.h"
#include "AYInputTypes.h"

#include <array>

namespace ayt::device {

// Tracks pressed state for every KeyCode plus this-frame edges.
//
// Event flow per frame:
//   1. newFrame()            -> snapshot current state as "previous"
//   2. onKeyDown/onKeyUp ... -> platform feeds this frame's key events
//   3. queries               -> isKeyPressed / isKeyJustPressed / ...
class KeyboardDevice final : public IInputDevice {
public:
    KeyboardDevice();

    // ===== IInputDevice =====
    void newFrame() override;
    bool isConnected() const override { return true; }
    const char* deviceType() const override { return "Keyboard"; }

    // ===== Event feed (called by the platform layer) =====
    void onKeyDown(KeyCode key);
    void onKeyUp(KeyCode key);
    void reset();

    // ===== Queries =====
    bool isKeyPressed(KeyCode key) const;
    bool isKeyJustPressed(KeyCode key) const;   // down this frame, up last frame
    bool isKeyJustReleased(KeyCode key) const;  // up this frame, down last frame

private:
    static int index(KeyCode key);

    std::array<bool, kKeyCodeCount> _current{};
    std::array<bool, kKeyCodeCount> _previous{};
};

} // namespace ayt::device
