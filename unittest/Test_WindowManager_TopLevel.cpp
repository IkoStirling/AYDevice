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
#include "AYDevice/WindowManager.h"

#include <string>

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
    d.visible = false;   // keep the test runner silent

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

TEST_CASE(test_top_level_owner_and_runtime_title) {
    WindowManager windows;
    WindowCreateInfo info{};
    info.title = "Primary owner";
    info.width = 320;
    info.height = 240;
    info.hidden = true;
    CHECK(windows.createWindow(info));
    const HWND primary = static_cast<HWND>(windows.getWindowHandle());

    TopLevelWindowDesc desc{};
    desc.title = "Owned tool";
    desc.ownerHandle = primary;
    desc.width = 480;
    desc.height = 320;
    desc.visible = false;

    void* handle = nullptr;
    CHECK(windows.createTopLevelWindow(desc, handle));
    const HWND tool = static_cast<HWND>(handle);
    CHECK(::GetWindow(tool, GW_OWNER) == primary);
    CHECK(windows.setTopLevelTitle(handle, "AYUI Designer - Dirty *"));

    wchar_t title[128] = {};
    CHECK(::GetWindowTextW(tool, title, 128) > 0);
    CHECK(std::wstring(title) == L"AYUI Designer - Dirty *");

    windows.destroyTopLevelWindow(handle);
    CHECK_FALSE(::IsWindow(tool));
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
    d.visible = false;   // keep the test runner silent
    void* h = nullptr;
    CHECK(windows.createTopLevelWindow(d, h));
    HWND hw = static_cast<HWND>(h);

    // PR-Dock-TearOff: AdjustWindowRect — the CLIENT area must match the
    // desc exactly (the promoted card is laid out in client space).
    {
        RECT rc{};
        GetClientRect(hw, &rc);
        CHECK(rc.right - rc.left == 400);
        CHECK(rc.bottom - rc.top == 300);
    }

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
// 4. PR-Dock-TearOff input routing: typed callbacks fire from injected
//    Win32 messages, with coordinate/key translation (client-relative
//    floats, KeyCode, UTF-8 chars, wheel notches). SendMessage gives us
//    synchronous dispatch — no pump needed.
// -------------------------------------------------------------------------
TEST_CASE(test_top_level_input_routing) {
    WindowManager windows;
    WindowCreateInfo info{};
    info.width = 320;
    info.height = 240;
    info.hidden = true;
    CHECK(windows.createWindow(info));

    TopLevelWindowDesc d{};
    d.title = "D5_Input";
    d.width = 400;
    d.height = 300;
    d.visible = false;
    void* h = nullptr;
    CHECK(windows.createTopLevelWindow(d, h));
    HWND hw = static_cast<HWND>(h);

    int moveX = -1, moveY = -1, moveCount = 0;
    int btnX = -1, btnY = -1, btnCode = -1, pressCount = 0, releaseCount = 0;
    bool captured = false;
    float wheelX = -1.0f, wheelY = -1.0f, wheelDelta = 0.0f;
    int keyCount = 0;
    bool tabDown = false, tabUp = false;
    std::string charText;
    int leaveCount = 0;

    TopLevelWindowCallbacks cbs;
    cbs.onMouseMove = [&](float x, float y) {
        moveX = static_cast<int>(x);
        moveY = static_cast<int>(y);
        ++moveCount;
    };
    cbs.onMouseButton = [&](float x, float y, int button, bool pressed) {
        btnX = static_cast<int>(x);
        btnY = static_cast<int>(y);
        btnCode = button;
        if (pressed) {
            ++pressCount;
            return true;   // request capture
        }
        ++releaseCount;
        return false;
    };
    cbs.onMouseWheel = [&](float x, float y, float deltaY) {
        wheelX = x;
        wheelY = y;
        wheelDelta = deltaY;
    };
    cbs.onKey = [&](KeyCode key, bool pressed) {
        ++keyCount;
        if (key == KeyCode::Tab) {
            tabDown = pressed;
            tabUp = !pressed;
        }
    };
    cbs.onChar = [&](const char* utf8, int byteCount) {
        charText.assign(utf8, static_cast<size_t>(byteCount));
    };
    cbs.onMouseLeave = [&]() { ++leaveCount; };
    windows.setTopLevelCallbacks(h, cbs);

    // Mouse move — lParam is client coords.
    SendMessage(hw, WM_MOUSEMOVE, 0, MAKELPARAM(12, 34));
    CHECK(moveCount == 1);
    CHECK(moveX == 12);
    CHECK(moveY == 34);

    // Button down requests capture → SetCapture; up releases.
    SendMessage(hw, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(56, 78));
    CHECK(pressCount == 1);
    CHECK(btnX == 56);
    CHECK(btnY == 78);
    CHECK(btnCode == 0);
    CHECK(GetCapture() == hw);
    SendMessage(hw, WM_LBUTTONUP, 0, MAKELPARAM(56, 78));
    CHECK(releaseCount == 1);
    CHECK(GetCapture() != hw);

    // Wheel — lParam is SCREEN coords; window sits at the OS default
    // position, so use its actual screen rect to compute a point inside.
    RECT winRc{};
    GetWindowRect(hw, &winRc);
    const POINT inside{winRc.left + 5, winRc.top + 5};
    SendMessage(hw, WM_MOUSEWHEEL,
                MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA)),
                MAKELPARAM(inside.x, inside.y));
    CHECK(wheelDelta == -1.0f);   // one notch down, normalized
    {
        POINT check = inside;
        ScreenToClient(hw, &check);
        CHECK(wheelX == static_cast<float>(check.x));
        CHECK(wheelY == static_cast<float>(check.y));
    }

    // Keyboard — VK_TAB maps to KeyCode::Tab; repeat bit 30 suppressed.
    SendMessage(hw, WM_KEYDOWN, VK_TAB, 0);
    CHECK(tabDown);
    SendMessage(hw, WM_KEYDOWN, VK_TAB, (1L << 30));  // repeat → suppressed
    CHECK(keyCount == 1);
    SendMessage(hw, WM_KEYUP, VK_TAB, 0);
    CHECK(tabUp);
    CHECK(keyCount == 2);

    // Text — WM_CHAR 'A' → UTF-8 "A".
    SendMessage(hw, WM_CHAR, L'A', 0);
    CHECK(charText == "A");

    // Leave-notify.
    SendMessage(hw, WM_MOUSELEAVE, 0, 0);
    CHECK(leaveCount == 1);

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
    d.visible = false;   // keep the test runner silent
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

TEST_CASE(test_top_level_focus_callback_fires) {
    WindowManager windows;
    WindowCreateInfo info{};
    info.hidden = true;
    CHECK(windows.createWindow(info));

    TopLevelWindowDesc desc{};
    desc.visible = false;
    void* handle = nullptr;
    CHECK(windows.createTopLevelWindow(desc, handle));
    CHECK(handle != nullptr);

    int focusCount = 0;
    bool lastFocused = false;
    TopLevelWindowCallbacks callbacks{};
    callbacks.onFocusChanged = [&](bool focused) {
        ++focusCount;
        lastFocused = focused;
    };
    windows.setTopLevelCallbacks(handle, callbacks);

    const HWND hwnd = static_cast<HWND>(handle);
    ::SendMessageW(hwnd, WM_SETFOCUS, 0, 0);
    CHECK_INT_EQ(focusCount, 1);
    CHECK(lastFocused);
    ::SendMessageW(hwnd, WM_KILLFOCUS, 0, 0);
    CHECK_INT_EQ(focusCount, 2);
    CHECK_FALSE(lastFocused);

    windows.destroyTopLevelWindow(handle);
    windows.destroyWindow();
}

#endif // _WIN32

TEST_SUITE_END
