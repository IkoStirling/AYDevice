#include "AYDevice/MouseDevice.h"

namespace ayt::device {

MouseDevice::MouseDevice() = default;

int MouseDevice::index(MouseButton button)
{
    const int i = static_cast<int>(button);
    if (i < 0 || i >= kMouseButtonCount) {
        return 0;
    }
    return i;
}

void MouseDevice::newFrame()
{
    _previous = _current;
    _delta = Vector2{};
    _wheelDelta = 0.0f;
}

void MouseDevice::onMove(float x, float y)
{
    if (_hasLastPosition) {
        _delta.x += x - _position.x;
        _delta.y += y - _position.y;
    }
    _position = Vector2{x, y};
    _hasLastPosition = true;
}

void MouseDevice::onRelativeMove(float deltaX, float deltaY)
{
    _delta.x += deltaX;
    _delta.y += deltaY;
}

void MouseDevice::onButtonDown(MouseButton button)
{
    _current[index(button)] = true;
}

void MouseDevice::onButtonUp(MouseButton button)
{
    _current[index(button)] = false;
}

void MouseDevice::onWheel(float delta)
{
    _wheelDelta += delta;
}

void MouseDevice::releaseAllButtons()
{
    _current.fill(false);
    _delta = Vector2{};
    _wheelDelta = 0.0f;
    _hasLastPosition = false;
}

void MouseDevice::reset()
{
    _current.fill(false);
    _previous.fill(false);
    _position = Vector2{};
    _delta = Vector2{};
    _wheelDelta = 0.0f;
    _hasLastPosition = false;
}

bool MouseDevice::isButtonPressed(MouseButton button) const
{
    return _current[index(button)];
}

bool MouseDevice::isButtonJustPressed(MouseButton button) const
{
    const int i = index(button);
    return _current[i] && !_previous[i];
}

bool MouseDevice::isButtonJustReleased(MouseButton button) const
{
    const int i = index(button);
    return !_current[i] && _previous[i];
}

} // namespace ayt::device
