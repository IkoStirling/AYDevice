#include "AYDevice/GamepadDevice.h"

#include <algorithm>
#include <cmath>

#if defined(AY_DEVICE_USE_SDL2)
#  include <SDL.h>
#endif

#if defined(_WIN32) && !defined(AY_DEVICE_USE_SDL2)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <Windows.h>
#  include <Xinput.h>
#  define AY_DEVICE_XINPUT 1
#endif

namespace ayt::device {

namespace {

// XInput stick range is int16; triggers are uint8. Deadzones follow the
// XInput SDK recommendations.
constexpr float kStickMax = 32767.0f;
constexpr float kTriggerMax = 255.0f;
constexpr float kLeftStickDeadzone = 7849.0f / kStickMax;
constexpr float kRightStickDeadzone = 8689.0f / kStickMax;
constexpr float kTriggerThreshold = 30.0f / kTriggerMax;

// Apply a radial-ish per-axis deadzone and rescale the remainder to 0..1.
float applyDeadzone(float value, float deadzone)
{
    const float magnitude = std::fabs(value);
    if (magnitude <= deadzone) {
        return 0.0f;
    }
    const float sign = value < 0.0f ? -1.0f : 1.0f;
    const float scaled = (magnitude - deadzone) / (1.0f - deadzone);
    return sign * std::clamp(scaled, 0.0f, 1.0f);
}

#if defined(AY_DEVICE_XINPUT)
GamepadButton xinputButtonAt(int i)
{
    return static_cast<GamepadButton>(i);
}

// Bitmask lookup parallel to GamepadButton ordering.
WORD xinputButtonMask(GamepadButton button)
{
    switch (button) {
    case GamepadButton::A:           return XINPUT_GAMEPAD_A;
    case GamepadButton::B:           return XINPUT_GAMEPAD_B;
    case GamepadButton::X:           return XINPUT_GAMEPAD_X;
    case GamepadButton::Y:           return XINPUT_GAMEPAD_Y;
    case GamepadButton::LeftBumper:  return XINPUT_GAMEPAD_LEFT_SHOULDER;
    case GamepadButton::RightBumper: return XINPUT_GAMEPAD_RIGHT_SHOULDER;
    case GamepadButton::Back:        return XINPUT_GAMEPAD_BACK;
    case GamepadButton::Start:       return XINPUT_GAMEPAD_START;
    case GamepadButton::Guide:       return 0;  // not exposed by public XInput
    case GamepadButton::LeftStick:   return XINPUT_GAMEPAD_LEFT_THUMB;
    case GamepadButton::RightStick:  return XINPUT_GAMEPAD_RIGHT_THUMB;
    case GamepadButton::DpadUp:      return XINPUT_GAMEPAD_DPAD_UP;
    case GamepadButton::DpadDown:    return XINPUT_GAMEPAD_DPAD_DOWN;
    case GamepadButton::DpadLeft:    return XINPUT_GAMEPAD_DPAD_LEFT;
    case GamepadButton::DpadRight:   return XINPUT_GAMEPAD_DPAD_RIGHT;
    default:                         return 0;
    }
}
#elif defined(AY_DEVICE_USE_SDL2)
SDL_GameControllerButton sdlButton(GamepadButton button)
{
    switch (button) {
    case GamepadButton::A:           return SDL_CONTROLLER_BUTTON_A;
    case GamepadButton::B:           return SDL_CONTROLLER_BUTTON_B;
    case GamepadButton::X:           return SDL_CONTROLLER_BUTTON_X;
    case GamepadButton::Y:           return SDL_CONTROLLER_BUTTON_Y;
    case GamepadButton::LeftBumper:  return SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
    case GamepadButton::RightBumper: return SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
    case GamepadButton::Back:        return SDL_CONTROLLER_BUTTON_BACK;
    case GamepadButton::Start:       return SDL_CONTROLLER_BUTTON_START;
    case GamepadButton::Guide:       return SDL_CONTROLLER_BUTTON_GUIDE;
    case GamepadButton::LeftStick:   return SDL_CONTROLLER_BUTTON_LEFTSTICK;
    case GamepadButton::RightStick:  return SDL_CONTROLLER_BUTTON_RIGHTSTICK;
    case GamepadButton::DpadUp:      return SDL_CONTROLLER_BUTTON_DPAD_UP;
    case GamepadButton::DpadDown:    return SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    case GamepadButton::DpadLeft:    return SDL_CONTROLLER_BUTTON_DPAD_LEFT;
    case GamepadButton::DpadRight:   return SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
    default:                         return SDL_CONTROLLER_BUTTON_INVALID;
    }
}

float normalizeSdlAxis(Sint16 value)
{
    return value < 0 ? static_cast<float>(value) / 32768.0f
                     : static_cast<float>(value) / 32767.0f;
}
#endif

} // namespace

GamepadDevice::GamepadDevice(int slot) : _slot(slot)
{
}

int GamepadDevice::buttonIndex(GamepadButton button)
{
    const int i = static_cast<int>(button);
    if (i < 0 || i >= kGamepadButtonCount) {
        return 0;
    }
    return i;
}

int GamepadDevice::axisIndex(GamepadAxis axis)
{
    const int i = static_cast<int>(axis);
    if (i < 0 || i >= kGamepadAxisCount) {
        return 0;
    }
    return i;
}

void GamepadDevice::newFrame()
{
    _previous = _current;
}

void GamepadDevice::poll()
{
#if defined(AY_DEVICE_XINPUT)
    XINPUT_STATE state{};
    const DWORD result = XInputGetState(static_cast<DWORD>(_slot), &state);
    if (result != ERROR_SUCCESS) {
        // L14 (2026-08-26): distinguish disconnected from busy.
        // ERROR_DEVICE_NOT_CONNECTED is the canonical "unplugged" code;
        // ERROR_INVALID_PARAMETER indicates the slot is out of range;
        // any other code (ERROR_EMPTY, ERROR_BUSY, ERROR_TIMEOUT) is a
        // transient failure and we leave _connected unchanged so the
        // UI does not flicker to "disconnected" on a single slow poll.
        if (result == ERROR_DEVICE_NOT_CONNECTED
            || result == ERROR_INVALID_PARAMETER) {
            if (_connected) {
                _connected = false;
                _current.fill(false);
                _axes.fill(0.0f);
            }
        }
        // Otherwise: transient — keep last state. Caller may still see
        // stale buttons held, but next successful poll will refresh.
        return;
    }

    _connected = true;
    const XINPUT_GAMEPAD& pad = state.Gamepad;

    for (int i = 0; i < kGamepadButtonCount; ++i) {
        const GamepadButton button = xinputButtonAt(i);
        const WORD mask = xinputButtonMask(button);
        _current[i] = mask != 0 && (pad.wButtons & mask) != 0;
    }

    _axes[axisIndex(GamepadAxis::LeftX)] =
        applyDeadzone(static_cast<float>(pad.sThumbLX) / kStickMax, kLeftStickDeadzone);
    _axes[axisIndex(GamepadAxis::LeftY)] =
        applyDeadzone(static_cast<float>(pad.sThumbLY) / kStickMax, kLeftStickDeadzone);
    _axes[axisIndex(GamepadAxis::RightX)] =
        applyDeadzone(static_cast<float>(pad.sThumbRX) / kStickMax, kRightStickDeadzone);
    _axes[axisIndex(GamepadAxis::RightY)] =
        applyDeadzone(static_cast<float>(pad.sThumbRY) / kStickMax, kRightStickDeadzone);
    _axes[axisIndex(GamepadAxis::LeftTrigger)] =
        applyDeadzone(static_cast<float>(pad.bLeftTrigger) / kTriggerMax, kTriggerThreshold);
    _axes[axisIndex(GamepadAxis::RightTrigger)] =
        applyDeadzone(static_cast<float>(pad.bRightTrigger) / kTriggerMax, kTriggerThreshold);
#elif defined(AY_DEVICE_USE_SDL2)
    auto* controller = static_cast<SDL_GameController*>(_platformController);
    if (controller == nullptr || SDL_GameControllerGetAttached(controller) == SDL_FALSE) {
        setConnected(false);
        return;
    }

    _connected = true;
    for (int i = 0; i < kGamepadButtonCount; ++i) {
        const auto button = sdlButton(static_cast<GamepadButton>(i));
        _current[i] = button != SDL_CONTROLLER_BUTTON_INVALID
                   && SDL_GameControllerGetButton(controller, button) != 0;
    }

    const auto axis = [controller](SDL_GameControllerAxis which) {
        return normalizeSdlAxis(SDL_GameControllerGetAxis(controller, which));
    };
    _axes[axisIndex(GamepadAxis::LeftX)] =
        applyDeadzone(axis(SDL_CONTROLLER_AXIS_LEFTX), kLeftStickDeadzone);
    _axes[axisIndex(GamepadAxis::LeftY)] =
        applyDeadzone(-axis(SDL_CONTROLLER_AXIS_LEFTY), kLeftStickDeadzone);
    _axes[axisIndex(GamepadAxis::RightX)] =
        applyDeadzone(axis(SDL_CONTROLLER_AXIS_RIGHTX), kRightStickDeadzone);
    _axes[axisIndex(GamepadAxis::RightY)] =
        applyDeadzone(-axis(SDL_CONTROLLER_AXIS_RIGHTY), kRightStickDeadzone);
    _axes[axisIndex(GamepadAxis::LeftTrigger)] =
        applyDeadzone(std::max(0.0f, axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT)),
                      kTriggerThreshold);
    _axes[axisIndex(GamepadAxis::RightTrigger)] =
        applyDeadzone(std::max(0.0f, axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT)),
                      kTriggerThreshold);
#endif
}

bool GamepadDevice::isButtonPressed(GamepadButton button) const
{
    return _current[buttonIndex(button)];
}

bool GamepadDevice::isButtonJustPressed(GamepadButton button) const
{
    const int i = buttonIndex(button);
    return _current[i] && !_previous[i];
}

bool GamepadDevice::isButtonJustReleased(GamepadButton button) const
{
    const int i = buttonIndex(button);
    return !_current[i] && _previous[i];
}

void GamepadDevice::setVibration(float leftMotor, float rightMotor)
{
    if (!_connected) {
        return;
    }
#if defined(AY_DEVICE_XINPUT)
    XINPUT_VIBRATION vibration{};
    vibration.wLeftMotorSpeed =
        static_cast<WORD>(std::clamp(leftMotor, 0.0f, 1.0f) * 65535.0f);
    vibration.wRightMotorSpeed =
        static_cast<WORD>(std::clamp(rightMotor, 0.0f, 1.0f) * 65535.0f);
    XInputSetState(static_cast<DWORD>(_slot), &vibration);
#elif defined(AY_DEVICE_USE_SDL2)
    auto* controller = static_cast<SDL_GameController*>(_platformController);
    if (controller != nullptr) {
        SDL_GameControllerRumble(
            controller,
            static_cast<Uint16>(std::clamp(leftMotor, 0.0f, 1.0f) * 65535.0f),
            static_cast<Uint16>(std::clamp(rightMotor, 0.0f, 1.0f) * 65535.0f),
            0xFFFFFFFFu);
    }
#else
    (void)leftMotor;
    (void)rightMotor;
#endif
}

void GamepadDevice::attachPlatformController(void* controller)
{
    _platformController = controller;
    setConnected(controller != nullptr);
}

void GamepadDevice::setConnected(bool connected)
{
    _connected = connected;
    if (!connected) {
        _current.fill(false);
        _axes.fill(0.0f);
    }
}

void GamepadDevice::onButtonDown(GamepadButton button)
{
    _current[buttonIndex(button)] = true;
}

void GamepadDevice::onButtonUp(GamepadButton button)
{
    _current[buttonIndex(button)] = false;
}

void GamepadDevice::setAxis(GamepadAxis axis, float value)
{
    _axes[axisIndex(axis)] = std::clamp(value, -1.0f, 1.0f);
}

void GamepadDevice::reset()
{
    _platformController = nullptr;
    _connected = false;
    _current.fill(false);
    _previous.fill(false);
    _axes.fill(0.0f);
}

} // namespace ayt::device
