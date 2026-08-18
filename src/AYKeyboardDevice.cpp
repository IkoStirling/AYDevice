#include "AYDevice/KeyboardDevice.h"

namespace ayt::device {

KeyboardDevice::KeyboardDevice() = default;

int KeyboardDevice::index(KeyCode key)
{
    const int i = static_cast<int>(key);
    if (i < 0 || i >= kKeyCodeCount) {
        return static_cast<int>(KeyCode::Unknown);
    }
    return i;
}

void KeyboardDevice::newFrame()
{
    _previous = _current;
}

void KeyboardDevice::onKeyDown(KeyCode key)
{
    _current[index(key)] = true;
}

void KeyboardDevice::onKeyUp(KeyCode key)
{
    _current[index(key)] = false;
}

void KeyboardDevice::releaseAll()
{
    _current.fill(false);
}

void KeyboardDevice::reset()
{
    _current.fill(false);
    _previous.fill(false);
}

bool KeyboardDevice::isKeyPressed(KeyCode key) const
{
    return _current[index(key)];
}

bool KeyboardDevice::isKeyJustPressed(KeyCode key) const
{
    const int i = index(key);
    return _current[i] && !_previous[i];
}

bool KeyboardDevice::isKeyJustReleased(KeyCode key) const
{
    const int i = index(key);
    return !_current[i] && _previous[i];
}

} // namespace ayt::device
