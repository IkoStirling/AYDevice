#pragma once
// AYInputNames.h - Stable string names for input enums (profile serialization)

#include "AYInputTypes.h"

#include <string>
#include <string_view>

namespace ayt::device {

// Human-readable, version-stable names for physical inputs. Used by InputProfile
// so saved configs survive enum reordering and can be hand-edited.
//
// Token conventions (see AYInputProfile):
//   keyboard key   -> "Space", "A", "F1"          (bare KeyCode name)
//   mouse button   -> "Mouse:Left"
//   gamepad button -> "Pad:A"
//   gamepad axis   -> "PadAxis:LeftX"

// KeyCode <-> name (bare, e.g. "Space"). Unknown maps to "Unknown" / KeyCode::Unknown.
std::string_view keyCodeName(KeyCode key);
KeyCode keyCodeFromName(std::string_view name);

// MouseButton <-> bare name (e.g. "Left"). Out-of-range -> "Left" / MouseButton::Left.
std::string_view mouseButtonName(MouseButton button);
bool mouseButtonFromName(std::string_view name, MouseButton& out);

// GamepadButton <-> bare name (e.g. "A", "DpadUp").
std::string_view gamepadButtonName(GamepadButton button);
bool gamepadButtonFromName(std::string_view name, GamepadButton& out);

// GamepadAxis <-> bare name (e.g. "LeftX").
std::string_view gamepadAxisName(GamepadAxis axis);
bool gamepadAxisFromName(std::string_view name, GamepadAxis& out);

} // namespace ayt::device
