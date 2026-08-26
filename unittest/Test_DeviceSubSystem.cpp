#include "AYTest.h"
#include "AYDevice/DeviceSubSystem.h"

#include <AYGameLoop/SubSystemRegistry.h>

using namespace ayt::device;

TEST_SUITE(AYDevice_SubSystem)

TEST_CASE(test_subsystem_descriptor) {
    DeviceSubSystem sub;
    CHECK(std::string(sub.getName()) == "Device");

    const auto& desc = sub.getDescriptor();
    CHECK(std::string(desc.name) == "Device");
    CHECK(desc.basePriority == 0);
    CHECK(desc.dependencies.empty());
    CHECK(desc.timeType == ayt::game::SubSystemDescriptor::TimeType::Unscaled);
    CHECK(desc.phasePriority == 1000);
    bool writesInputFrame = false;
    for (const char* resource : desc.writes) {
        if (resource && std::string(resource) == "Input.TickFrame") {
            writesInputFrame = true;
            break;
        }
    }
    CHECK(writesInputFrame);
}

TEST_CASE(test_subsystem_lifecycle) {
    DeviceConfig config{};
    config.window.title = "DeviceSubSystem Test";
    config.window.width = 320;
    config.window.height = 240;
    config.window.hidden = true;  // headless CI
    DeviceSubSystem::setBootstrapConfig(config);

    DeviceSubSystem sub;
    CHECK(!sub.isReady());

    CHECK(sub.initialize());
    CHECK(sub.isReady());
    CHECK(sub.manager().isInitialized());
    CHECK(sub.manager().window().isWindowValid());

    // Update pumps events + polls input; must not throw and stay ready.
    sub.update(0.016f);
    sub.fixedUpdate(0.016f);
    CHECK(sub.isReady());

    // Input devices are reachable through the wrapped manager.
    CHECK(sub.manager().keyboard() != nullptr);
    CHECK(sub.manager().mouse() != nullptr);

    sub.shutdown();
    CHECK(!sub.isReady());
    CHECK(!sub.manager().isInitialized());
}

TEST_CASE(test_subsystem_double_init_shutdown_safe) {
    DeviceConfig config{};
    config.window.hidden = true;
    DeviceSubSystem::setBootstrapConfig(config);

    DeviceSubSystem sub;
    CHECK(sub.initialize());
    CHECK(sub.initialize());  // idempotent
    sub.shutdown();
    sub.shutdown();           // idempotent
    CHECK(!sub.isReady());
}

TEST_CASE(test_subsystem_register_and_find) {
    // Clean slate so the test is order-independent.
    ayt::game::SubSystemRegistry::instance().unregisterSubSystem("Device");
    CHECK(DeviceSubSystem::findRegistered() == nullptr);

    ayt::game::IGameLoop::instance().registerSubSystem(new DeviceSubSystem());

    DeviceSubSystem* found = DeviceSubSystem::findRegistered();
    CHECK(found != nullptr);
    CHECK(std::string(found->getName()) == "Device");

    ayt::game::SubSystemRegistry::instance().unregisterSubSystem("Device");
    CHECK(DeviceSubSystem::findRegistered() == nullptr);
}

TEST_CASE(test_window_provider_reports_valid_window) {
    ayt::game::SubSystemRegistry::instance().unregisterSubSystem("Device");

    auto provider = DeviceSubSystem::makeWindowProvider();
    void* handle = nullptr;
    uint32_t w = 0, h = 0;

    // No registered/ready subsystem yet -> provider returns false.
    CHECK(!provider(handle, w, h));

    DeviceConfig config{};
    config.window.width = 400;
    config.window.height = 300;
    config.window.hidden = true;
    DeviceSubSystem::setBootstrapConfig(config);

    auto* sub = new DeviceSubSystem();
    ayt::game::IGameLoop::instance().registerSubSystem(sub);

    // Registered but not initialized -> still false.
    CHECK(!provider(handle, w, h));

    CHECK(sub->initialize());
    CHECK(provider(handle, w, h));
    CHECK(handle != nullptr);
    CHECK(w > 0);
    CHECK(h > 0);

    sub->shutdown();
    ayt::game::SubSystemRegistry::instance().unregisterSubSystem("Device");
}

TEST_SUITE_END
