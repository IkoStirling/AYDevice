// =============================================================================
// D5 — WindowManager::createTopLevelWindow tests.
//
// All tests are Win32-only because D5 v1 only ships the Win32 backend
// for top-level windows (mirrors the existing createChildWindow
// asymmetry). The Linux/macOS path returns false; see AYD5 landmine #2.
//
// Reference patterns:
//   * Existing test_window_manager_child_surface (Test_WindowManager.cpp:55)
//     uses createWindow with hidden=true so the test never pumps messages.
//   * The resize test pumps messages implicitly via setSize (which calls
//     SetWindowPos, which triggers WM_SIZE).
//   * The close test uses SendMessage(hwnd, WM_CLOSE, ...) for
//     synchronous dispatch (SendMessage is the documented way to
//     inject a close without an event pump).
// =============================================================================

#include "AYTest.h"
#include "AYWindowManager.h"

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <Windows.h>
#endif

using namespace ayt::device;

namespace {

// Pump any pending Win32 messages without blocking. The standard
// MSG_TRANSLATE_DISPATCH loop is sufficient — this is a test
// helper, not a real message loop.
void pumpMessages()
{
#if defined(_WIN32)
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
#endif
}

} // namespace

TEST_SUITE(AYDevice_WindowManager_TopLevel)

#if defined(_WIN32)

// -------------------------------------------------------------------------
// 1. Create + destroy round-trip — verifies the new path returns a valid
//    handle and that destroy cleans the registry. Baseline for the
//    other two tests.
// -------------------------------------------------------------------------
TEST_CASE(test_top_level_create_destroy) {
    WindowManager windows;
    // A primary hidden window is required because the WndProc thunk
    // references `_impl->instance` (GetModuleHandleW) — but for top-level
    // windows we don't strictly require a primary. Create anyway to keep
    // the test deterministic against any future "instance from main window"
    // plumbing.
    WindowCreateInfo info{};
    info.title = "Primary";
    info.width = 320;
    info.height = 240;
    info.hidden = true;
    CHECK(windows.createWindow(info));

    TopLevelWindowDesc d{};
    d.title  = "D5_TL_ChildA";
    d.x      = 100;
    d.y      = 100;
    d.width  = 640;
    d.height = 480;

    void* h = nullptr;
    CHECK(windows.createTopLevelWindow(d, h));
    CHECK(h != nullptr);

    HWND hw = static_cast<HWND>(h);
    CHECK(IsWindow(hw));

    // Destroy round-trip — should drop the handle from the registry.
    windows.destroyTopLevelWindow(h);
    CHECK_FALSE(IsWindow(hw));

    windows.destroyWindow();
}

// -------------------------------------------------------------------------
// 2. Resize callback fires. Win32's WM_SIZE cannot be synthesised by
//    SendMessage (the kernel generates it). Instead, MoveWindow triggers
//    an actual WM_SIZE; we pump messages to make sure the WndProc runs
//    before we read the lambda's captured values.
// -------------------------------------------------------------------------
TEST_CASE(test_top_level_resize_callback_fires) {
    WindowManager windows;
    WindowCreateInfo info{};
    info.width = 320;
    info.height = 240;
    info.hidden = true;
    CHECK(windows.createWindow(info));

    TopLevelWindowDesc d{};
    d.title = "D5_Resize";
    d.width = 400;
    d.height = 300;
    void* h = nullptr;
    CHECK(windows.createTopLevelWindow(d, h));
    HWND hw = static_cast<HWND>(h);

    int reportedW = 0;
    int reportedH = 0;
    int callCount = 0;

    TopLevelWindowCallbacks cbs;
    cbs.onResize = [&](int width, int height) {
        reportedW = width;
        reportedH = height;
        ++callCount;
    };
    windows.setTopLevelCallbacks(h, cbs);

    // Force a size change. MoveWindow triggers WM_SIZE synchronously
    // (posted, then dispatched the moment we yield).
    HWND parentForMove = GetParent(hw);  // nullptr for top-level
    (void)parentForMove;
    const BOOL ok = MoveWindow(hw, 0, 0, 800, 600, TRUE);
    CHECK(ok);
    pumpMessages();

    // Client rect should now be (800, 600) modulo the chrome; the callback
    // reports client size (readClientSize), so the numbers should reflect
    // that. We don't pin exact client size because chrome depends on the
    // OS theme — only verify the callback fired and read non-zero values.
    CHECK(callCount >= 1);
    CHECK(reportedW > 0);
    CHECK(reportedH > 0);

    windows.destroyTopLevelWindow(h);
    windows.destroyWindow();
}

// -------------------------------------------------------------------------
// 3. Close-requested callback fires on WM_CLOSE. SendMessage is the
//    documented way to inject a close — and our thunk suppresses the
//    default DestroyWindow path (returns 0) so the test can verify
//    handle post-dispatch.
// -------------------------------------------------------------------------
TEST_CASE(test_top_level_close_requested_callback) {
    WindowManager windows;
    WindowCreateInfo info{};
    info.width = 320;
    info.height = 240;
    info.hidden = true;
    CHECK(windows.createWindow(info));

    TopLevelWindowDesc d{};
    d.title = "D5_Close";
    d.width = 400;
    d.height = 300;
    void* h = nullptr;
    CHECK(windows.createTopLevelWindow(d, h));
    HWND hw = static_cast<HWND>(h);

    int closeCount = 0;
    TopLevelWindowCallbacks cbs;
    cbs.onCloseRequested = [&]() { ++closeCount; };
    windows.setTopLevelCallbacks(h, cbs);

    SendMessage(hw, WM_CLOSE, 0, 0);
    pumpMessages();

    CHECK(closeCount == 1);

    // Because our thunk returns 0 on WM_CLOSE when a callback fires,
    // the OS does NOT auto-DestroyWindow — the window is still alive.
    CHECK(IsWindow(hw));

    windows.destroyTopLevelWindow(h);
    CHECK_FALSE(IsWindow(hw));
    windows.destroyWindow();
}

#endif // _WIN32

TEST_SUITE_END
