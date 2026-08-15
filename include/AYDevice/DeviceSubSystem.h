#pragma once
// AYDevice/DeviceSubSystem.h - GameLoop subsystem wrapping DeviceManager

#include "AYDevice/DeviceManager.h"

#include <AYGameLoop.h>

// INT-03 (2026-07-20): Device -> EventBus bridge. EventBusHostScope is the
// host-side RAII container from AYApplication that owns any per-Device-sub-
// system listeners (Phase 4 §a8c8be9 lesson — the device bridge is a pure
// producer today, but the scope is here so future Device-side listeners
// plug in via _events.subscribe<T>()).
#include <AYApplication/AppEventHost.h>

#include <cstdint>
#include <functional>

namespace ayt::device {

// Owns a DeviceManager and drives it from the game loop:
//   initialize() -> create window + input devices from the bootstrap config
//   update()     -> pollEvents() once per frame (advances input, pumps window)
//   shutdown()   -> tear down
//
// Registered as "Device" at priority 0 so it initializes first and polls before
// gameplay/physics/UI subsystems read input. Uses Unscaled time so input and
// window events keep flowing while the game is paused (poll ignores dt anyway).
//
// Lives in a separate target (AYDeviceSubSystem) so the core AYDevice library
// stays free of the AYGameLoop dependency; the editor uses DeviceManager
// directly without pulling in the loop.
class DeviceSubSystem : public ayt::game::ISubSystem {
public:
    // Configure the window + enabled devices applied at initialize().
    // Call before GameLoop::run(). Safe to call multiple times before init.
    static void setBootstrapConfig(const DeviceConfig& config);

    // ===== ISubSystem =====
    const char* getName() const override { return "Device"; }
    const ayt::game::SubSystemDescriptor& getDescriptor() const override;

    bool initialize() override;
    void update(float deltaTime) override;
    void fixedUpdate(float /*fixedDeltaTime*/) override {}
    void shutdown() override;

    // ===== Access =====
    DeviceManager& manager() { return _devices; }
    const DeviceManager& manager() const { return _devices; }
    bool isReady() const { return _ready; }

    // Locate the registered instance (e.g. for RendererSubSystem to fetch the
    // window handle). Null if not registered.
    static DeviceSubSystem* findRegistered();

    // Explicit, idempotent registration (static-lib-safe; preferred over the
    // REGISTER_SUBSYSTEM auto-init macro which can be stripped from a static lib).
    static void registerSubSystem();

    /// Post InputMapping just-pressed/released as DeviceActionEvent.
    /// Called from update() after pollEvents(); exposed so tests can inject
    /// synthetic key edges between poll and publish without a real HWND pump.
    void publishPendingInputEvents();

    // Window-source callback for RendererSubSystem. Signature matches
    // ayt::render::WindowProvider structurally (bool(void*&, uint32_t&,
    // uint32_t&)) so the two modules interoperate without a header dependency:
    // the application passes this into RendererSubSystem::setWindowProvider.
    // Returns false until the DeviceSubSystem window is valid.
    using WindowProvider = std::function<bool(void*&, uint32_t&, uint32_t&)>;
    static WindowProvider makeWindowProvider();

private:
    DeviceManager _devices;
    bool          _ready = false;

    // Last observed window dimensions — compared each update() against the
    // current WindowManager::getSize() to detect resize deltas. Seeded from
    // initialize() so the first poll doesn't fire a spurious event.
    int _lastWidth  = 0;
    int _lastHeight = 0;

    // Host-side EventBus host scope (Phase 4 lesson). The bridge today is a
    // pure producer (post<WindowResize/Close>); the scope is here so future
    // Device-side listeners plug in via _events.subscribe<T>() without
    // touching this file. Released in shutdown() before GameLoop continues
    // teardown.
    ayt::app::EventBusHostScope _events;
};

} // namespace ayt::device
