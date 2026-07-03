# AYDevice

Device subsystem for AY Engine: window lifecycle (Phase-1), input mapping and XR (later).

## Phase-1 (E3 skeleton)

- `WindowManager` — native window create/destroy, size/title, resize/close/focus callbacks
- `createChildWindow()` — interim editor viewport child HWND (E2-interim path)
- `DeviceManager` — owns `WindowManager`, `initialize` / `shutdown` / `pollEvents`

Default backend is **Win32** on Windows. Optional SDL2 via CMake:

```cmake
-DAY_DEVICE_USE_SDL2=ON
```

## Usage

```cpp
#include "AYDevice.h"

ayt::device::DeviceManager devices;
ayt::device::DeviceConfig config{};
config.window.title = "Editor";
config.window.width = 1280;
config.window.height = 720;

devices.initialize(config);
// devices.window().getWindowHandle() -> pass to bgfx / renderer bootstrap
devices.pollEvents();
devices.shutdown();
```

## Tests

Build and run `Test_AYDevice` after configuring the project.

## See also

- [design.md](design.md) — full architecture; §10.4 editor integration timeline
