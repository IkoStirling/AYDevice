#pragma once
// AYInputMapping.h - Action / Axis abstraction over physical input

#include "AYInputTypes.h"

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ayt::device {

class KeyboardDevice;
class MouseDevice;
class GamepadDevice;

// Decouples game logic from physical keys:
//   mapping.bindAction("Jump", {KeyCode::Space});
//   mapping.isActionPressed("Jump");
//
// Binds to keyboard keys, mouse buttons, and gamepad buttons/axes. All sources
// bound to an Action are OR'd; all sources bound to an Axis are summed then
// clamped. XR sources are added in a later phase without changing the query API.
class InputMapping {
public:
    // Devices the mapping reads from. Any may be null.
    void setKeyboard(const KeyboardDevice* keyboard) { _keyboard = keyboard; }
    void setMouse(const MouseDevice* mouse) { _mouse = mouse; }
    void setGamepad(const GamepadDevice* gamepad) { _gamepad = gamepad; }

    // ===== Action binding (binary press/release) =====
    void bindAction(std::string_view action, std::span<const KeyCode> keys);
    void bindActionMouse(std::string_view action, std::span<const MouseButton> buttons);
    void bindActionGamepad(std::string_view action, std::span<const GamepadButton> buttons);
    void clearAction(std::string_view action);

    bool isActionPressed(std::string_view action) const;
    bool isActionJustPressed(std::string_view action) const;
    bool isActionJustReleased(std::string_view action) const;

    // ===== Axis binding (continuous -1..1) =====
    struct KeyPair {
        KeyCode negative = KeyCode::Unknown;
        KeyCode positive = KeyCode::Unknown;
    };

    void bindAxis(std::string_view axis, std::span<const KeyPair> pairs, float scale = 1.0f);
    // Bind a gamepad analog axis as an additional source for the named axis.
    void bindAxisGamepad(std::string_view axis, GamepadAxis gamepadAxis, float scale = 1.0f);
    void clearAxis(std::string_view axis);

    float getAxisValue(std::string_view axis) const;

    bool hasAction(std::string_view action) const;
    bool hasAxis(std::string_view axis) const;

private:
    struct ActionBinding {
        std::vector<KeyCode>       keys;
        std::vector<MouseButton>   buttons;
        std::vector<GamepadButton> gamepadButtons;
    };

    struct GamepadAxisSource {
        GamepadAxis axis = GamepadAxis::LeftX;
        float       scale = 1.0f;
    };

    struct AxisBinding {
        std::vector<KeyPair>          pairs;
        float                         scale = 1.0f;
        std::vector<GamepadAxisSource> gamepadAxes;
    };

    const ActionBinding* findAction(std::string_view action) const;
    const AxisBinding* findAxis(std::string_view axis) const;

    const KeyboardDevice* _keyboard = nullptr;
    const MouseDevice*    _mouse = nullptr;
    const GamepadDevice*  _gamepad = nullptr;

    std::unordered_map<std::string, ActionBinding> _actions;
    std::unordered_map<std::string, AxisBinding>   _axes;
};

} // namespace ayt::device
