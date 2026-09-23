#pragma once
// AYDevice/InputMapping.h - Action / Axis abstraction over physical input

#include "AYDevice/InputTypes.h"

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ayt::device {

class KeyboardDevice;

// L11+L12 (2026-08-26): transparent hasher + transparent equality
// functor for heterogeneous unordered_map lookup against std::string
// keys. Why custom types instead of std::hash<std::string_view> +
// std::equal_to<void>:
//
//   - std::hash<std::string_view> in MSVC's STL does NOT expose
//     `is_transparent`. Its specialization only hashes
//     `basic_string_view<_Elem>` itself, never other key-like
//     types. So MSVC's transparent find/erase overload is gated
//     off via `_Has_transparent_overloads = false` in
//     xhash and the call falls back to the non-transparent
//     overload that requires a `const std::string&`.
//
//   - std::equal_to<void> DOES expose `is_transparent` (it's the
//     primary template specialization), so it works on MSVC for
//     the key_equal half.
//
// To unify the three STLs (MSVC / libstdc++ / libc++) we define our
// own pair. The hash delegates to std::hash<std::string_view> for
// the byte-content hash (so the underlying hash is identical to
// std::hash<std::string> for equal content). The struct is
// is_transparent so MSVC's `_Transparent` concept accepts it.
struct TransparentStringHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view text) const noexcept {
        return std::hash<std::string_view>{}(text);
    }
    std::size_t operator()(const std::string& text) const noexcept {
        return std::hash<std::string>{}(text);
    }
    std::size_t operator()(const char* text) const noexcept {
        return std::hash<std::string_view>{}(text);
    }
};

struct TransparentStringEqual {
    using is_transparent = void;
    bool operator()(std::string_view a, std::string_view b) const noexcept {
        return a == b;
    }
    bool operator()(const std::string& a, std::string_view b) const noexcept {
        return a == b;
    }
    bool operator()(std::string_view a, const std::string& b) const noexcept {
        return a == b;
    }
    bool operator()(const std::string& a, const std::string& b) const noexcept {
        return a == b;
    }
    bool operator()(const char* a, const std::string& b) const noexcept {
        return b == a;
    }
    bool operator()(const std::string& a, const char* b) const noexcept {
        return a == b;
    }
};
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

    /**
     * @brief Reports whether any physical source bound to an Action is held.
     * @param action Stable logical Action name from the active input profile.
     * @return true while at least one bound key, button, or gamepad source is held.
     * @framephase
     * Device polling must complete before observing the current frame.
     * @threading
     * Call from the game/UI thread that owns DeviceManager polling.
     */
    bool isActionPressed(std::string_view action) const;
    bool isActionJustPressed(std::string_view action) const;
    bool isActionJustReleased(std::string_view action) const;

    // Deterministic fixed-tick snapshot. Platform captures input for the next
    // sim tick; multiple render frames targeting the same tick merge edges.
    // Catch-up ticks reuse held/axis values but consume press/release edges
    // exactly once.
    void captureTickInputFrame(uint64_t targetSimTick);
    void beginSimulationTick(uint64_t simTick);
    bool isTickActionPressed(std::string_view action) const;
    bool isTickActionJustPressed(std::string_view action) const;
    bool isTickActionJustReleased(std::string_view action) const;
    float getTickAxisValue(std::string_view axis) const;
    uint64_t getCurrentInputSimTick() const { return _currentInputSimTick; }

    // ===== Axis binding (continuous -1..1) =====
    struct KeyPair {
        KeyCode negative = KeyCode::Unknown;
        KeyCode positive = KeyCode::Unknown;
    };

    void bindAxis(std::string_view axis, std::span<const KeyPair> pairs, float scale = 1.0f);
    // Bind a gamepad analog axis as an additional source for the named axis.
    void bindAxisGamepad(std::string_view axis, GamepadAxis gamepadAxis, float scale = 1.0f);
    void clearAxis(std::string_view axis);

    // L13 (2026-08-26): composite value is clamped to [-1, 1] after the
    // keyboard + gamepad sum. Callers that bind a single axis source can
    // exceed ±1 by setting a per-source scale > 1 (e.g. bindAxis
    // scale=1.5); the final clamp is the consumer's responsibility only
    // when they explicitly opt out via bindAxisRaw. In all other cases
    // the returned value is in [-1, 1].
    /**
     * @brief Reads the aggregated value of a logical input Axis.
     * @param axis Stable logical Axis name from the active input profile.
     * @return Sum of bound sources clamped to [-1, 1], or 0 when unbound.
     * @framephase
     * Device polling must complete before observing the current frame.
     * @threading
     * Call from the game/UI thread that owns DeviceManager polling.
     */
    float getAxisValue(std::string_view axis) const;

    bool hasAction(std::string_view action) const;
    bool hasAxis(std::string_view axis) const;

    /// Visit every bound action name (for Device→EventBus edge fan-out).
    template<typename Fn>
    void forEachAction(Fn&& fn) const {
        for (const auto& kv : _actions) {
            fn(std::string_view(kv.first));
        }
    }

    template<typename Fn>
    void forEachAxis(Fn&& fn) const {
        for (const auto& kv : _axes) {
            fn(std::string_view(kv.first));
        }
    }

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

    // L11+L12 (2026-08-26): heterogeneous unordered_map with
    // TransparentStringHash + TransparentStringEqual so lookups
    // don't construct a std::string from string_view on every query.
    // Hot path: isActionPressed / getAxisValue run every frame.
    //
    // Why custom hasher/equal (see also the comments on those
    // structs): MSVC's std::hash<std::string_view> does not expose
    // `is_transparent`, which is the gate MSVC's STL uses to enable
    // the transparent find/erase/contains overloads (see xhash's
    // _Uhash_choose_transparency / _Transparent concepts). The
    // workaround is a tiny wrapper that exposes `is_transparent` and
    // delegates to std::hash<std::string_view>/std::hash<std::string>.
    using ActionMap = std::unordered_map<std::string, ActionBinding,
                                         TransparentStringHash,
                                         TransparentStringEqual>;
    using AxisMap = std::unordered_map<std::string, AxisBinding,
                                       TransparentStringHash,
                                       TransparentStringEqual>;
    using Axis2DMap = std::unordered_map<std::string, Axis2DBinding,
                                         TransparentStringHash,
                                         TransparentStringEqual>;
    ActionMap _actions;
    AxisMap   _axes;
    Axis2DMap _axes2D;

    struct TickActionState {
        bool pressed = false;
        bool justPressed = false;
        bool justReleased = false;
    };
    using TickActionMap = std::unordered_map<std::string, TickActionState,
                                             TransparentStringHash,
                                             TransparentStringEqual>;
    using TickAxisMap = std::unordered_map<std::string, float,
                                           TransparentStringHash,
                                           TransparentStringEqual>;
    TickActionMap _pendingTickActions;
    TickAxisMap   _pendingTickAxes;
    TickActionMap _currentTickActions;
    TickAxisMap   _currentTickAxes;
    uint64_t _pendingInputSimTick = 0;
    uint64_t _currentInputSimTick = 0;
};

} // namespace ayt::device
