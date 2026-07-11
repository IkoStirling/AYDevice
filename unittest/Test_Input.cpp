#include "AYTest.h"
#include "AYKeyboardDevice.h"
#include "AYMouseDevice.h"
#include "AYInputMapping.h"

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

TEST_SUITE_END
