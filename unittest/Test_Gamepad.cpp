#include "AYTest.h"
#include "AYGamepadDevice.h"
#include "AYInputMapping.h"

using namespace ayt::device;

TEST_SUITE(AYDevice_Gamepad)

TEST_CASE(test_gamepad_button_edges) {
    GamepadDevice pad(0);
    pad.setConnected(true);

    pad.newFrame();
    pad.onButtonDown(GamepadButton::A);
    CHECK(pad.isButtonPressed(GamepadButton::A));
    CHECK(pad.isButtonJustPressed(GamepadButton::A));
    CHECK(!pad.isButtonJustReleased(GamepadButton::A));

    pad.newFrame();
    CHECK(pad.isButtonPressed(GamepadButton::A));
    CHECK(!pad.isButtonJustPressed(GamepadButton::A));

    pad.newFrame();
    pad.onButtonUp(GamepadButton::A);
    CHECK(!pad.isButtonPressed(GamepadButton::A));
    CHECK(pad.isButtonJustReleased(GamepadButton::A));
}

TEST_CASE(test_gamepad_axes) {
    GamepadDevice pad(0);
    pad.setConnected(true);

    pad.setAxis(GamepadAxis::LeftX, 0.5f);
    pad.setAxis(GamepadAxis::LeftY, -0.25f);
    pad.setAxis(GamepadAxis::RightTrigger, 0.8f);

    CHECK(pad.getLeftStick().x == 0.5f);
    CHECK(pad.getLeftStick().y == -0.25f);
    CHECK(pad.getRightTrigger() == 0.8f);

    // Out-of-range clamps to [-1, 1].
    pad.setAxis(GamepadAxis::RightX, 3.0f);
    CHECK(pad.getRightStick().x == 1.0f);
}

TEST_CASE(test_gamepad_disconnect_clears_state) {
    GamepadDevice pad(0);
    pad.setConnected(true);
    pad.onButtonDown(GamepadButton::Start);
    pad.setAxis(GamepadAxis::LeftX, 0.9f);

    pad.setConnected(false);
    CHECK(!pad.isConnected());
    CHECK(!pad.isButtonPressed(GamepadButton::Start));
    CHECK(pad.getLeftStick().x == 0.0f);
}

TEST_CASE(test_gamepad_vibration_disconnected_safe) {
    GamepadDevice pad(0);
    // Not connected: must be a safe no-op (no crash, no XInput call effect).
    pad.setVibration(1.0f, 1.0f);
    pad.stopVibration();
    CHECK(!pad.isConnected());
}

TEST_CASE(test_mapping_action_gamepad) {
    GamepadDevice pad(0);
    pad.setConnected(true);
    InputMapping mapping;
    mapping.setGamepad(&pad);

    const GamepadButton jump[] = {GamepadButton::A};
    mapping.bindActionGamepad("Jump", jump);

    CHECK(!mapping.isActionPressed("Jump"));

    pad.newFrame();
    pad.onButtonDown(GamepadButton::A);
    CHECK(mapping.isActionPressed("Jump"));
    CHECK(mapping.isActionJustPressed("Jump"));

    pad.newFrame();
    pad.onButtonUp(GamepadButton::A);
    CHECK(mapping.isActionJustReleased("Jump"));
}

TEST_CASE(test_mapping_axis_gamepad_and_keyboard_combine) {
    GamepadDevice pad(0);
    pad.setConnected(true);
    InputMapping mapping;
    mapping.setGamepad(&pad);

    mapping.bindAxisGamepad("MoveX", GamepadAxis::LeftX);

    pad.setAxis(GamepadAxis::LeftX, 0.4f);
    CHECK(mapping.getAxisValue("MoveX") == 0.4f);

    // Combined value clamps to 1.0.
    pad.setAxis(GamepadAxis::LeftX, 1.0f);
    mapping.bindAxisGamepad("MoveX", GamepadAxis::RightX);
    pad.setAxis(GamepadAxis::RightX, 1.0f);
    CHECK(mapping.getAxisValue("MoveX") == 1.0f);
}

TEST_SUITE_END
