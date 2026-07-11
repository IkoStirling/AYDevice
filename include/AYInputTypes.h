#pragma once
// AYInputTypes.h - Input device value types (keys, buttons, vectors)

#include <cstdint>
#include <functional>

namespace ayt::device {

// Lightweight 2D vector for cursor / wheel / axis values.
// AYDevice stays free of the SSE-heavy ayt::math::FVector2 dependency.
struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;
};

// Physical keyboard keys. Values follow USB HID usage where convenient, but the
// enum is backend-agnostic: Win32 / SDL2 translate their native codes into these.
enum class KeyCode : uint16_t {
    Unknown = 0,

    // Letters
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // Digits (top row)
    Num0, Num1, Num2, Num3, Num4,
    Num5, Num6, Num7, Num8, Num9,

    // Function keys
    F1, F2, F3, F4, F5, F6,
    F7, F8, F9, F10, F11, F12,

    // Control / whitespace
    Escape, Enter, Tab, Space, Backspace,
    Insert, Delete, Home, End, PageUp, PageDown,

    // Arrows
    Left, Right, Up, Down,

    // Modifiers
    LeftShift, RightShift,
    LeftControl, RightControl,
    LeftAlt, RightAlt,
    LeftSuper, RightSuper,

    // Punctuation
    Minus, Equal, LeftBracket, RightBracket, Backslash,
    Semicolon, Apostrophe, Comma, Period, Slash, GraveAccent,

    // Keypad
    Kp0, Kp1, Kp2, Kp3, Kp4,
    Kp5, Kp6, Kp7, Kp8, Kp9,
    KpDecimal, KpDivide, KpMultiply, KpSubtract, KpAdd, KpEnter,

    // Locks
    CapsLock, NumLock, ScrollLock,

    Count
};

inline constexpr int kKeyCodeCount = static_cast<int>(KeyCode::Count);

// Mouse buttons.
enum class MouseButton : uint8_t {
    Left = 0,
    Right,
    Middle,
    X1,
    X2,

    Count
};

inline constexpr int kMouseButtonCount = static_cast<int>(MouseButton::Count);

// Raw input callbacks emitted by WindowManager as the platform pump translates
// native events. DeviceManager wires these into the concrete devices.
using KeyCallback = std::function<void(KeyCode key, bool pressed)>;
using MouseButtonCallback = std::function<void(MouseButton button, bool pressed)>;
using MouseMoveCallback = std::function<void(float x, float y)>;
using MouseWheelCallback = std::function<void(float delta)>;

} // namespace ayt::device
