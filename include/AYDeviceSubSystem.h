#pragma once
// AYDeviceSubSystem.h - GameLoop subsystem wrapping DeviceManager

#include "AYDeviceManager.h"

#include <AYGameLoop.h>

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

private:
    DeviceManager _devices;
    bool          _ready = false;
};

} // namespace ayt::device
