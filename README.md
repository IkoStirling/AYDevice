# AYDevice

Device subsystem for AY Engine: **window + input** (single module). Input mapping and XR are phased in per [design.md](design.md).

> **2026-07-11**：**`AYInput` 独立模块已废弃** — 键盘/鼠标/Action 映射均在 AYDevice。GameLoop 只注册 `DeviceSubSystem`。

## Phase-1 (E3 skeleton)

- `WindowManager` — native window create/destroy, size/title, resize/close/focus callbacks
- `createChildWindow()` — interim editor viewport child HWND (E2-interim path)
- `DeviceManager` — owns `WindowManager`, `initialize` / `shutdown` / `pollEvents`

## Phase-2 (input)

- `KeyboardDevice` / `MouseDevice` — per-frame edge detection (pressed / just-pressed / just-released), mouse delta + wheel
- `InputMapping` — Action bindings (keyboard + mouse) and Axis bindings (keyboard key pairs) with query API
- `DeviceManager` — owns keyboard/mouse/mapping; `pollEvents()` advances frame edges then drains the platform pump
- Win32 message → `KeyCode` / `MouseButton` translation wired through `WindowManager` input callbacks

## Phase-3 (gamepad)

- `GamepadDevice` — XInput backend, up to 4 slots; sticks (deadzoned, normalized -1..1), triggers (0..1), buttons with per-frame edges, and rumble (`setVibration` / `stopVibration`)
- `InputMapping` — Action bindings extended with gamepad buttons; Axis bindings extended with gamepad analog axes (summed with keyboard, gamepad portion clamped)
- `DeviceManager` — `gamepad(slot)` accessor; `pollEvents()` polls all slots each frame (XInput is polled, not event-driven)
- Event-feed API (`setConnected` / `onButtonDown` / `setAxis`) lets the devices and mapping be unit-tested without hardware

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

// Input (Phase-2)
ayt::device::KeyCode jump[] = { ayt::device::KeyCode::Space };
devices.mapping().bindAction("Jump", jump);
ayt::device::InputMapping::KeyPair moveX[] = {{ ayt::device::KeyCode::A, ayt::device::KeyCode::D }};
devices.mapping().bindAxis("MoveX", moveX);

// Gamepad (Phase-3)
ayt::device::GamepadButton jumpPad[] = { ayt::device::GamepadButton::A };
devices.mapping().bindActionGamepad("Jump", jumpPad);
devices.mapping().bindAxisGamepad("MoveX", ayt::device::GamepadAxis::LeftX);

// per frame:
devices.pollEvents();
bool jumping = devices.isActionPressed("Jump");
float moveAmount = devices.getAxisValue("MoveX");
if (auto* pad = devices.gamepad(0); pad && pad->isConnected()) {
    pad->setVibration(0.5f, 0.5f);
}

devices.shutdown();
```

## Tests

Build and run `Test_AYDevice` after configuring the project.

## See also

- [design.md](design.md) — full architecture; §10.4 editor integration timeline
