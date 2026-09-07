#include <AYDevice/DeviceRuntimeModule.h>

#include <AYDevice/DeviceSubSystem.h>

#include <memory>
#include <string>
#include <utility>

namespace ayt::device
{

DeviceRuntimeModule::DeviceRuntimeModule(DeviceConfig config)
    : SubSystemModule(
          ayt::module::ModuleDescriptor{
              .id = std::string(kDeviceRuntimeModuleId),
              .displayName = "AYDevice Runtime",
              .version = "0.1.0",
              .dependencies = {}},
          "Device",
          [config = std::move(config)]() {
              DeviceSubSystem::setBootstrapConfig(config);
              return std::make_unique<DeviceSubSystem>();
          })
{
}

} // namespace ayt::device
