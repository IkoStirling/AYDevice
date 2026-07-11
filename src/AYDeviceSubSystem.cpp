#include "AYDeviceSubSystem.h"

#include <AYSubSystemRegistry.h>

namespace ayt::device {

namespace {
DeviceConfig g_bootstrapConfig{};
}

void DeviceSubSystem::setBootstrapConfig(const DeviceConfig& config)
{
    g_bootstrapConfig = config;
}

const ayt::game::SubSystemDescriptor& DeviceSubSystem::getDescriptor() const
{
    static const ayt::game::SubSystemDescriptor desc = {
        .name = "Device",
        .dependencies = {},
        .basePriority = 0,  // first to init / update: poll before consumers read
        .timeType = ayt::game::SubSystemDescriptor::TimeType::Unscaled,
    };
    return desc;
}

bool DeviceSubSystem::initialize()
{
    if (_ready) {
        return true;
    }
    if (!_devices.initialize(g_bootstrapConfig)) {
        return false;
    }
    _ready = true;
    return true;
}

void DeviceSubSystem::update(float /*deltaTime*/)
{
    if (_ready) {
        _devices.pollEvents();
    }
}

void DeviceSubSystem::shutdown()
{
    if (_ready) {
        _devices.shutdown();
        _ready = false;
    }
}

DeviceSubSystem* DeviceSubSystem::findRegistered()
{
    ayt::game::ISubSystem* system =
        ayt::game::SubSystemRegistry::instance().findSubSystem("Device");
    return dynamic_cast<DeviceSubSystem*>(system);
}

void DeviceSubSystem::registerSubSystem()
{
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;
    if (findRegistered() != nullptr) {
        return;
    }
    ayt::game::IGameLoop::instance().registerSubSystem(new DeviceSubSystem());
}

DeviceSubSystem::WindowProvider DeviceSubSystem::makeWindowProvider()
{
    return [](void*& outHandle, uint32_t& outWidth, uint32_t& outHeight) -> bool {
        DeviceSubSystem* self = findRegistered();
        if (self == nullptr || !self->isReady()) {
            return false;
        }
        const WindowManager& window = self->manager().window();
        if (!window.isWindowValid()) {
            return false;
        }
        outHandle = window.getWindowHandle();
        outWidth  = static_cast<uint32_t>(window.getWidth());
        outHeight = static_cast<uint32_t>(window.getHeight());
        return outHandle != nullptr;
    };
}

} // namespace ayt::device
