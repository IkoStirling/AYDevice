#include "AYInputMapping.h"

#include "AYKeyboardDevice.h"
#include "AYMouseDevice.h"

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

void InputMapping::clearAxis(std::string_view axis)
{
    _axes.erase(std::string(axis));
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
    return false;
}

float InputMapping::getAxisValue(std::string_view axis) const
{
    const AxisBinding* binding = findAxis(axis);
    if (binding == nullptr || _keyboard == nullptr) {
        return 0.0f;
    }

    float value = 0.0f;
    for (const KeyPair& pair : binding->pairs) {
        if (pair.positive != KeyCode::Unknown && _keyboard->isKeyPressed(pair.positive)) {
            value += 1.0f;
        }
        if (pair.negative != KeyCode::Unknown && _keyboard->isKeyPressed(pair.negative)) {
            value -= 1.0f;
        }
    }

    if (value > 1.0f) {
        value = 1.0f;
    } else if (value < -1.0f) {
        value = -1.0f;
    }
    return value * binding->scale;
}

bool InputMapping::hasAction(std::string_view action) const
{
    return findAction(action) != nullptr;
}

bool InputMapping::hasAxis(std::string_view axis) const
{
    return findAxis(axis) != nullptr;
}

} // namespace ayt::device
