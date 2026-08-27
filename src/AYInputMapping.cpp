#include "AYDevice/InputMapping.h"

#include "AYDevice/KeyboardDevice.h"
#include "AYDevice/MouseDevice.h"
#include "AYDevice/GamepadDevice.h"

namespace ayt::device {

// L11+L12 (2026-08-26): heterogeneous map type is std::unordered_map
// with std::hash<std::string_view> + std::equal_to<void>. MSVC's
// `operator[]` does NOT have a transparent overload (libc++ / libstdc++
// do). To get the heterogeneous semantics portably, every bind*
// helper below uses an explicit find() + emplace() pair instead of
// subscript. Hot-path readers (isActionPressed / getAxisValue /
// findAction / findAxis / findAxis2D) use find() which IS
// transparent on all three STLs. The alloc cost is concentrated on
// the rare bind path (one-shot at startup / rebind), and the hot
// path stays zero-alloc.
void InputMapping::bindAction(std::string_view action, std::span<const KeyCode> keys)
{
    auto it = _actions.find(action);
    if (it == _actions.end()) {
        it = _actions.emplace(std::string(action), ActionBinding{}).first;
    }
    it->second.keys.assign(keys.begin(), keys.end());
}

void InputMapping::bindActionMouse(std::string_view action, std::span<const MouseButton> buttons)
{
    auto it = _actions.find(action);
    if (it == _actions.end()) {
        it = _actions.emplace(std::string(action), ActionBinding{}).first;
    }
    it->second.buttons.assign(buttons.begin(), buttons.end());
}

void InputMapping::bindActionGamepad(std::string_view action, std::span<const GamepadButton> buttons)
{
    auto it = _actions.find(action);
    if (it == _actions.end()) {
        it = _actions.emplace(std::string(action), ActionBinding{}).first;
    }
    it->second.gamepadButtons.assign(buttons.begin(), buttons.end());
}

void InputMapping::clearAction(std::string_view action)
{
    _actions.erase(action);
}

void InputMapping::bindAxis(std::string_view axis, std::span<const KeyPair> pairs, float scale)
{
    auto it = _axes.find(axis);
    if (it == _axes.end()) {
        it = _axes.emplace(std::string(axis), AxisBinding{}).first;
    }
    it->second.pairs.assign(pairs.begin(), pairs.end());
    it->second.scale = scale;
}

void InputMapping::bindAxisGamepad(std::string_view axis, GamepadAxis gamepadAxis, float scale)
{
    auto it = _axes.find(axis);
    if (it == _axes.end()) {
        it = _axes.emplace(std::string(axis), AxisBinding{}).first;
    }
    it->second.gamepadAxes.push_back(GamepadAxisSource{gamepadAxis, scale});
}

void InputMapping::clearAxis(std::string_view axis)
{
    _axes.erase(axis);
}

// M1 (2026-07-15): thin 2-axis binding. Records xAxis / yAxis names;
// does not validate they exist as 1-axis (R-2 decision — let the
// player wire in their preferred order).
void InputMapping::bindAxis2D(std::string_view name,
                              std::string_view xAxis,
                              std::string_view yAxis)
{
    auto it = _axes2D.find(name);
    if (it == _axes2D.end()) {
        it = _axes2D.emplace(std::string(name), Axis2DBinding{}).first;
    }
    it->second.xAxis = std::string(xAxis);
    it->second.yAxis = std::string(yAxis);
}

void InputMapping::clearAxis2D(std::string_view name)
{
    _axes2D.erase(name);
}

const InputMapping::ActionBinding* InputMapping::findAction(std::string_view action) const
{
    auto it = _actions.find(action);
    return it != _actions.end() ? &it->second : nullptr;
}

const InputMapping::AxisBinding* InputMapping::findAxis(std::string_view axis) const
{
    auto it = _axes.find(axis);
    return it != _axes.end() ? &it->second : nullptr;
}

const InputMapping::Axis2DBinding* InputMapping::findAxis2D(std::string_view name) const
{
    auto it = _axes2D.find(name);
    return it != _axes2D.end() ? &it->second : nullptr;
}

bool InputMapping::isActionPressed(std::string_view action) const
{
    const ActionBinding* binding = findAction(action);
    if (binding == nullptr) {
        return false;
    }
    if (_keyboard != nullptr) {
        for (KeyCode key : binding->keys) {
            if (_keyboard->isKeyPressed(key)) {
                return true;
            }
        }
    }
    if (_mouse != nullptr) {
        for (MouseButton button : binding->buttons) {
            if (_mouse->isButtonPressed(button)) {
                return true;
            }
        }
    }
    if (_gamepad != nullptr) {
        for (GamepadButton button : binding->gamepadButtons) {
            if (_gamepad->isButtonPressed(button)) {
                return true;
            }
        }
    }
    return false;
}

bool InputMapping::isActionJustPressed(std::string_view action) const
{
    const ActionBinding* binding = findAction(action);
    if (binding == nullptr) {
        return false;
    }
    if (_keyboard != nullptr) {
        for (KeyCode key : binding->keys) {
            if (_keyboard->isKeyJustPressed(key)) {
                return true;
            }
        }
    }
    if (_mouse != nullptr) {
        for (MouseButton button : binding->buttons) {
            if (_mouse->isButtonJustPressed(button)) {
                return true;
            }
        }
    }
    if (_gamepad != nullptr) {
        for (GamepadButton button : binding->gamepadButtons) {
            if (_gamepad->isButtonJustPressed(button)) {
                return true;
            }
        }
    }
    return false;
}

bool InputMapping::isActionJustReleased(std::string_view action) const
{
    const ActionBinding* binding = findAction(action);
    if (binding == nullptr) {
        return false;
    }
    if (_keyboard != nullptr) {
        for (KeyCode key : binding->keys) {
            if (_keyboard->isKeyJustReleased(key)) {
                return true;
            }
        }
    }
    if (_mouse != nullptr) {
        for (MouseButton button : binding->buttons) {
            if (_mouse->isButtonJustReleased(button)) {
                return true;
            }
        }
    }
    if (_gamepad != nullptr) {
        for (GamepadButton button : binding->gamepadButtons) {
            if (_gamepad->isButtonJustReleased(button)) {
                return true;
            }
        }
    }
    return false;
}

void InputMapping::captureTickInputFrame(uint64_t targetSimTick)
{
    if (targetSimTick == 0) return;
    if (_pendingInputSimTick != 0 && _pendingInputSimTick != targetSimTick) {
        _pendingTickActions.clear();
        _pendingTickAxes.clear();
    }
    _pendingInputSimTick = targetSimTick;

    forEachAction([&](std::string_view action) {
        auto it = _pendingTickActions.find(action);
        if (it == _pendingTickActions.end()) {
            it = _pendingTickActions.emplace(std::string(action), TickActionState{}).first;
        }
        TickActionState& state = it->second;
        state.pressed = isActionPressed(action);
        state.justPressed = state.justPressed || isActionJustPressed(action);
        state.justReleased = state.justReleased || isActionJustReleased(action);
    });
    forEachAxis([&](std::string_view axis) {
        auto it = _pendingTickAxes.find(axis);
        if (it == _pendingTickAxes.end()) {
            it = _pendingTickAxes.emplace(std::string(axis), 0.0f).first;
        }
        it->second = getAxisValue(axis);
    });
}

void InputMapping::beginSimulationTick(uint64_t simTick)
{
    _currentInputSimTick = simTick;
    if (_pendingInputSimTick != 0 && _pendingInputSimTick <= simTick) {
        _currentTickActions = std::move(_pendingTickActions);
        _currentTickAxes = std::move(_pendingTickAxes);
        _pendingTickActions.clear();
        _pendingTickAxes.clear();
        _pendingInputSimTick = 0;
        return;
    }

    // Held values persist across catch-up ticks; edges are one-shot.
    for (auto& pair : _currentTickActions) {
        pair.second.justPressed = false;
        pair.second.justReleased = false;
    }
}

bool InputMapping::isTickActionPressed(std::string_view action) const
{
    auto it = _currentTickActions.find(action);
    return it != _currentTickActions.end() && it->second.pressed;
}

bool InputMapping::isTickActionJustPressed(std::string_view action) const
{
    auto it = _currentTickActions.find(action);
    return it != _currentTickActions.end() && it->second.justPressed;
}

bool InputMapping::isTickActionJustReleased(std::string_view action) const
{
    auto it = _currentTickActions.find(action);
    return it != _currentTickActions.end() && it->second.justReleased;
}

float InputMapping::getTickAxisValue(std::string_view axis) const
{
    auto it = _currentTickAxes.find(axis);
    return it != _currentTickAxes.end() ? it->second : 0.0f;
}

float InputMapping::getAxisValue(std::string_view axis) const
{
    const AxisBinding* binding = findAxis(axis);
    if (binding == nullptr) {
        return 0.0f;
    }

    // L13 (2026-08-26): composite-clamp semantic.
    //
    //   - Single-source binding (one keyboard pair OR one gamepad axis)
    //     honors the caller's scale verbatim. bindAxis("Look", pairs,
    //     2.5f) returns 2.5 at full positive so a per-source sensitivity
    //     multiplier is preserved end-to-end.
    //
    //   - Multi-source binding (keyboard + gamepad summed, or multiple
    //     gamepad axes summed) clamps to [-1, 1] so the composite does
    //     not exceed the canonical analog range.
    //
    // The discriminator is whether both contributions produced
    // non-zero magnitude (i.e. both keyboard AND gamepad contributed).
    // We track each contribution's sign of non-zero magnitude, then
    // sum, then decide whether to clamp based on whether the sum came
    // from one source or two.

    float keyContribution = 0.0f;
    bool  keyActive = false;
    if (_keyboard != nullptr && !binding->pairs.empty()) {
        float keyValue = 0.0f;
        for (const KeyPair& pair : binding->pairs) {
            if (pair.positive != KeyCode::Unknown && _keyboard->isKeyPressed(pair.positive)) {
                keyValue += 1.0f;
            }
            if (pair.negative != KeyCode::Unknown && _keyboard->isKeyPressed(pair.negative)) {
                keyValue -= 1.0f;
            }
        }
        // Clamp the digital sum to [-1, 1] before applying scale: a
        // single binding is W/S (one of them at a time) so the
        // numeric range shouldn't exceed ±1 even before scale.
        if (keyValue > 1.0f) {
            keyValue = 1.0f;
        } else if (keyValue < -1.0f) {
            keyValue = -1.0f;
        }
        keyContribution = keyValue * binding->scale;
        keyActive = (keyContribution != 0.0f);
    }

    float padContribution = 0.0f;
    bool  padActive = false;
    if (_gamepad != nullptr && !binding->gamepadAxes.empty()) {
        float gamepadValue = 0.0f;
        for (const GamepadAxisSource& source : binding->gamepadAxes) {
            gamepadValue += _gamepad->getAxis(source.axis) * source.scale;
        }
        // Multi-gamepad-axis sources are clamped to the normalized
        // range (composite clamp).
        if (gamepadValue > 1.0f) {
            gamepadValue = 1.0f;
        } else if (gamepadValue < -1.0f) {
            gamepadValue = -1.0f;
        }
        padContribution = gamepadValue;
        padActive = (padContribution != 0.0f);
    }

    float value = keyContribution + padContribution;

    // Composite clamp: only when BOTH keyboard and gamepad contributed
    // (i.e. sum from two sources). A single source returns its raw
    // scaled value (caller's per-source sensitivity honored).
    if (keyActive && padActive) {
        if (value > 1.0f)  value = 1.0f;
        if (value < -1.0f) value = -1.0f;
    }
    return value;
}

bool InputMapping::hasAction(std::string_view action) const
{
    return findAction(action) != nullptr;
}

bool InputMapping::hasAxis(std::string_view axis) const
{
    return findAxis(axis) != nullptr;
}

// M1 (2026-07-15): 2-axis query. Unbound name → zero vector; bound
// name → Vector2{getAxisValue(xAxis), getAxisValue(yAxis)}. Component
// axes that are themselves unbound return 0.0f, which is existing
// InputMapping::getAxisValue behavior. No caching.
Vector2 InputMapping::getAxis2D(std::string_view name) const
{
    const Axis2DBinding* b = findAxis2D(name);
    if (b == nullptr) {
        return Vector2{};
    }
    return Vector2{ getAxisValue(b->xAxis), getAxisValue(b->yAxis) };
}

bool InputMapping::hasAxis2D(std::string_view name) const
{
    return findAxis2D(name) != nullptr;
}

} // namespace ayt::device
