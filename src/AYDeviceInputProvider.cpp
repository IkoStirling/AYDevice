// AYDeviceInputProvider.cpp - INT-02 (2026-07-15)

#include "AYDeviceInputProvider.h"

#include "AYDeviceManager.h"
#include "AYInputMapping.h"

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

} // namespace ayt::device