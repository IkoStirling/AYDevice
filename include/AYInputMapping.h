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

    // M1 (2026-07-15): thin 2-axis wrapper. Convention:
    //   bindAxis2D("move", "move_x", "move_y")
    // declares that the named 2-axis is composed of two already-bound
    // 1-D axes by name. Pure metadata — bindAxis2D does NOT verify
    // xAxis / yAxis exist as 1-axis; getAxis2D falls back through
    // InputMapping::getAxisValue's existing safe-default (0.0f).
    // getAxis2D unbound returns Vector2{} (zero vector). No caching —
    // each query does two map lookups, matching the 1-axis cost.
    //
    // AYScript Logia (M1) consumes this through InputProvider's
    // getAxisValue2D virtual; Editor/Application wiring binds via
    // PlayerController::onStart or similar.
    void bindAxis2D(std::string_view name,
                    std::string_view xAxis,
                    std::string_view yAxis);
    Vector2 getAxis2D(std::string_view name) const;
    bool hasAxis2D(std::string_view name) const;
    void clearAxis2D(std::string_view name);

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

    // M1 (2026-07-15): 2-axis metadata. Stores the names of the two
    // 1-D axes that compose a 2-axis read. getAxis2D dereferences via
    // getAxisValue (which itself reads device state). String-stored so
    // users can bindAxis2D before the underlying bindAxis calls.
    struct Axis2DBinding {
        std::string xAxis;
        std::string yAxis;
    };

    const ActionBinding* findAction(std::string_view action) const;
    const AxisBinding* findAxis(std::string_view axis) const;
    const Axis2DBinding* findAxis2D(std::string_view name) const;

    const KeyboardDevice* _keyboard = nullptr;
    const MouseDevice*    _mouse = nullptr;
    const GamepadDevice*  _gamepad = nullptr;

    std::unordered_map<std::string, ActionBinding> _actions;
    std::unordered_map<std::string, AxisBinding>   _axes;
    std::unordered_map<std::string, Axis2DBinding> _axes2D;
};

} // namespace ayt::device
