# AYDevice

Device subsystem for AY Engine: **window + input** (single module). Input mapping and XR are phased in per [design.md](design.md).

> **2026-07-11**：**`AYInput` 独立模块已废弃** — 键盘/鼠标/Action 映射均在 AYDevice。GameLoop 只注册 `DeviceSubSystem`。

## Phase-1 (E3 skeleton)

- `WindowManager` — primary and owned top-level native windows, create/destroy,
  activation/title updates, and typed resize/close/focus/input callbacks
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

## Phase-3 (input profile / rebinding)

- `InputProfile` — serializable snapshot of Action/Axis bindings a player can customize; `applyTo(mapping)` pushes bindings into the live `InputMapping`. Bindings are neutral string **tokens** so saved configs survive enum reordering and are hand-editable.
- `AYInputNames` — stable enum↔string names for `KeyCode` / `MouseButton` / `GamepadButton` / `GamepadAxis`.
- **AYConfig bridge** (`AYInputProfileConfig`, separate `AYDeviceConfig` target) — persists a profile through AYConfig under `Input.*` dot-keys. Kept out of the core library so `AYDevice` stays free of the AYConfig / nlohmann_json dependency; only consumers that persist bindings link `AYDeviceConfig`.

### Token grammar

| Source | Token |
|--------|-------|
| Keyboard key | `Space`, `A`, `F1` |
| Mouse button | `Mouse:Left` |
| Gamepad button | `Pad:A`, `Pad:DpadUp` |
| Axis key pair (neg/pos) | `A/D` (either side may be empty) |
| Gamepad analog axis | `PadAxis:LeftX`, `PadAxis:RightX*0.5` (optional `*scale`) |

### Config keys (AYConfig)

```
Input.Profile.Name     = "Default"
Input.Actions.Jump     = "Space,Pad:A"
Input.Actions.Fire     = "Mouse:Left"
Input.Axes.MoveX       = "A/D,PadAxis:LeftX"
Input.AxesScale.MoveX  = 1.5
```

Values go through Config's string/float API — human-readable and layerable
(Engine default → User override) in both JSON and INI.

## Phase-3 (touch + text/IME)

- `TouchDevice` — multi-touch contacts with per-frame phases (`Began` / `Moved` / `Stationary` / `Ended` / `Cancelled`), accumulated deltas, and a primary-contact helper. Fed from Win32 `WM_TOUCH` (window registered via `RegisterTouchWindow`, gated on `DeviceConfig::enableTouch`).
- `TextInput` — committed UTF-8 text (`WM_CHAR`, surrogate-pair aware) plus in-progress IME composition string (`WM_IME_COMPOSITION`, via `imm32`). Gated by `setEnabled()` so game keybinds don't double-fire while a text field has focus; `onCommit` / `onCompositionUpdate` callbacks for live UI.
- `DeviceManager` — `touch()` accessor (null unless enabled) and always-present `textInput()`; `pollEvents()` advances their frame state alongside the other devices.

## GameLoop integration (DeviceSubSystem)

`DeviceSubSystem` (separate `AYDeviceSubSystem` target) wraps `DeviceManager` as
a GameLoop `ISubSystem`:

The default AYApplication Client host installs it through `DeviceRuntimeModule`
(`AYDevice.Runtime`). The explicit `registerSubSystem()` API below remains the
low-level compatibility path for standalone demos and tests.

- Registered as **"Device"** at **priority 0** (initializes and updates first, so
  input is polled before gameplay/physics/UI read it), **Unscaled** time so input
  and window events keep flowing while paused.
- `initialize()` creates the window + devices from a bootstrap `DeviceConfig`;
  `update()` calls `pollEvents()` once per frame; `shutdown()` tears down.
- Static-lib-safe explicit registration (mirrors `RendererSubSystem`), not the
  `REGISTER_SUBSYSTEM` auto-init macro.

```cpp
#include "AYDevice/DeviceSubSystem.h"
using namespace ayt::device;

DeviceConfig config{};
config.window.title = "Game";
DeviceSubSystem::setBootstrapConfig(config);   // before GameLoop::run()
DeviceSubSystem::registerSubSystem();

// Other subsystems fetch the polled devices:
if (auto* dev = DeviceSubSystem::findRegistered()) {
    bool jump = dev->manager().isActionPressed("Jump");
    void* hwnd = dev->manager().window().getWindowHandle();  // -> RendererSubSystem
}
```

Kept in a separate target so the core `AYDevice` library stays free of the
`AYGameLoop` dependency — the editor uses `DeviceManager` directly without the loop.

## EventBus producers (DeviceSubSystem)

| Event | When |
|-------|------|
| `WindowResizeEvent` | Window size delta after `pollEvents` |
| `WindowCloseEvent` | Close requested |
| `DeviceActionEvent` | Bound InputMapping **just-pressed** / **just-released** (`actionId` = FNV-1a of action name) |

**Not** posted: continuous axes / mouse deltas (`DeviceAxisEvent` catalog exists but stays off-bus by design). See [`../AYEventSystem/README.md`](../AYEventSystem/README.md).

### Handing the window to the renderer

`DeviceSubSystem::makeWindowProvider()` returns a `std::function<bool(void*&,
uint32_t&, uint32_t&)>` that reports the live window handle + size once the
subsystem is ready. The application passes it to
`RendererSubSystem::setWindowProvider(...)`, so the renderer fetches its native
surface from the device layer without either module depending on the other's
headers (they meet only through the `std::function` signature). Renderer declares
a `"Device"` subsystem dependency so it initializes after the window exists.

## Backend

Default backend is **Win32** on Windows. Optional SDL2 via CMake:

```cmake
-DAY_DEVICE_USE_SDL2=ON
```

Both backends feed the same keyboard, mouse, touch, UTF-8 text/IME and
InputMapping state. Win32 gamepads use XInput; SDL2 gamepads use
SDL_GameController with hot-plug slot assignment and rumble. Relative mouse
mode is available through `WindowManager::setRelativeMouseMode()` and is
automatically suspended on focus loss.

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
// devices.window().setRelativeMouseMode(true); -> FPS/raw mouse look

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

Build and run `AYDevice_Test` after configuring the project. Backend work can
use the smaller `AYDevice_Core_Test`; SDL2 builds additionally provide
`AYDevice_SDL_Test` for native event translation.

## See also

- [design.md](design.md) — full architecture; §10.4 editor integration timeline
