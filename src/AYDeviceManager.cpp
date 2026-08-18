#include "AYDevice/DeviceManager.h"

#include <algorithm>
#include <cstring>

#if defined(AY_DEVICE_USE_SDL2)
#  include <SDL.h>
#endif

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <Windows.h>
#endif

namespace ayt::device {

#if defined(AY_DEVICE_USE_SDL2)
namespace {

KeyCode translateSdlKey(SDL_Scancode code)
{
    if (code >= SDL_SCANCODE_A && code <= SDL_SCANCODE_Z) {
        return static_cast<KeyCode>(static_cast<int>(KeyCode::A)
                                  + (code - SDL_SCANCODE_A));
    }
    if (code >= SDL_SCANCODE_1 && code <= SDL_SCANCODE_9) {
        return static_cast<KeyCode>(static_cast<int>(KeyCode::Num1)
                                  + (code - SDL_SCANCODE_1));
    }
    if (code >= SDL_SCANCODE_F1 && code <= SDL_SCANCODE_F12) {
        return static_cast<KeyCode>(static_cast<int>(KeyCode::F1)
                                  + (code - SDL_SCANCODE_F1));
    }
    if (code >= SDL_SCANCODE_KP_1 && code <= SDL_SCANCODE_KP_9) {
        return static_cast<KeyCode>(static_cast<int>(KeyCode::Kp1)
                                  + (code - SDL_SCANCODE_KP_1));
    }

    switch (code) {
    case SDL_SCANCODE_0:             return KeyCode::Num0;
    case SDL_SCANCODE_ESCAPE:        return KeyCode::Escape;
    case SDL_SCANCODE_RETURN:        return KeyCode::Enter;
    case SDL_SCANCODE_TAB:           return KeyCode::Tab;
    case SDL_SCANCODE_SPACE:         return KeyCode::Space;
    case SDL_SCANCODE_BACKSPACE:     return KeyCode::Backspace;
    case SDL_SCANCODE_INSERT:        return KeyCode::Insert;
    case SDL_SCANCODE_DELETE:        return KeyCode::Delete;
    case SDL_SCANCODE_HOME:          return KeyCode::Home;
    case SDL_SCANCODE_END:           return KeyCode::End;
    case SDL_SCANCODE_PAGEUP:        return KeyCode::PageUp;
    case SDL_SCANCODE_PAGEDOWN:      return KeyCode::PageDown;
    case SDL_SCANCODE_LEFT:          return KeyCode::Left;
    case SDL_SCANCODE_RIGHT:         return KeyCode::Right;
    case SDL_SCANCODE_UP:            return KeyCode::Up;
    case SDL_SCANCODE_DOWN:          return KeyCode::Down;
    case SDL_SCANCODE_LSHIFT:        return KeyCode::LeftShift;
    case SDL_SCANCODE_RSHIFT:        return KeyCode::RightShift;
    case SDL_SCANCODE_LCTRL:         return KeyCode::LeftControl;
    case SDL_SCANCODE_RCTRL:         return KeyCode::RightControl;
    case SDL_SCANCODE_LALT:          return KeyCode::LeftAlt;
    case SDL_SCANCODE_RALT:          return KeyCode::RightAlt;
    case SDL_SCANCODE_LGUI:          return KeyCode::LeftSuper;
    case SDL_SCANCODE_RGUI:          return KeyCode::RightSuper;
    case SDL_SCANCODE_MINUS:         return KeyCode::Minus;
    case SDL_SCANCODE_EQUALS:        return KeyCode::Equal;
    case SDL_SCANCODE_LEFTBRACKET:   return KeyCode::LeftBracket;
    case SDL_SCANCODE_RIGHTBRACKET:  return KeyCode::RightBracket;
    case SDL_SCANCODE_BACKSLASH:
    case SDL_SCANCODE_NONUSBACKSLASH:return KeyCode::Backslash;
    case SDL_SCANCODE_SEMICOLON:     return KeyCode::Semicolon;
    case SDL_SCANCODE_APOSTROPHE:    return KeyCode::Apostrophe;
    case SDL_SCANCODE_COMMA:         return KeyCode::Comma;
    case SDL_SCANCODE_PERIOD:        return KeyCode::Period;
    case SDL_SCANCODE_SLASH:         return KeyCode::Slash;
    case SDL_SCANCODE_GRAVE:         return KeyCode::GraveAccent;
    case SDL_SCANCODE_KP_0:          return KeyCode::Kp0;
    case SDL_SCANCODE_KP_PERIOD:     return KeyCode::KpDecimal;
    case SDL_SCANCODE_KP_DIVIDE:     return KeyCode::KpDivide;
    case SDL_SCANCODE_KP_MULTIPLY:   return KeyCode::KpMultiply;
    case SDL_SCANCODE_KP_MINUS:      return KeyCode::KpSubtract;
    case SDL_SCANCODE_KP_PLUS:       return KeyCode::KpAdd;
    case SDL_SCANCODE_KP_ENTER:      return KeyCode::KpEnter;
    case SDL_SCANCODE_CAPSLOCK:      return KeyCode::CapsLock;
    case SDL_SCANCODE_NUMLOCKCLEAR:  return KeyCode::NumLock;
    case SDL_SCANCODE_SCROLLLOCK:    return KeyCode::ScrollLock;
    default:                         return KeyCode::Unknown;
    }
}

bool translateSdlMouseButton(Uint8 native, MouseButton& button)
{
    switch (native) {
    case SDL_BUTTON_LEFT:   button = MouseButton::Left; return true;
    case SDL_BUTTON_RIGHT:  button = MouseButton::Right; return true;
    case SDL_BUTTON_MIDDLE: button = MouseButton::Middle; return true;
    case SDL_BUTTON_X1:     button = MouseButton::X1; return true;
    case SDL_BUTTON_X2:     button = MouseButton::X2; return true;
    default: return false;
    }
}

int utf8ByteOffset(const char* text, int characterOffset)
{
    if (text == nullptr || characterOffset <= 0) {
        return 0;
    }
    int bytes = 0;
    int characters = 0;
    while (text[bytes] != '\0' && characters < characterOffset) {
        const unsigned char lead = static_cast<unsigned char>(text[bytes]);
        int width = 1;
        if ((lead & 0xE0u) == 0xC0u) width = 2;
        else if ((lead & 0xF0u) == 0xE0u) width = 3;
        else if ((lead & 0xF8u) == 0xF0u) width = 4;
        for (int i = 0; i < width && text[bytes] != '\0'; ++i) {
            ++bytes;
        }
        ++characters;
    }
    return bytes;
}

} // namespace
#endif

DeviceManager::DeviceManager() = default;

DeviceManager::~DeviceManager()
{
    shutdown();
}

bool DeviceManager::initialize(const DeviceConfig& config)
{
    if (_initialized) {
        return true;
    }

    if (!_windowManager.createWindow(config.window)) {
        return false;
    }

    _keyboardEnabled = config.enableKeyboard;
    _mouseEnabled = config.enableMouse;
    _gamepadEnabled = config.enableGamepad;
    _touchEnabled = config.enableTouch;

    _mapping.setKeyboard(_keyboardEnabled ? &_keyboard : nullptr);
    _mapping.setMouse(_mouseEnabled ? &_mouse : nullptr);
    _mapping.setGamepad(_gamepadEnabled ? &_gamepads[0] : nullptr);

    if (_touchEnabled) {
        _windowManager.setTouchEnabled(true);
    }

    wireInputCallbacks();

    if (_gamepadEnabled) {
        initializePlatformGamepads();
    }

    _initialized = true;
    return true;
}

void DeviceManager::wireInputCallbacks()
{
    if (_keyboardEnabled) {
        _windowManager.setKeyCallback([this](KeyCode key, bool pressed) {
            if (pressed) {
                _keyboard.onKeyDown(key);
            } else {
                _keyboard.onKeyUp(key);
            }
        });
    }

    if (_mouseEnabled) {
        _windowManager.setMouseButtonCallback([this](MouseButton button, bool pressed) {
            if (pressed) {
                _mouse.onButtonDown(button);
            } else {
                _mouse.onButtonUp(button);
            }
        });
        _windowManager.setMouseMoveCallback([this](float x, float y) {
            _mouse.onMove(x, y);
        });
        _windowManager.setMouseDeltaCallback([this](float deltaX, float deltaY) {
            _mouse.onRelativeMove(deltaX, deltaY);
        });
        _windowManager.setMouseWheelCallback([this](float delta) {
            _mouse.onWheel(delta);
        });
    }

    if (_touchEnabled) {
        _windowManager.setTouchCallback([this](int64_t id, float x, float y, TouchPhase phase) {
            _touch.onTouch(id, x, y, phase);
        });
    }

    // Text/IME is always wired; TextInput itself gates on setEnabled().
    _windowManager.setCharCallback([this](const char* utf8, int byteCount) {
        _textInput.onChar(utf8, byteCount);
    });
    _windowManager.setCompositionCallback([this](const char* utf8, int byteCount, int cursor) {
        if (utf8 == nullptr && byteCount < 0) {
            _textInput.endComposition();
        } else {
            _textInput.onComposition(utf8, byteCount, cursor);
        }
    });
    _textInput.setEnabledChangedCallback([this](bool enabled) {
        _windowManager.setTextInputEnabled(enabled);
    });

    _windowManager.setInputResetCallback([this]() {
        if (_keyboardEnabled) {
            _keyboard.releaseAll();
        }
        if (_mouseEnabled) {
            _mouse.releaseAllButtons();
        }
        if (_touchEnabled) {
            _touch.cancelAll();
        }
        if (_textInput.isComposing()) {
            _textInput.endComposition();
        }
    });
}

GamepadDevice* DeviceManager::gamepad(int slot)
{
    if (!_gamepadEnabled || slot < 0 || slot >= kMaxGamepads) {
        return nullptr;
    }
    return &_gamepads[slot];
}

const GamepadDevice* DeviceManager::gamepad(int slot) const
{
    if (!_gamepadEnabled || slot < 0 || slot >= kMaxGamepads) {
        return nullptr;
    }
    return &_gamepads[slot];
}

void DeviceManager::shutdown()
{
    if (!_initialized) {
        return;
    }

    shutdownPlatformGamepads();
    _textInput.setEnabled(false);
    _windowManager.destroyWindow();
    _keyboard.reset();
    _mouse.reset();
    for (GamepadDevice& pad : _gamepads) {
        pad.reset();
    }
    _touch.reset();
    _textInput.reset();
    _keyboardEnabled = false;
    _mouseEnabled = false;
    _gamepadEnabled = false;
    _touchEnabled = false;
    _initialized = false;
}

void DeviceManager::pollEvents()
{
    if (!_initialized) {
        return;
    }

    // Advance edge state before draining events so just-pressed / deltas are
    // measured relative to the previous frame.
    if (_keyboardEnabled) {
        _keyboard.newFrame();
    }
    if (_mouseEnabled) {
        _mouse.newFrame();
    }
    if (_gamepadEnabled) {
        for (GamepadDevice& pad : _gamepads) {
            pad.newFrame();
        }
    }
    if (_touchEnabled) {
        _touch.newFrame();
    }
    _textInput.newFrame();

#if defined(AY_DEVICE_USE_SDL2)
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            _windowManager.notifyClosed();
            break;
        case SDL_WINDOWEVENT:
            switch (event.window.event) {
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                _windowManager.notifyResized(event.window.data1, event.window.data2);
                break;
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                _windowManager.notifyFocused(true);
                break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
                _windowManager.notifyFocused(false);
                break;
            case SDL_WINDOWEVENT_CLOSE:
                _windowManager.notifyClosed();
                break;
            default:
                break;
            }
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (_keyboardEnabled && event.key.repeat == 0) {
                const KeyCode key = translateSdlKey(event.key.keysym.scancode);
                if (key != KeyCode::Unknown) {
                    if (event.type == SDL_KEYDOWN) _keyboard.onKeyDown(key);
                    else _keyboard.onKeyUp(key);
                }
            }
            break;
        case SDL_MOUSEMOTION:
            if (_mouseEnabled) {
                if (_windowManager.isRelativeMouseMode()) {
                    _mouse.onRelativeMove(static_cast<float>(event.motion.xrel),
                                          static_cast<float>(event.motion.yrel));
                } else {
                    _mouse.onMove(static_cast<float>(event.motion.x),
                                  static_cast<float>(event.motion.y));
                }
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if (_mouseEnabled) {
                MouseButton button{};
                if (translateSdlMouseButton(event.button.button, button)) {
                    if (event.type == SDL_MOUSEBUTTONDOWN) _mouse.onButtonDown(button);
                    else _mouse.onButtonUp(button);
                }
            }
            break;
        case SDL_MOUSEWHEEL:
            if (_mouseEnabled) {
#if SDL_VERSION_ATLEAST(2, 0, 18)
                float delta = event.wheel.preciseY;
#else
                float delta = static_cast<float>(event.wheel.y);
#endif
                if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                    delta = -delta;
                }
                _mouse.onWheel(delta);
            }
            break;
        case SDL_TEXTINPUT:
            if (_textInput.isComposing()) {
                _textInput.endComposition();
            }
            _textInput.onChar(event.text.text,
                              static_cast<int>(std::strlen(event.text.text)));
            break;
        case SDL_TEXTEDITING: {
            const int bytes = static_cast<int>(std::strlen(event.edit.text));
            if (bytes == 0) {
                _textInput.endComposition();
            } else {
                _textInput.onComposition(event.edit.text, bytes,
                                         utf8ByteOffset(event.edit.text, event.edit.start));
            }
            break;
        }
        case SDL_FINGERDOWN:
        case SDL_FINGERMOTION:
        case SDL_FINGERUP:
            if (_touchEnabled) {
                TouchPhase phase = TouchPhase::Moved;
                if (event.type == SDL_FINGERDOWN) phase = TouchPhase::Began;
                else if (event.type == SDL_FINGERUP) phase = TouchPhase::Ended;
                _touch.onTouch(static_cast<int64_t>(event.tfinger.fingerId),
                               event.tfinger.x * static_cast<float>(_windowManager.getWidth()),
                               event.tfinger.y * static_cast<float>(_windowManager.getHeight()),
                               phase);
            }
            break;
        case SDL_CONTROLLERDEVICEADDED:
            if (_gamepadEnabled) openPlatformGamepad(event.cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            if (_gamepadEnabled) closePlatformGamepad(event.cdevice.which);
            break;
        default:
            break;
        }
    }

#elif defined(_WIN32)
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
#endif

    // Gamepad state is sampled once per host frame (XInput or SDL controller).
    // SDL device events above only attach/detach controller handles.
    if (_gamepadEnabled) {
#if defined(AY_DEVICE_USE_SDL2)
        SDL_GameControllerUpdate();
#endif
        for (GamepadDevice& pad : _gamepads) {
            pad.poll();
        }
    }
}

void DeviceManager::initializePlatformGamepads()
{
#if defined(AY_DEVICE_USE_SDL2)
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        return;
    }
    const int count = SDL_NumJoysticks();
    for (int deviceIndex = 0; deviceIndex < count; ++deviceIndex) {
        openPlatformGamepad(deviceIndex);
    }
#endif
}

void DeviceManager::shutdownPlatformGamepads()
{
#if defined(AY_DEVICE_USE_SDL2)
    for (int slot = 0; slot < kMaxGamepads; ++slot) {
        auto* controller = static_cast<SDL_GameController*>(_platformGamepads[slot]);
        _gamepads[slot].attachPlatformController(nullptr);
        if (controller != nullptr) {
            SDL_GameControllerClose(controller);
        }
        _platformGamepads[slot] = nullptr;
        _platformGamepadIds[slot] = -1;
    }
    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    }
#endif
}

void DeviceManager::openPlatformGamepad(int deviceIndex)
{
#if defined(AY_DEVICE_USE_SDL2)
    if (!SDL_IsGameController(deviceIndex)) {
        return;
    }
    SDL_GameController* controller = SDL_GameControllerOpen(deviceIndex);
    if (controller == nullptr) {
        return;
    }
    SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller);
    const int32_t instanceId = static_cast<int32_t>(SDL_JoystickInstanceID(joystick));
    if (std::find(_platformGamepadIds.begin(), _platformGamepadIds.end(), instanceId)
        != _platformGamepadIds.end()) {
        SDL_GameControllerClose(controller);
        return;
    }
    for (int slot = 0; slot < kMaxGamepads; ++slot) {
        if (_platformGamepads[slot] == nullptr) {
            _platformGamepads[slot] = controller;
            _platformGamepadIds[slot] = instanceId;
            _gamepads[slot].attachPlatformController(controller);
            return;
        }
    }
    SDL_GameControllerClose(controller);
#else
    (void)deviceIndex;
#endif
}

void DeviceManager::closePlatformGamepad(int32_t instanceId)
{
#if defined(AY_DEVICE_USE_SDL2)
    for (int slot = 0; slot < kMaxGamepads; ++slot) {
        if (_platformGamepadIds[slot] != instanceId) {
            continue;
        }
        auto* controller = static_cast<SDL_GameController*>(_platformGamepads[slot]);
        _gamepads[slot].attachPlatformController(nullptr);
        if (controller != nullptr) {
            SDL_GameControllerClose(controller);
        }
        _platformGamepads[slot] = nullptr;
        _platformGamepadIds[slot] = -1;
        return;
    }
#else
    (void)instanceId;
#endif
}

} // namespace ayt::device
