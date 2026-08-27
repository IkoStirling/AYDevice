#include "AYTest.h"
#include "AYDevice/DeviceManager.h"

using namespace ayt::device;

TEST_SUITE(AYDevice_WindowManager)

TEST_CASE(test_window_manager_create_hidden) {
    WindowManager windows;
    WindowCreateInfo info{};
    info.title = "AYDevice Test";
    info.width = 640;
    info.height = 480;
    info.hidden = true;

    CHECK(windows.createWindow(info));
    CHECK(windows.isWindowValid());
    CHECK(windows.getWindowHandle() != nullptr);
    CHECK(windows.getWidth() > 0);
    CHECK(windows.getHeight() > 0);

    windows.destroyWindow();
    CHECK(!windows.isWindowValid());
    CHECK(windows.getWindowHandle() == nullptr);
}

TEST_CASE(test_window_manager_resize_callback) {
    WindowManager windows;
    WindowCreateInfo info{};
    info.width = 800;
    info.height = 600;
    info.hidden = true;

    int reportedWidth = 0;
    int reportedHeight = 0;
    windows.setWindowResizeCallback([&](int width, int height) {
        reportedWidth = width;
        reportedHeight = height;
    });

    CHECK(windows.createWindow(info));
    windows.setSize(1024, 768);

    int width = 0;
    int height = 0;
    windows.getSize(width, height);
    CHECK(width == 1024);
    CHECK(height == 768);
    CHECK(reportedWidth == 1024);
    CHECK(reportedHeight == 768);

    windows.destroyWindow();
}

#if defined(_WIN32)
TEST_CASE(test_window_manager_child_surface) {
    WindowManager windows;
    WindowCreateInfo info{};
    info.width = 640;
    info.height = 480;
    info.hidden = true;

    CHECK(windows.createWindow(info));

    ChildWindowDesc child{};
    child.parentHandle = windows.getWindowHandle();
    child.x = 10;
    child.y = 20;
    child.width = 200;
    child.height = 150;

    void* childHandle = nullptr;
    CHECK(windows.createChildWindow(child, childHandle));
    CHECK(childHandle != nullptr);

    windows.destroyAllChildWindows();
    windows.destroyWindow();
}
#endif

TEST_CASE(test_device_manager_initialize_poll) {
    DeviceManager devices;
    DeviceConfig config{};
    config.window.title = "DeviceManager Test";
    config.window.width = 640;
    config.window.height = 480;
    config.window.hidden = true;

    CHECK(devices.initialize(config));
    CHECK(devices.isInitialized());
    CHECK(devices.window().isWindowValid());

    devices.pollEvents();

    devices.shutdown();
    CHECK(!devices.isInitialized());
    CHECK(!devices.window().isWindowValid());
}

TEST_CASE(test_device_manager_focus_loss_releases_transient_input) {
    DeviceManager devices;
    DeviceConfig config{};
    config.window.hidden = true;
    config.enableTouch = true;
    CHECK(devices.initialize(config));

    devices.pollEvents();
    devices.keyboard()->onKeyDown(KeyCode::W);
    devices.mouse()->onButtonDown(MouseButton::Left);
    devices.touch()->onTouch(7, 12.0f, 18.0f, TouchPhase::Began);
    // The controls were held across a frame boundary before focus was lost.
    devices.keyboard()->newFrame();
    devices.mouse()->newFrame();
    devices.touch()->newFrame();
    devices.textInput().setEnabled(true);
    devices.textInput().onComposition("abc", 3, 1);

    devices.window().notifyFocused(false);

    CHECK(!devices.keyboard()->isKeyPressed(KeyCode::W));
    CHECK(devices.keyboard()->isKeyJustReleased(KeyCode::W));
    CHECK(!devices.mouse()->isButtonPressed(MouseButton::Left));
    CHECK(devices.mouse()->isButtonJustReleased(MouseButton::Left));
    CHECK(devices.touch()->getTouchById(7) != nullptr);
    CHECK(devices.touch()->getTouchById(7)->phase == TouchPhase::Cancelled);
    CHECK(!devices.textInput().isComposing());

    devices.shutdown();
}

#if defined(_WIN32)
TEST_CASE(test_relative_mouse_request_survives_unfocused_window) {
    WindowManager windows;
    WindowCreateInfo info{};
    info.hidden = true;
    CHECK(windows.createWindow(info));

    // L3 (2026-08-26): setRelativeMouseMode now returns RelativeMouseResult
    // distinguishing Disabled/Enabled/Busy. The enable call should
    // report Enabled (or Busy, never Disabled); the disable call
    // reports Disabled (the canonical "we successfully disabled it"
    // state).
    using R = WindowManager::RelativeMouseResult;
    const R onResult  = windows.setRelativeMouseMode(true);
    CHECK((onResult == R::Enabled || onResult == R::Busy));
    CHECK(windows.isRelativeMouseMode());
    const R offResult = windows.setRelativeMouseMode(false);
    CHECK(offResult == R::Disabled);
    CHECK(!windows.isRelativeMouseMode());

    windows.destroyWindow();
}
#endif

TEST_SUITE_END
