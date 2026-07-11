#include "AYInputNames.h"

#include <array>

namespace ayt::device {

namespace {

// Names indexed by KeyCode value (must stay in sync with the enum order).
constexpr std::array<std::string_view, kKeyCodeCount> kKeyNames = {
    "Unknown",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    "Num0", "Num1", "Num2", "Num3", "Num4",
    "Num5", "Num6", "Num7", "Num8", "Num9",
    "F1", "F2", "F3", "F4", "F5", "F6",
    "F7", "F8", "F9", "F10", "F11", "F12",
    "Escape", "Enter", "Tab", "Space", "Backspace",
    "Insert", "Delete", "Home", "End", "PageUp", "PageDown",
    "Left", "Right", "Up", "Down",
    "LeftShift", "RightShift",
    "LeftControl", "RightControl",
    "LeftAlt", "RightAlt",
    "LeftSuper", "RightSuper",
    "Minus", "Equal", "LeftBracket", "RightBracket", "Backslash",
    "Semicolon", "Apostrophe", "Comma", "Period", "Slash", "GraveAccent",
    "Kp0", "Kp1", "Kp2", "Kp3", "Kp4",
    "Kp5", "Kp6", "Kp7", "Kp8", "Kp9",
    "KpDecimal", "KpDivide", "KpMultiply", "KpSubtract", "KpAdd", "KpEnter",
    "CapsLock", "NumLock", "ScrollLock",
};

constexpr std::array<std::string_view, kMouseButtonCount> kMouseNames = {
    "Left", "Right", "Middle", "X1", "X2",
};

constexpr std::array<std::string_view, kGamepadButtonCount> kGamepadButtonNames = {
    "A", "B", "X", "Y",
    "LeftBumper", "RightBumper",
    "Back", "Start", "Guide",
    "LeftStick", "RightStick",
    "DpadUp", "DpadDown", "DpadLeft", "DpadRight",
};

constexpr std::array<std::string_view, kGamepadAxisCount> kGamepadAxisNames = {
    "LeftX", "LeftY", "RightX", "RightY", "LeftTrigger", "RightTrigger",
};

} // namespace

std::string_view keyCodeName(KeyCode key)
{
    const int i = static_cast<int>(key);
    if (i < 0 || i >= kKeyCodeCount) {
        return kKeyNames[static_cast<int>(KeyCode::Unknown)];
    }
    return kKeyNames[i];
}

KeyCode keyCodeFromName(std::string_view name)
{
    for (int i = 0; i < kKeyCodeCount; ++i) {
        if (kKeyNames[i] == name) {
            return static_cast<KeyCode>(i);
        }
    }
    return KeyCode::Unknown;
}

std::string_view mouseButtonName(MouseButton button)
{
    const int i = static_cast<int>(button);
    if (i < 0 || i >= kMouseButtonCount) {
        return kMouseNames[0];
    }
    return kMouseNames[i];
}

bool mouseButtonFromName(std::string_view name, MouseButton& out)
{
    for (int i = 0; i < kMouseButtonCount; ++i) {
        if (kMouseNames[i] == name) {
            out = static_cast<MouseButton>(i);
            return true;
        }
    }
    return false;
}

std::string_view gamepadButtonName(GamepadButton button)
{
    const int i = static_cast<int>(button);
    if (i < 0 || i >= kGamepadButtonCount) {
        return kGamepadButtonNames[0];
    }
    return kGamepadButtonNames[i];
}

bool gamepadButtonFromName(std::string_view name, GamepadButton& out)
{
    for (int i = 0; i < kGamepadButtonCount; ++i) {
        if (kGamepadButtonNames[i] == name) {
            out = static_cast<GamepadButton>(i);
            return true;
        }
    }
    return false;
}

std::string_view gamepadAxisName(GamepadAxis axis)
{
    const int i = static_cast<int>(axis);
    if (i < 0 || i >= kGamepadAxisCount) {
        return kGamepadAxisNames[0];
    }
    return kGamepadAxisNames[i];
}

bool gamepadAxisFromName(std::string_view name, GamepadAxis& out)
{
    for (int i = 0; i < kGamepadAxisCount; ++i) {
        if (kGamepadAxisNames[i] == name) {
            out = static_cast<GamepadAxis>(i);
            return true;
        }
    }
    return false;
}

} // namespace ayt::device
