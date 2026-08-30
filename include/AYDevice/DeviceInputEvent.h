#pragma once

#include "AYDevice/InputTypes.h"

#include <cstdint>
#include <functional>
#include <string>

namespace ayt::device {

// Ordered, backend-neutral input emitted by DeviceManager while it pumps the
// platform queue. Consumers subscribe to DeviceManager rather than replacing
// WindowManager's state-maintenance callbacks or decoding native messages.
enum class DeviceInputEventType : uint8_t {
    Key,
    MouseMove,
    MouseDelta,
    MouseButton,
    MouseWheel,
    MouseLeave,
    Touch,
    TextCommit,
    CompositionStart,
    CompositionUpdate,
    CompositionEnd,
};

// Windows can expose the same physical wheel gesture through more than one
// message family. DeviceManager keeps only the highest-priority source seen in
// a frame (Standard > Pointer > RawInput) before notifying UI consumers.
enum class MouseWheelSource : uint8_t {
    RawInput,
    Pointer,
    Standard,
};

struct DeviceInputEvent {
    DeviceInputEventType type = DeviceInputEventType::MouseMove;

    KeyCode key = KeyCode::Unknown;
    MouseButton mouseButton = MouseButton::Left;
    TouchPhase touchPhase = TouchPhase::Ended;
    MouseWheelSource wheelSource = MouseWheelSource::Standard;

    bool pressed = false;
    bool repeat = false;

    // Main-window client coordinates for absolute pointer/touch events.
    float x = 0.0f;
    float y = 0.0f;
    // Relative motion or wheel values. Wheel delta is normalized to notches;
    // AYUI performs the policy conversion from notches to logical pixels.
    float deltaX = 0.0f;
    float deltaY = 0.0f;

    int64_t pointerId = -1;
    std::string text;
    int compositionCursor = 0;
};

using DeviceInputEventCallback =
    std::function<void(const DeviceInputEvent& event)>;
using DeviceInputListenerId = uint64_t;

} // namespace ayt::device
