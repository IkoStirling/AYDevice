#include "AYTest.h"
#include "AYDevice/DeviceManager.h"

#include <vector>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <Windows.h>
#endif

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

#if defined(_WIN32)
TEST_CASE(test_device_manager_emits_ordered_platform_neutral_input) {
    DeviceManager devices;
    DeviceConfig config{};
    config.window.title = "Device input outlet test";
    config.window.hidden = true;
    CHECK(devices.initialize(config));

    std::vector<DeviceInputEvent> received;
    const DeviceInputListenerId listener = devices.addInputListener(
        [&](const DeviceInputEvent& event) { received.push_back(event); });
    CHECK(listener != 0);

    const HWND hwnd = static_cast<HWND>(devices.window().getWindowHandle());
    CHECK(hwnd != nullptr);
    if (hwnd == nullptr) {
        devices.shutdown();
        return;
    }

    ::PostMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(12, 34));
    ::PostMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(12, 34));
    ::PostMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(12, 34));
    ::PostMessageW(hwnd, WM_KEYDOWN, 'A', 0);
    ::PostMessageW(hwnd, WM_KEYDOWN, 'A', static_cast<LPARAM>(1u << 30));
    ::PostMessageW(hwnd, WM_KEYUP, 'A', 0);
    ::PostMessageW(hwnd, WM_CHAR, 'x', 0);

    POINT wheelPoint{12, 34};
    ::ClientToScreen(hwnd, &wheelPoint);
    ::PostMessageW(hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA),
                   MAKELPARAM(wheelPoint.x, wheelPoint.y));
    devices.pollEvents();

    int moveCount = 0;
    int buttonCount = 0;
    int keyCount = 0;
    int repeatCount = 0;
    int textCount = 0;
    int wheelCount = 0;
    for (const DeviceInputEvent& event : devices.inputEvents()) {
        switch (event.type) {
        case DeviceInputEventType::MouseMove: ++moveCount; break;
        case DeviceInputEventType::MouseButton: ++buttonCount; break;
        case DeviceInputEventType::Key:
            ++keyCount;
            if (event.repeat) ++repeatCount;
            break;
        case DeviceInputEventType::TextCommit:
            if (event.text == "x") ++textCount;
            break;
        case DeviceInputEventType::MouseWheel:
            if (event.wheelSource == MouseWheelSource::Standard
                && event.deltaY == 1.0f) {
                ++wheelCount;
            }
            break;
        default: break;
        }
    }

    CHECK_INT_EQ(moveCount, 1);
    CHECK_INT_EQ(buttonCount, 2);
    CHECK_INT_EQ(keyCount, 3);
    CHECK_INT_EQ(repeatCount, 1);
    CHECK_INT_EQ(textCount, 1);
    CHECK_INT_EQ(wheelCount, 1);
    CHECK_INT_EQ(received.size(), devices.inputEvents().size());

    const size_t receivedBeforeDisconnect = received.size();
    devices.removeInputListener(listener);
    ::PostMessageW(hwnd, WM_KEYDOWN, 'B', 0);
    devices.pollEvents();
    CHECK_INT_EQ(received.size(), receivedBeforeDisconnect);

    devices.shutdown();
}
#endif

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
