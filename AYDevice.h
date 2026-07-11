#pragma once

#include "include/AYDeviceManager.h"
#include "include/AYWindowManager.h"
#include "include/AYWindowTypes.h"
#include "include/AYInputTypes.h"
#include "include/AYInputDevice.h"
#include "include/AYKeyboardDevice.h"
#include "include/AYMouseDevice.h"
#include "include/AYGamepadDevice.h"
#include "include/AYInputMapping.h"
#include "include/AYInputNames.h"
#include "include/AYInputProfile.h"
// Note: AYInputProfileConfig.h (AYConfig bridge) is intentionally not included
// here; include it directly and link AYDeviceConfig only where persistence is
// needed, to keep the core AYDevice library free of the AYConfig dependency.
