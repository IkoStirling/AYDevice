#include "AYTest.h"
#include "AYDevice/TouchDevice.h"
#include "AYDevice/TextInput.h"

using namespace ayt::device;

TEST_SUITE(AYDevice_TouchText)

TEST_CASE(test_touch_began_move_end) {
    TouchDevice touch;

    touch.newFrame();
    touch.onTouch(1, 100.0f, 200.0f, TouchPhase::Began);
    CHECK(touch.getTouchCount() == 1);
    const TouchPoint* p = touch.getTouchById(1);
    CHECK(p != nullptr);
    CHECK(p->phase == TouchPhase::Began);
    CHECK(p->position.x == 100.0f);
    CHECK(p->delta.x == 0.0f);

    // Next frame: move accumulates delta.
    touch.newFrame();
    touch.onTouch(1, 110.0f, 230.0f, TouchPhase::Moved);
    p = touch.getTouchById(1);
    CHECK(p->phase == TouchPhase::Moved);
    CHECK(p->delta.x == 10.0f);
    CHECK(p->delta.y == 30.0f);

    // End marks it; still visible this frame.
    touch.newFrame();
    touch.onTouch(1, 110.0f, 230.0f, TouchPhase::Ended);
    CHECK(touch.getTouchCount() == 1);
    CHECK(touch.getTouchById(1)->phase == TouchPhase::Ended);

    // Retired next frame.
    touch.newFrame();
    CHECK(touch.getTouchCount() == 0);
    CHECK(touch.getTouchById(1) == nullptr);
}

TEST_CASE(test_touch_stationary_between_frames) {
    TouchDevice touch;
    touch.newFrame();
    touch.onTouch(5, 50.0f, 50.0f, TouchPhase::Began);

    // No event next frame -> survivor becomes Stationary with cleared delta.
    touch.newFrame();
    const TouchPoint* p = touch.getTouchById(5);
    CHECK(p != nullptr);
    CHECK(p->phase == TouchPhase::Stationary);
    CHECK(p->delta.x == 0.0f);
}

TEST_CASE(test_touch_multi_and_primary) {
    TouchDevice touch;
    touch.newFrame();
    touch.onTouch(3, 10.0f, 10.0f, TouchPhase::Began);
    touch.onTouch(1, 20.0f, 20.0f, TouchPhase::Began);
    touch.onTouch(2, 30.0f, 30.0f, TouchPhase::Began);

    CHECK(touch.getTouchCount() == 3);
    // Primary = lowest id.
    const TouchPoint* primary = touch.getPrimaryTouch();
    CHECK(primary != nullptr);
    CHECK(primary->id == 1);
}

TEST_CASE(test_touch_index_bounds_safe) {
    TouchDevice touch;
    CHECK(touch.getTouch(0) == nullptr);
    CHECK(touch.getTouch(-1) == nullptr);
    CHECK(!touch.isTouched());
}

TEST_CASE(test_touch_cancel_all_marks_active_contacts) {
    TouchDevice touch;
    touch.newFrame();
    touch.onTouch(9, 10.0f, 20.0f, TouchPhase::Began);
    touch.cancelAll();
    CHECK(touch.getTouchById(9) != nullptr);
    CHECK(touch.getTouchById(9)->phase == TouchPhase::Cancelled);

    touch.newFrame();
    CHECK(touch.getTouchById(9) == nullptr);
}

TEST_CASE(test_textinput_disabled_ignores) {
    TextInput text;
    // Disabled by default: events are dropped.
    text.onChar("a", 1);
    CHECK(!text.hasText());
    CHECK(text.getText().empty());
}

TEST_CASE(test_textinput_commit_accumulates_per_frame) {
    TextInput text;
    text.setEnabled(true);

    text.newFrame();
    text.onChar("H", 1);
    text.onChar("i", 1);
    CHECK(text.getText() == "Hi");
    CHECK(text.hasText());

    // Cleared next frame.
    text.newFrame();
    CHECK(!text.hasText());
}

TEST_CASE(test_textinput_commit_callback) {
    TextInput text;
    text.setEnabled(true);

    std::string received;
    text.onCommit = [&](const std::string& chunk) { received += chunk; };

    text.newFrame();
    text.onChar("A", 1);
    text.onChar("B", 1);
    CHECK(received == "AB");
}

TEST_CASE(test_textinput_composition_lifecycle) {
    TextInput text;
    text.setEnabled(true);

    text.onComposition("\xe4\xbd\xa0", 3, 3);  // "你" UTF-8
    CHECK(text.isComposing());
    CHECK(text.getComposition() == "\xe4\xbd\xa0");
    CHECK(text.getCompositionCursor() == 3);

    text.endComposition();
    CHECK(!text.isComposing());
    CHECK(text.getComposition().empty());
}

TEST_CASE(test_textinput_disable_clears_composition) {
    TextInput text;
    text.setEnabled(true);
    text.onComposition("abc", 3, 1);
    CHECK(text.isComposing());

    text.setEnabled(false);
    CHECK(!text.isComposing());
    CHECK(text.getComposition().empty());
}

TEST_SUITE_END
