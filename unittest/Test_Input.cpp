#include "AYTest.h"
#include "AYDevice/KeyboardDevice.h"
#include "AYDevice/MouseDevice.h"
#include "AYDevice/InputMapping.h"

using namespace ayt::device;

TEST_SUITE(AYDevice_Input)

TEST_CASE(test_keyboard_press_and_edges) {
    KeyboardDevice kb;

    // Frame 1: press Space.
    kb.newFrame();
    kb.onKeyDown(KeyCode::Space);
    CHECK(kb.isKeyPressed(KeyCode::Space));
    CHECK(kb.isKeyJustPressed(KeyCode::Space));
    CHECK(!kb.isKeyJustReleased(KeyCode::Space));

    // Frame 2: still held, no longer "just" pressed.
    kb.newFrame();
    CHECK(kb.isKeyPressed(KeyCode::Space));
    CHECK(!kb.isKeyJustPressed(KeyCode::Space));

    // Frame 3: release.
    kb.newFrame();
    kb.onKeyUp(KeyCode::Space);
    CHECK(!kb.isKeyPressed(KeyCode::Space));
    CHECK(kb.isKeyJustReleased(KeyCode::Space));
}

TEST_CASE(test_keyboard_unknown_key_safe) {
    KeyboardDevice kb;
    kb.newFrame();
    // Out-of-range values must not crash; treated as Unknown.
    kb.onKeyDown(static_cast<KeyCode>(60000));
    CHECK(!kb.isKeyPressed(KeyCode::A));
}

TEST_CASE(test_mouse_move_delta_and_wheel) {
    MouseDevice mouse;

    mouse.newFrame();
    mouse.onMove(100.0f, 100.0f);   // first move seeds position, no delta
    CHECK(mouse.getPosition().x == 100.0f);
    CHECK(mouse.getDelta().x == 0.0f);

    mouse.newFrame();
    mouse.onMove(110.0f, 130.0f);
    CHECK(mouse.getDelta().x == 10.0f);
    CHECK(mouse.getDelta().y == 30.0f);

    mouse.newFrame();
    mouse.onWheel(1.0f);
    mouse.onWheel(0.5f);
    CHECK(mouse.getWheelDelta() == 1.5f);

    // Delta and wheel reset each frame.
    mouse.newFrame();
    CHECK(mouse.getDelta().x == 0.0f);
    CHECK(mouse.getWheelDelta() == 0.0f);
}

TEST_CASE(test_mouse_button_edges) {
    MouseDevice mouse;

    mouse.newFrame();
    mouse.onButtonDown(MouseButton::Left);
    CHECK(mouse.isButtonPressed(MouseButton::Left));
    CHECK(mouse.isButtonJustPressed(MouseButton::Left));

    mouse.newFrame();
    CHECK(mouse.isButtonPressed(MouseButton::Left));
    CHECK(!mouse.isButtonJustPressed(MouseButton::Left));

    mouse.newFrame();
    mouse.onButtonUp(MouseButton::Left);
    CHECK(!mouse.isButtonPressed(MouseButton::Left));
    CHECK(mouse.isButtonJustReleased(MouseButton::Left));
}

TEST_CASE(test_mapping_action_keyboard_and_mouse) {
    KeyboardDevice kb;
    MouseDevice mouse;
    InputMapping mapping;
    mapping.setKeyboard(&kb);
    mapping.setMouse(&mouse);

    const KeyCode jumpKeys[] = {KeyCode::Space, KeyCode::W};
    mapping.bindAction("Jump", jumpKeys);
    const MouseButton fireButtons[] = {MouseButton::Left};
    mapping.bindActionMouse("Fire", fireButtons);

    CHECK(mapping.hasAction("Jump"));
    CHECK(!mapping.isActionPressed("Jump"));

    kb.newFrame();
    kb.onKeyDown(KeyCode::W);   // second bound key
    CHECK(mapping.isActionPressed("Jump"));
    CHECK(mapping.isActionJustPressed("Jump"));

    mouse.newFrame();
    mouse.onButtonDown(MouseButton::Left);
    CHECK(mapping.isActionPressed("Fire"));

    // Unbound action is false, not a crash.
    CHECK(!mapping.isActionPressed("Nonexistent"));
}

TEST_CASE(test_mapping_axis) {
    KeyboardDevice kb;
    InputMapping mapping;
    mapping.setKeyboard(&kb);

    const InputMapping::KeyPair moveX[] = {{KeyCode::A, KeyCode::D}};
    mapping.bindAxis("MoveX", moveX);

    CHECK(mapping.getAxisValue("MoveX") == 0.0f);

    kb.newFrame();
    kb.onKeyDown(KeyCode::D);   // positive
    CHECK(mapping.getAxisValue("MoveX") == 1.0f);

    kb.onKeyDown(KeyCode::A);   // both -> cancels to 0
    CHECK(mapping.getAxisValue("MoveX") == 0.0f);

    kb.onKeyUp(KeyCode::D);     // only negative
    CHECK(mapping.getAxisValue("MoveX") == -1.0f);
}

TEST_CASE(test_mapping_axis_scale) {
    KeyboardDevice kb;
    InputMapping mapping;
    mapping.setKeyboard(&kb);

    const InputMapping::KeyPair pairs[] = {{KeyCode::Down, KeyCode::Up}};
    mapping.bindAxis("Look", pairs, 2.5f);

    kb.newFrame();
    kb.onKeyDown(KeyCode::Up);
    CHECK(mapping.getAxisValue("Look") == 2.5f);
}

TEST_CASE(test_tick_snapshot_consumes_edges_once_across_catch_up_ticks) {
    KeyboardDevice kb;
    InputMapping mapping;
    mapping.setKeyboard(&kb);

    const KeyCode jumpKeys[] = {KeyCode::Space};
    mapping.bindAction("Jump", jumpKeys);

    kb.newFrame();
    kb.onKeyDown(KeyCode::Space);
    mapping.captureTickInputFrame(41);

    mapping.beginSimulationTick(41);
    CHECK(mapping.getCurrentInputSimTick() == 41);
    CHECK(mapping.isTickActionPressed("Jump"));
    CHECK(mapping.isTickActionJustPressed("Jump"));

    // A catch-up tick inherits held state, but an input edge is never replayed.
    mapping.beginSimulationTick(42);
    CHECK(mapping.isTickActionPressed("Jump"));
    CHECK(!mapping.isTickActionJustPressed("Jump"));
    CHECK(!mapping.isTickActionJustReleased("Jump"));

    kb.newFrame();
    kb.onKeyUp(KeyCode::Space);
    mapping.captureTickInputFrame(43);
    mapping.beginSimulationTick(43);
    CHECK(!mapping.isTickActionPressed("Jump"));
    CHECK(mapping.isTickActionJustReleased("Jump"));
}

// M1 (2026-07-15): thin 2-axis wrapper tests. Convention:
//   bindAxis2D("move", "move_x", "move_y")
// composes two already-bound 1-D axes by name. getAxis2D calls
// getAxisValue(xAxis) + getAxisValue(yAxis) and packages the
// result as Vector2. No caching; each query = 1 map lookup for
// the binding + 2 x getAxisValue. Unbound 2-axis name returns
// Vector2{} (zero vector); underlying 1-axis falls back through
// InputMapping::getAxisValue's existing 0.0f default.
TEST_CASE(test_mapping_axis2d_unbound_returns_zero) {
    InputMapping mapping;   // no keyboard, no bindings
    CHECK_FALSE(mapping.hasAxis2D("move"));
    const Vector2 v = mapping.getAxis2D("move");
    CHECK(v.x == 0.0f);
    CHECK(v.y == 0.0f);
    mapping.clearAxis2D("move");   // idempotent on unbound
}

TEST_CASE(test_mapping_axis2d_combines_two_bound_axes) {
    KeyboardDevice kb;
    InputMapping mapping;
    mapping.setKeyboard(&kb);

    const InputMapping::KeyPair moveX[] = {{KeyCode::A, KeyCode::D}};
    const InputMapping::KeyPair moveY[] = {{KeyCode::S, KeyCode::W}};
    mapping.bindAxis("move_x", moveX);
    mapping.bindAxis("move_y", moveY);
    mapping.bindAxis2D("move", "move_x", "move_y");

    CHECK(mapping.hasAxis2D("move"));

    kb.newFrame();
    kb.onKeyDown(KeyCode::D);   // +1 x
    kb.onKeyDown(KeyCode::W);   // +1 y
    {
        Vector2 v = mapping.getAxis2D("move");
        CHECK(v.x == 1.0f);
        CHECK(v.y == 1.0f);
    }

    // Cancel x; clearAxis2D shouldn't affect bindAxis.
    kb.onKeyDown(KeyCode::A);   // x → 0
    {
        Vector2 v = mapping.getAxis2D("move");
        CHECK(v.x == 0.0f);
        CHECK(v.y == 1.0f);
    }
}

TEST_CASE(test_mapping_axis2d_does_not_disturb_single_axis_query) {
    KeyboardDevice kb;
    InputMapping mapping;
    mapping.setKeyboard(&kb);

    const InputMapping::KeyPair moveX[] = {{KeyCode::A, KeyCode::D}};
    mapping.bindAxis("move_x", moveX);
    mapping.bindAxis2D("move", "move_x", "move_y");   // move_y unbound

    kb.newFrame();
    kb.onKeyDown(KeyCode::D);

    // Single-axis query untouched; 2-axis query yields {1, 0} (y defaults to 0).
    CHECK(mapping.getAxisValue("move_x") == 1.0f);
    Vector2 v = mapping.getAxis2D("move");
    CHECK(v.x == 1.0f);
    CHECK(v.y == 0.0f);
}

TEST_CASE(test_mapping_axis2d_clear_drops_binding) {
    KeyboardDevice kb;
    InputMapping mapping;
    mapping.setKeyboard(&kb);

    const InputMapping::KeyPair moveX[] = {{KeyCode::A, KeyCode::D}};
    mapping.bindAxis("move_x", moveX);
    mapping.bindAxis2D("move", "move_x", "move_y");

    CHECK(mapping.hasAxis2D("move"));
    mapping.clearAxis2D("move");
    CHECK_FALSE(mapping.hasAxis2D("move"));
    Vector2 v = mapping.getAxis2D("move");
    CHECK(v.x == 0.0f);
    CHECK(v.y == 0.0f);
}

TEST_CASE(test_mapping_axis2d_overwrite_replaces_axes) {
    // Re-binding "move" with different axes should replace, not
    // compose. PlayerController code may want to re-bind a 2-axis
    // (e.g. swap "move_x"+"move_y" -> "aim_x"+"aim_y").
    KeyboardDevice kb;
    InputMapping mapping;
    mapping.setKeyboard(&kb);

    const InputMapping::KeyPair xA[] = {{KeyCode::A, KeyCode::D}};
    const InputMapping::KeyPair xB[] = {{KeyCode::J, KeyCode::L}};
    mapping.bindAxis("left_x", xA);
    mapping.bindAxis("right_x", xB);

    mapping.bindAxis2D("steer", "left_x", "left_y");   // left_y unbound
    mapping.bindAxis2D("steer", "right_x", "right_y");  // replace
    const Vector2 v = mapping.getAxis2D("steer");
    CHECK(v.x == 0.0f);
    CHECK(v.y == 0.0f);
}

TEST_SUITE_END
