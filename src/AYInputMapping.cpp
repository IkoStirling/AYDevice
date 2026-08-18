#include "AYDevice/InputMapping.h"

#include "AYDevice/KeyboardDevice.h"
#include "AYDevice/MouseDevice.h"
#include "AYDevice/GamepadDevice.h"

namespace ayt::device {

void InputMapping::bindAction(std::string_view action, std::span<const KeyCode> keys)
{
    ActionBinding& binding = _actions[std::string(action)];
    binding.keys.assign(keys.begin(), keys.end());
}

void InputMapping::bindActionMouse(std::string_view action, std::span<const MouseButton> buttons)
{
    ActionBinding& binding = _actions[std::string(action)];
    binding.buttons.assign(buttons.begin(), buttons.end());
}

void InputMapping::bindActionGamepad(std::string_view action, std::span<const GamepadButton> buttons)
{
    ActionBinding& binding = _actions[std::string(action)];
    binding.gamepadButtons.assign(buttons.begin(), buttons.end());
}

void InputMapping::clearAction(std::string_view action)
{
    _actions.erase(std::string(action));
}

void InputMapping::bindAxis(std::string_view axis, std::span<const KeyPair> pairs, float scale)
{
    AxisBinding& binding = _axes[std::string(axis)];
    binding.pairs.assign(pairs.begin(), pairs.end());
    binding.scale = scale;
}

void InputMapping::bindAxisGamepad(std::string_view axis, GamepadAxis gamepadAxis, float scale)
{
    AxisBinding& binding = _axes[std::string(axis)];
    binding.gamepadAxes.push_back(GamepadAxisSource{gamepadAxis, scale});
}

void InputMapping::clearAxis(std::string_view axis)
{
    _axes.erase(std::string(axis));
}

// M1 (2026-07-15): thin 2-axis binding. Records xAxis / yAxis names;
// does not validate they exist as 1-axis (R-2 decision — let the
// player wire in their preferred order).
void InputMapping::bindAxis2D(std::string_view name,
                              std::string_view xAxis,
                              std::string_view yAxis)
{
    Axis2DBinding& b = _axes2D[std::string(name)];
    b.xAxis = std::string(xAxis);
    b.yAxis = std::string(yAxis);
}

void InputMapping::clearAxis2D(std::string_view name)
{
    _axes2D.erase(std::string(name));
}

const InputMapping::ActionBinding* InputMapping::findAction(std::string_view action) const
{
    auto it = _actions.find(std::string(action));
    return it != _actions.end() ? &it->second : nullptr;
}

const InputMapping::AxisBinding* InputMapping::findAxis(std::string_view axis) const
{
    auto it = _axes.find(std::string(axis));
    return it != _axes.end() ? &it->second : nullptr;
}

const InputMapping::Axis2DBinding* InputMapping::findAxis2D(std::string_view name) const
{
    auto it = _axes2D.find(std::string(name));
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
        TickActionState& state = _pendingTickActions[std::string(action)];
        state.pressed = isActionPressed(action);
        state.justPressed = state.justPressed || isActionJustPressed(action);
        state.justReleased = state.justReleased || isActionJustReleased(action);
    });
    forEachAxis([&](std::string_view axis) {
        _pendingTickAxes[std::string(axis)] = getAxisValue(axis);
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
    auto it = _currentTickActions.find(std::string(action));
    return it != _currentTickActions.end() && it->second.pressed;
}

bool InputMapping::isTickActionJustPressed(std::string_view action) const
{
    auto it = _currentTickActions.find(std::string(action));
    return it != _currentTickActions.end() && it->second.justPressed;
}

bool InputMapping::isTickActionJustReleased(std::string_view action) const
{
    auto it = _currentTickActions.find(std::string(action));
    return it != _currentTickActions.end() && it->second.justReleased;
}

float InputMapping::getTickAxisValue(std::string_view axis) const
{
    auto it = _currentTickAxes.find(std::string(axis));
    return it != _currentTickAxes.end() ? it->second : 0.0f;
}

float InputMapping::getAxisValue(std::string_view axis) const
{
    const AxisBinding* binding = findAxis(axis);
    if (binding == nullptr) {
        return 0.0f;
    }

    float value = 0.0f;
    if (_keyboard != nullptr) {
        float keyValue = 0.0f;
        for (const KeyPair& pair : binding->pairs) {
            if (pair.positive != KeyCode::Unknown && _keyboard->isKeyPressed(pair.positive)) {
                keyValue += 1.0f;
            }
            if (pair.negative != KeyCode::Unknown && _keyboard->isKeyPressed(pair.negative)) {
                keyValue -= 1.0f;
            }
        }
        // Clamp the digital sum before applying scale; scale may exceed 1.
        if (keyValue > 1.0f) {
            keyValue = 1.0f;
        } else if (keyValue < -1.0f) {
            keyValue = -1.0f;
        }
        value += keyValue * binding->scale;
    }

    if (_gamepad != nullptr && !binding->gamepadAxes.empty()) {
        float gamepadValue = 0.0f;
        for (const GamepadAxisSource& source : binding->gamepadAxes) {
            gamepadValue += _gamepad->getAxis(source.axis) * source.scale;
        }
        // Combined analog sources clamp to the normalized range.
        if (gamepadValue > 1.0f) {
            gamepadValue = 1.0f;
        } else if (gamepadValue < -1.0f) {
            gamepadValue = -1.0f;
        }
        value += gamepadValue;
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
