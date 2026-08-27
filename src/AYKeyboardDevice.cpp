#include "AYDevice/KeyboardDevice.h"

namespace ayt::device {

KeyboardDevice::KeyboardDevice() = default;

int KeyboardDevice::index(KeyCode key)
{
    const int i = static_cast<int>(key);
    if (i < 0 || i >= kKeyCodeCount) {
        return -1;  // L17 (2026-08-26): out-of-range sentinel
    }
    return i;
}

bool KeyboardDevice::inRange(KeyCode key)
{
    return index(key) >= 0;
}

void KeyboardDevice::newFrame()
{
    _previous = _current;
}

void KeyboardDevice::onKeyDown(KeyCode key)
{
    // L17 (2026-08-26): out-of-range KeyCode values are silently
    // ignored rather than silently marking the Unknown slot as
    // pressed (the previous behavior, which leaked buggy upstream
    // platforms into the canonical Unknown state).
    const int i = index(key);
    if (i >= 0) {
        _current[i] = true;
    }
}

void KeyboardDevice::onKeyUp(KeyCode key)
{
    const int i = index(key);
    if (i >= 0) {
        _current[i] = false;
    }
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
    // L17 (2026-08-26): out-of-range queries return false (was
    // _current[Unknown] which always reported false in practice but
    // masked the actual cause — callers couldn't tell whether the
    // key was genuinely unpressed or the platform leaked garbage).
    const int i = index(key);
    return i >= 0 && _current[i];
}

bool KeyboardDevice::isKeyJustPressed(KeyCode key) const
{
    const int i = index(key);
    if (i < 0) return false;
    return _current[i] && !_previous[i];
}

bool KeyboardDevice::isKeyJustReleased(KeyCode key) const
{
    const int i = index(key);
    if (i < 0) return false;
    return !_current[i] && _previous[i];
}

} // namespace ayt::device
