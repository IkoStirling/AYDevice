#pragma once

#include <AYDevice/DeviceManager.h>
#include <AYGameLoop/SubSystemModule.h>

#include <string_view>

namespace ayt::device
{

inline constexpr std::string_view kDeviceRuntimeModuleId =
    "AYDevice.Runtime";

class DeviceRuntimeModule final : public ayt::game::SubSystemModule
{
public:
    explicit DeviceRuntimeModule(DeviceConfig config);
};

} // namespace ayt::device
