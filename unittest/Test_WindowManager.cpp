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

TEST_SUITE_END