#include "AYTest.h"
#include "AYInputNames.h"
#include "AYInputProfile.h"
#include "AYInputMapping.h"
#include "AYKeyboardDevice.h"
#include "AYMouseDevice.h"
#include "AYGamepadDevice.h"

using namespace ayt::device;

TEST_SUITE(AYDevice_InputProfile)

TEST_CASE(test_input_names_roundtrip) {
    CHECK(keyCodeName(KeyCode::Space) == "Space");
    CHECK(keyCodeFromName("Space") == KeyCode::Space);
    CHECK(keyCodeFromName("NotAKey") == KeyCode::Unknown);

    MouseButton mb;
    CHECK(mouseButtonFromName("Left", mb));
    CHECK(mb == MouseButton::Left);
    CHECK(mouseButtonName(MouseButton::Middle) == "Middle");

    GamepadButton gb;
    CHECK(gamepadButtonFromName("DpadUp", gb));
    CHECK(gb == GamepadButton::DpadUp);

    GamepadAxis ga;
    CHECK(gamepadAxisFromName("LeftX", ga));
    CHECK(ga == GamepadAxis::LeftX);
    CHECK(!gamepadAxisFromName("Nope", ga));
}

TEST_CASE(test_profile_apply_action_multisource) {
    InputProfile profile;
    profile.setAction("Jump", {"Space", "Mouse:Left", "Pad:A"});

    KeyboardDevice kb;
    MouseDevice mouse;
    GamepadDevice pad(0);
    pad.setConnected(true);

    InputMapping mapping;
    mapping.setKeyboard(&kb);
    mapping.setMouse(&mouse);
    mapping.setGamepad(&pad);

    const int failed = profile.applyTo(mapping);
    CHECK(failed == 0);
    CHECK(mapping.hasAction("Jump"));

    kb.newFrame();
    kb.onKeyDown(KeyCode::Space);
    CHECK(mapping.isActionPressed("Jump"));

    kb.newFrame();  // release space
    mouse.newFrame();
    mouse.onButtonDown(MouseButton::Left);
    CHECK(mapping.isActionPressed("Jump"));

    mouse.newFrame();  // release mouse
    pad.newFrame();
    pad.onButtonDown(GamepadButton::A);
    CHECK(mapping.isActionPressed("Jump"));
}

TEST_CASE(test_profile_apply_axis_keypair_and_gamepad) {
    InputProfile profile;
    profile.setAxis("MoveX", {"A/D", "PadAxis:LeftX"});

    KeyboardDevice kb;
    GamepadDevice pad(0);
    pad.setConnected(true);

    InputMapping mapping;
    mapping.setKeyboard(&kb);
    mapping.setGamepad(&pad);

    CHECK(profile.applyTo(mapping) == 0);

    kb.newFrame();
    kb.onKeyDown(KeyCode::D);
    CHECK(mapping.getAxisValue("MoveX") == 1.0f);

    kb.newFrame();
    kb.onKeyUp(KeyCode::D);  // release D
    pad.setAxis(GamepadAxis::LeftX, -0.5f);
    CHECK(mapping.getAxisValue("MoveX") == -0.5f);
}

TEST_CASE(test_profile_axis_scale_token) {
    InputProfile profile;
    profile.setAxis("Look", {"PadAxis:RightX*2"});

    GamepadDevice pad(0);
    pad.setConnected(true);
    InputMapping mapping;
    mapping.setGamepad(&pad);

    CHECK(profile.applyTo(mapping) == 0);

    pad.setAxis(GamepadAxis::RightX, 0.4f);
    // 0.4 * 2 = 0.8 (within clamp)
    CHECK(mapping.getAxisValue("Look") == 0.8f);
}

TEST_CASE(test_profile_bad_tokens_counted) {
    InputProfile profile;
    profile.setAction("Bad", {"Space", "Pad:NotAButton", "Mouse:Nope"});

    InputMapping mapping;
    const int failed = profile.applyTo(mapping);
    CHECK(failed == 2);  // two bad tokens; Space still binds
    CHECK(mapping.hasAction("Bad"));
}

TEST_CASE(test_profile_default_enumeration) {
    InputProfile profile = InputProfile::makeDefault();
    CHECK(!profile.empty());
    CHECK(profile.actionTokens("Jump") != nullptr);
    CHECK(profile.axisTokens("MoveX") != nullptr);
    CHECK(profile.actionNames().size() >= 2);
    CHECK(profile.axisNames().size() >= 2);
}

TEST_SUITE_END
