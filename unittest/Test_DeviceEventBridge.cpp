// AYDevice/unittest/Test_DeviceEventBridge.cpp
//
// INT-03 (2026-07-20) 鈥?DeviceSubSystem 鈫?EventBus bridge tests.
//
// The DeviceSubSystem posts `WindowResizeEvent` / `WindowCloseEvent` onto
// the process-wide EventBus each update() when a window state delta is
// detected. These tests verify both paths using the explicit notify API
// (`notifyResized` / `notifyClosed`) so we don't need a real SDL window
// in CI. Tests share the process-singleton EventBus 鈥?each test uses a
// hand-picked event type for delta testing so other suites (and the
// DeviceSubSystem's own WindowResize/Close events) don't trip the
// listener-count baselines.

#include "AYDeviceSubSystem.h"
#include "AYTest.h"

#include <AYAppEventHost.h>
#include <ayevent/EventBus.h>
#include <ayevent/Events/DeviceEvents.h>
#include <ayevent/Events/WindowEvents.h>

#include <atomic>
#include <span>
#include <utility>

using namespace ayt::device;

namespace ayt::device::test
{

// Hand-picked synthetic event type used to count emit-side interactions
// without colliding with WindowResize/Close produced by the bridge itself.
struct BridgeProbeEvent {
    int  frame = 0;
    static constexpr ayt::event::EventTypeId   kTypeId   = 0x0A10'9001;
    static constexpr ayt::event::EventPriority kPriority = ayt::event::EventPriority::Normal;
};

namespace {

// Configure a hidden test window so DeviceSubSystem can initialize headlessly.
DeviceConfig makeHiddenConfig(const char* title, int w, int h)
{
    DeviceConfig cfg{};
    cfg.window.title   = title;
    cfg.window.width   = w;
    cfg.window.height  = h;
    cfg.window.hidden  = true;  // headless CI
    return cfg;
}

} // namespace

TEST_SUITE(DeviceEventBridge)

TEST_CASE(Bridge_WindowResize_PostsOnSizeDelta) {
    auto& bus = ayt::event::EventBus::instance();
    const auto baseline = bus.listenerCount(ayt::event::WindowResizeEvent::kTypeId);

    DeviceSubSystem::setBootstrapConfig(makeHiddenConfig("Bridge.Resize", 320, 240));
    DeviceSubSystem sub;
    CHECK(sub.initialize());

    // Snapshot the initial dimensions seeded by initialize() so the first
    // update() does NOT emit (no delta). Use a synthetic probe event to
    // count updates independent of resize delta 鈥?a probe per frame lets
    // us assert "no resize posted on the seed frame" because the only
    // way to make a resize event fire is a notifyResized() that changes
    // the dimension.
    ayt::app::EventBusHostScope probeScope;
    std::atomic<int> probeCount{0};
    probeScope.subscribe<BridgeProbeEvent>(
        [&probeCount](const BridgeProbeEvent&) { probeCount.fetch_add(1); });

    int w = 0, h = 0;
    sub.manager().window().getSize(w, h);
    CHECK(w == 320);
    CHECK(h == 240);

    std::atomic<int> resizeCount{0};
    int lastW = 0, lastH = 0;
    ayt::app::EventBusHostScope scope;
    scope.subscribe<ayt::event::WindowResizeEvent>(
        [&resizeCount, &lastW, &lastH](const ayt::event::WindowResizeEvent& e) {
            resizeCount.fetch_add(1);
            lastW = e.width;
            lastH = e.height;
        });

    // Frame 1: seed 鈥?no resize delta expected.
    sub.update(0.016f);
    bus.pump();
    bus.post(BridgeProbeEvent{1});
    bus.pump();
    CHECK_INT_EQ(resizeCount.load(), 0);
    CHECK_INT_EQ(probeCount.load(), 1);

    // Trigger a size change via notifyResized; next update() detects the
    // delta and posts WindowResizeEvent.
    sub.manager().window().notifyResized(640, 480);
    sub.update(0.016f);
    bus.pump();

    CHECK_INT_EQ(resizeCount.load(), 1);
    CHECK_INT_EQ(lastW, 640);
    CHECK_INT_EQ(lastH, 480);

    // No further change 鈥?no further event.
    sub.update(0.016f);
    bus.pump();
    CHECK_INT_EQ(resizeCount.load(), 1);

    // Cleanup.
    scope.disconnect();
    probeScope.disconnect();
    sub.shutdown();
    CHECK_INT_EQ(static_cast<int>(bus.listenerCount(ayt::event::WindowResizeEvent::kTypeId)),
                 static_cast<int>(baseline));
}

TEST_CASE(Bridge_WindowClose_PostsOnConsumeCloseRequested) {
    auto& bus = ayt::event::EventBus::instance();
    const auto baseline = bus.listenerCount(ayt::event::WindowCloseEvent::kTypeId);

    DeviceSubSystem::setBootstrapConfig(makeHiddenConfig("Bridge.Close", 200, 150));
    DeviceSubSystem sub;
    CHECK(sub.initialize());

    std::atomic<int> closeCount{0};
    ayt::app::EventBusHostScope scope;
    scope.subscribe<ayt::event::WindowCloseEvent>(
        [&closeCount](const ayt::event::WindowCloseEvent&) {
            closeCount.fetch_add(1);
        });

    // Frame 1: no close requested 鈥?no event.
    sub.update(0.016f);
    bus.pump();
    CHECK_INT_EQ(closeCount.load(), 0);

    // Latch a close request via the explicit notify path. update() detects
    // it via consumeCloseRequested() and posts a WindowCloseEvent.
    sub.manager().window().notifyClosed();
    sub.update(0.016f);
    bus.pump();
    CHECK_INT_EQ(closeCount.load(), 1);

    // consumeCloseRequested() is single-shot 鈥?next update() must NOT fire
    // again unless notifyClosed() is called again.
    sub.update(0.016f);
    bus.pump();
    CHECK_INT_EQ(closeCount.load(), 1);

    sub.manager().window().notifyClosed();
    sub.update(0.016f);
    bus.pump();
    CHECK_INT_EQ(closeCount.load(), 2);

    // Cleanup.
    scope.disconnect();
    sub.shutdown();
    CHECK_INT_EQ(static_cast<int>(bus.listenerCount(ayt::event::WindowCloseEvent::kTypeId)),
                 static_cast<int>(baseline));
}

TEST_CASE(Bridge_Shutdown_DisconnectsHostScope) {
    // Host-scope cleanup parity check: DeviceSubSystem owns an
    // EventBusHostScope (_events) 鈥?even though it's empty today (the
    // bridge is a pure producer), shutdown() must drain it idempotently
    // without touching the bus. Pin this so a future listener addition
    // (e.g. gamepad hot-plug watcher) plugs in via _events.subscribe<T>()
    // and inherits the Phase 4 lesson for free.
    auto& bus = ayt::event::EventBus::instance();
    const auto baseline = bus.listenerCount(BridgeProbeEvent::kTypeId);

    DeviceSubSystem::setBootstrapConfig(makeHiddenConfig("Bridge.Shutdown", 100, 100));
    DeviceSubSystem sub;
    CHECK(sub.initialize());

    // Drive a couple of frames to exercise update().
    sub.update(0.016f);
    sub.update(0.016f);

    // shutdown() must be idempotent and clean 鈥?no event posted by the
    // scope itself (we assert the listenerCount baseline remains stable).
    sub.shutdown();
    sub.shutdown();  // idempotent
    CHECK(!sub.isReady());

    CHECK_INT_EQ(static_cast<int>(bus.listenerCount(BridgeProbeEvent::kTypeId)),
                 static_cast<int>(baseline));
}

TEST_CASE(Bridge_DeviceAction_PostsOnJustPressed) {
    auto& bus = ayt::event::EventBus::instance();
    bus.unsubscribeAll();

    DeviceSubSystem::setBootstrapConfig(makeHiddenConfig("Bridge.Action", 200, 150));
    DeviceSubSystem sub;
    CHECK(sub.initialize());

    KeyCode space = KeyCode::Space;
    sub.manager().mapping().bindAction("Jump", std::span<const KeyCode>(&space, 1));

    std::atomic<int> pressCount{0};
    std::atomic<int> releaseCount{0};
    ayt::app::EventBusHostScope scope;
    scope.subscribe<ayt::event::DeviceActionEvent>(
        [&](const ayt::event::DeviceActionEvent& e) {
            if (e.pressed) {
                pressCount.fetch_add(1);
            } else {
                releaseCount.fetch_add(1);
            }
        });

    // Establish a clean edge baseline.
    sub.manager().pollEvents();
    sub.publishPendingInputEvents();
    bus.pump();
    CHECK_INT_EQ(pressCount.load(), 0);

    // Inject after newFrame (inside pollEvents) so just-pressed is true.
    sub.manager().pollEvents();
    if (auto* kb = sub.manager().keyboard()) {
        kb->onKeyDown(KeyCode::Space);
    }
    sub.publishPendingInputEvents();
    bus.pump();
    CHECK_INT_EQ(pressCount.load(), 1);
    CHECK_INT_EQ(releaseCount.load(), 0);

    sub.manager().pollEvents();
    if (auto* kb = sub.manager().keyboard()) {
        kb->onKeyUp(KeyCode::Space);
    }
    sub.publishPendingInputEvents();
    bus.pump();
    CHECK_INT_EQ(releaseCount.load(), 1);

    scope.disconnect();
    sub.shutdown();
    bus.unsubscribeAll();
}

TEST_SUITE_END

} // namespace ayt::device::test

