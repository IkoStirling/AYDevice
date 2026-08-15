#pragma once

#include "AYDevice/DeviceManager.h"
#include "AYDevice/WindowManager.h"
#include "AYDevice/WindowTypes.h"
#include "AYDevice/InputTypes.h"
#include "AYDevice/InputDevice.h"
#include "AYDevice/KeyboardDevice.h"
#include "AYDevice/MouseDevice.h"
#include "AYDevice/GamepadDevice.h"
#include "AYDevice/TouchDevice.h"
#include "AYDevice/TextInput.h"
#include "AYDevice/InputMapping.h"
#include "AYDevice/InputNames.h"
#include "AYDevice/InputProfile.h"
// Note: AYDevice/InputProfileConfig.h (AYConfig bridge) is intentionally not included
// here; include it directly and link AYDeviceConfig only where persistence is
// needed, to keep the core AYDevice library free of the AYConfig dependency.
