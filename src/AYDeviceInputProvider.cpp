// AYDeviceInputProvider.cpp - INT-02 (2026-07-15)

#include "AYDevice/DeviceInputProvider.h"

#include "AYDevice/DeviceManager.h"
#include "AYDevice/InputMapping.h"

#include <string>

namespace ayt::device {

DeviceInputProvider::DeviceInputProvider(DeviceManager* mgr) noexcept
    : _mgr(mgr)
{
}

bool DeviceInputProvider::isPressed(const std::string& action) const
{
    // Mirrors MockInputProvider's permissive "unknown action -> false"
    // behavior so that pre-binding queries (during World::update's
    // first tick before InputMapping::bindAction runs) don't crash.
    if (!_mgr) return false;
    return _mgr->mapping().isActionPressed(action);
}

bool DeviceInputProvider::isJustPressed(const std::string& action) const
{
    // Edge semantics are owned by InputMapping which reads from
    // KeyboardDevice::isKeyJustPressed / MouseDevice::isButtonJustPressed
    // / GamepadDevice::isButtonJustPressed. DeviceManager::pollEvents()
    // calls newFrame() BEFORE the platform pump (AYDeviceManager.cpp
    // pollEvents() order), so an event fed during the platform pump
    // becomes visible to this query within the same frame and is
    // cleared by next frame's newFrame(). Callers must invoke
    // pollEvents() before ScriptSubSystem::update(dt) — GameLoop
    // priority Device=0 < Script=100 enforces this ordering.
    if (!_mgr) return false;
    return _mgr->mapping().isActionJustPressed(action);
}

float DeviceInputProvider::getAxisValue(const std::string& action) const
{
    // INT-03 (2026-07-15): Logia `input.axis(name)` reads
    // InputMapping::getAxisValue(name). Phase-2 already ships
    // bindAxis(KeyPair) / bindAxisGamepad(GamepadAxis) — see
    // AYDevice/InputMapping.h:48-50. Unbound axes return 0.0f from
    // InputMapping; same default applies to a nullptr mgr here.
    // Range: keyboard KeyPair sums to [-1, 1] then * scale;
    // gamepad analog summed without scale (see AYInputMapping.cpp
    // getAxisValue impl). Logia scripts typically scale via
    // `self.speed * input.axis("move_x")` rather than pre-scaling
    // at bind time, so the default 1.0f scale is the right
    // starting point.
    if (!_mgr) return 0.0f;
    return _mgr->mapping().getAxisValue(action);
}

bool DeviceInputProvider::isJustReleased(const std::string& action) const
{
    // INT-03 (2026-07-15): Logia `input.is_just_released(name)`
    // reads InputMapping::isActionJustReleased(name). Same edge
    // semantics as isJustPressed above (DeviceManager::pollEvents
    // newFrame-before-pump ordering). Use case: charge-up timer
    // that triggers on release, weapon swap-on-release, etc.
    if (!_mgr) return false;
    return _mgr->mapping().isActionJustReleased(action);
}

bool DeviceInputProvider::getAxisValue2D(const std::string& action,
                                         double& outX, double& outY) const
{
    // M1 (2026-07-15): 2-axis read for input.vec2. Returns false
    // (and zero-bias outX/outY) when the named 2-axis has not been
    // configured through InputMapping::bindAxis2D. Caller (Logia
    // ambient lambda) treats false as "unbound" and falls back to
    // {0, 0}; outX/outY are pre-zeroed below so the branch is a
    // pure return. Nullptr mgr also returns false safely — matches
    // the permissive posture of the other DeviceInputProvider
    // overrides and prevents Editor transient state from crashing
    // during shutdown races.
    outX = 0.0;
    outY = 0.0;
    if (!_mgr) return false;
    if (!_mgr->mapping().hasAxis2D(action)) return false;
    const Vector2 v = _mgr->mapping().getAxis2D(action);
    outX = static_cast<double>(v.x);
    outY = static_cast<double>(v.y);
    return true;
}

} // namespace ayt::device