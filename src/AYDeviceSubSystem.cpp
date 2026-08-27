#include "AYDevice/DeviceSubSystem.h"

#include <AYGameLoop/SubSystemRegistry.h>

#include <AYApplication/AppEventHost.h>
#include <AYEventSystem/EventBus.h>
#include <AYEventSystem/Events/DeviceEvents.h>
#include <AYEventSystem/Events/WindowEvents.h>

#include <cstdint>
#include <string_view>

namespace ayt::device {

namespace {
DeviceConfig g_bootstrapConfig{};

// Stable FNV-1a 32-bit of the action name → DeviceActionEvent::actionId.
int actionIdFromName(std::string_view name)
{
    uint32_t h = 2166136261u;
    for (unsigned char c : name) {
        h ^= c;
        h *= 16777619u;
    }
    return static_cast<int>(h);
}

void postActionEdges(InputMapping& mapping)
{
    auto& bus = ayt::event::EventBus::instance();
    mapping.forEachAction([&](std::string_view name) {
        if (mapping.isActionJustPressed(name)) {
            bus.post(ayt::event::DeviceActionEvent{actionIdFromName(name), true});
        }
        if (mapping.isActionJustReleased(name)) {
            bus.post(ayt::event::DeviceActionEvent{actionIdFromName(name), false});
        }
    });
}
} // namespace

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
        .phases = ayt::game::phaseBit(ayt::game::FramePhase::Platform)
                | ayt::game::phaseBit(ayt::game::FramePhase::FixedPrePhysics),
        .clock = ayt::game::ClockDomain::Unscaled,
        // Input.TickFrame producers must sort ahead of consumers before the
        // phase-local resource hazard pass serializes the conflicting batch.
        // This avoids a hard name dependency: editor hosts may provide input
        // through an externally-owned DeviceManager instead of registering
        // this window-owning subsystem.
        .phasePriority = 1000,
        .reads = {},
        .writes = {"Input.TickFrame"},
    };
    return desc;
}

void DeviceSubSystem::tick(ayt::game::FramePhase phase,
                           const ayt::game::FrameContext& context)
{
    if (phase == ayt::game::FramePhase::Platform) {
        update(context.deltaTime);
        if (_ready) {
            _devices.mapping().captureTickInputFrame(context.simTick + 1);
        }
    } else if (phase == ayt::game::FramePhase::FixedPrePhysics && _ready) {
        _devices.mapping().beginSimulationTick(context.simTick);
    }
}

bool DeviceSubSystem::initialize()
{
    if (_ready) {
        return true;
    }
    if (!_devices.initialize(g_bootstrapConfig)) {
        return false;
    }

    // Seed last-size tracker so the first update() doesn't fire a spurious
    // resize event for the window's initial dimensions.
    if (_devices.window().isWindowValid()) {
        _devices.window().getSize(_lastWidth, _lastHeight);
    } else {
        _lastWidth = _lastHeight = 0;
    }

    _ready = true;
    return true;
}

void DeviceSubSystem::update(float /*deltaTime*/)
{
    if (!_ready) {
        return;
    }

    // pollEvents() drives the SDL/Win32 message pump + advances input edge
    // state. By the time it returns, all per-frame window state mutations
    // (resizes, close requests) have already been latched.
    _devices.pollEvents();

    // Discrete Action edges → EventBus (continuous axes stay off-bus).
    publishPendingInputEvents();

    auto& window = _devices.window();
    if (!window.isWindowValid()) {
        return;
    }

    // ----- WindowResize delta detection -----
    // post (not emit) so the bridge is safe to call from the main thread
    // without forcing synchronous listener execution mid-frame; consumers
    // pump the bus at their own cadence (GameLoop pumps once per frame
    // after waitForRenderComplete — see AYGameLoop Phase 4).
    int width  = 0;
    int height = 0;
    window.getSize(width, height);
    if (width != _lastWidth || height != _lastHeight) {
        _lastWidth  = width;
        _lastHeight = height;
        ayt::event::EventBus::instance().post<ayt::event::WindowResizeEvent>(
            ayt::event::WindowResizeEvent{width, height});
    }

    // ----- WindowClose forward -----
    // consumeCloseRequested() is single-shot — we always re-arm by
    // re-posting until the host decides to call shutdown().
    if (window.consumeCloseRequested()) {
        ayt::event::EventBus::instance().post<ayt::event::WindowCloseEvent>({});
    }
}

void DeviceSubSystem::shutdown()
{
    if (!_ready) {
        return;
    }
    _devices.shutdown();
    // Release any host-scoped subscriptions we may have added in the future
    // (today the bridge is purely a producer — but the scope is here so
    // future Device-side listeners, e.g. accessibility hooks or gamepad
    // hot-plug watchers, plug in via _events.subscribe<T>() without
    // touching this file again).
    _events.disconnect();
    _ready = false;
}

void DeviceSubSystem::publishPendingInputEvents()
{
    if (!_ready) {
        return;
    }
    postActionEdges(_devices.mapping());
}

DeviceSubSystem* DeviceSubSystem::findRegistered()
{
    ayt::game::ISubSystem* system =
        ayt::game::SubSystemRegistry::instance().findSubSystem("Device");
    return dynamic_cast<DeviceSubSystem*>(system);
}

void DeviceSubSystem::registerSubSystem()
{
    // L7 (2026-08-26): static-bool guard + the underlying
    // SubSystemRegistry own a `new DeviceSubSystem()` whose lifetime
    // is tied to the GameLoop singleton. The guard prevents double
    // registration across translation units, but it does NOT prevent
    // static-deinit order problems: if a TU that called
    // registerSubSystem() during init is the same TU that tears the
    // GameLoop down (it isn't — GameLoop is owned by AYApplication),
    // the deletion path would race. The current contract is:
    //   - callers invoke registerSubSystem() exactly once during
    //     early-static init of their consumer translation unit
    //     (e.g. Gallery's Engine startup).
    //   - IGameLoop::instance() outlives all static objects in the
    //     consumer, because GameLoop is owned by AYApplication, which
    //     is constructed at program start and destroyed at program
    //     exit (well after our `static bool registered` is gone).
    // Do NOT change this to a `std::unique_ptr` cached in a function-
    // local static: function-local statics in headers get duplicated
    // across TUs (one definition rule violation).
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
