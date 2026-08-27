#include "AYTest.h"
#include "AYDevice/DeviceManager.h"

#include <SDL.h>
#include <cstring>

using namespace ayt::device;

TEST_SUITE(AYDevice_SDL2)

namespace {

DeviceConfig hiddenConfig()
{
    DeviceConfig config{};
    config.window.title = "AYDevice SDL Input Test";
    config.window.width = 640;
    config.window.height = 480;
    config.window.hidden = true;
    config.enableTouch = true;
    return config;
}

void pushKey(Uint32 type, SDL_Scancode scancode)
{
    SDL_Event event{};
    event.type = type;
    event.key.type = type;
    event.key.repeat = 0;
    event.key.keysym.scancode = scancode;
    CHECK(SDL_PushEvent(&event) == 1);
}

} // namespace

TEST_CASE(sdl_keyboard_mouse_text_and_touch_feed_canonical_devices) {
    DeviceManager devices;
    CHECK(devices.initialize(hiddenConfig()));
    devices.textInput().setEnabled(true);

    pushKey(SDL_KEYDOWN, SDL_SCANCODE_W);

    SDL_Event motion{};
    motion.type = SDL_MOUSEMOTION;
    motion.motion.x = 120;
    motion.motion.y = 80;
    CHECK(SDL_PushEvent(&motion) == 1);

    SDL_Event button{};
    button.type = SDL_MOUSEBUTTONDOWN;
    button.button.button = SDL_BUTTON_LEFT;
    CHECK(SDL_PushEvent(&button) == 1);

    SDL_Event wheel{};
    wheel.type = SDL_MOUSEWHEEL;
    wheel.wheel.y = 2;
    wheel.wheel.preciseY = 2.5f;
    CHECK(SDL_PushEvent(&wheel) == 1);

    SDL_Event editing{};
    editing.type = SDL_TEXTEDITING;
    constexpr char composition[] = "abc";
    std::memcpy(editing.edit.text, composition, sizeof(composition));
    editing.edit.start = 2;
    CHECK(SDL_PushEvent(&editing) == 1);

    SDL_Event text{};
    text.type = SDL_TEXTINPUT;
    constexpr char committed[] = "Z";
    std::memcpy(text.text.text, committed, sizeof(committed));
    CHECK(SDL_PushEvent(&text) == 1);

    SDL_Event finger{};
    finger.type = SDL_FINGERDOWN;
    finger.tfinger.fingerId = 42;
    finger.tfinger.x = 0.25f;
    finger.tfinger.y = 0.5f;
    CHECK(SDL_PushEvent(&finger) == 1);

    devices.pollEvents();

    CHECK(devices.keyboard()->isKeyPressed(KeyCode::W));
    CHECK(devices.keyboard()->isKeyJustPressed(KeyCode::W));
    CHECK(devices.mouse()->getPosition().x == 120.0f);
    CHECK(devices.mouse()->getPosition().y == 80.0f);
    CHECK(devices.mouse()->isButtonJustPressed(MouseButton::Left));
    CHECK_FLOAT_EQ(devices.mouse()->getWheelDelta(), 2.5f, 1e-5f);
    CHECK(devices.textInput().getText() == "Z");
    CHECK(!devices.textInput().isComposing());
    const TouchPoint* touch = devices.touch()->getTouchById(42);
    CHECK(touch != nullptr);
    CHECK(touch->phase == TouchPhase::Began);
    CHECK(touch->position.x == 160.0f);
    CHECK(touch->position.y == 240.0f);

    devices.shutdown();
}

TEST_CASE(sdl_focus_loss_releases_held_input) {
    DeviceManager devices;
    CHECK(devices.initialize(hiddenConfig()));

    pushKey(SDL_KEYDOWN, SDL_SCANCODE_SPACE);
    SDL_Event button{};
    button.type = SDL_MOUSEBUTTONDOWN;
    button.button.button = SDL_BUTTON_RIGHT;
    CHECK(SDL_PushEvent(&button) == 1);
    devices.pollEvents();
    CHECK(devices.keyboard()->isKeyPressed(KeyCode::Space));
    CHECK(devices.mouse()->isButtonPressed(MouseButton::Right));

    SDL_Event focus{};
    focus.type = SDL_WINDOWEVENT;
    focus.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    CHECK(SDL_PushEvent(&focus) == 1);
    devices.pollEvents();

    CHECK(devices.keyboard()->isKeyJustReleased(KeyCode::Space));
    CHECK(devices.mouse()->isButtonJustReleased(MouseButton::Right));

    devices.shutdown();
}

TEST_CASE(sdl_relative_mouse_uses_relative_delta) {
    DeviceManager devices;
    CHECK(devices.initialize(hiddenConfig()));
    // L3 (2026-08-26): RelativeMouseResult — accept any non-Disabled as
    // success on the SDL2 backend.
    using R = WindowManager::RelativeMouseResult;
    CHECK(devices.window().setRelativeMouseMode(true) != R::Disabled);

    SDL_Event motion{};
    motion.type = SDL_MOUSEMOTION;
    motion.motion.x = 300;
    motion.motion.y = 200;
    motion.motion.xrel = 7;
    motion.motion.yrel = -4;
    CHECK(SDL_PushEvent(&motion) == 1);
    devices.pollEvents();

    CHECK(devices.mouse()->getDelta().x == 7.0f);
    CHECK(devices.mouse()->getDelta().y == -4.0f);

    devices.window().setRelativeMouseMode(false);
    devices.shutdown();
}

TEST_SUITE_END
