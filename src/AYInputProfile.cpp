#include "AYInputProfile.h"

#include "AYInputNames.h"
#include "AYInputMapping.h"

#include <charconv>

namespace ayt::device {

namespace {

constexpr std::string_view kMousePrefix = "Mouse:";
constexpr std::string_view kPadPrefix = "Pad:";
constexpr std::string_view kPadAxisPrefix = "PadAxis:";

bool startsWith(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

// Parse an optional "*scale" suffix; returns the token without the suffix.
std::string_view splitScale(std::string_view token, float& scale)
{
    scale = 1.0f;
    const size_t star = token.find('*');
    if (star == std::string_view::npos) {
        return token;
    }
    const std::string_view scaleText = token.substr(star + 1);
    float parsed = 1.0f;
    const auto* begin = scaleText.data();
    const auto* end = begin + scaleText.size();
    if (std::from_chars(begin, end, parsed).ec == std::errc{}) {
        scale = parsed;
    }
    return token.substr(0, star);
}

} // namespace

void InputProfile::setAction(std::string_view action, std::vector<std::string> tokens)
{
    _actions[std::string(action)] = std::move(tokens);
}

void InputProfile::addActionToken(std::string_view action, std::string token)
{
    _actions[std::string(action)].push_back(std::move(token));
}

void InputProfile::clearAction(std::string_view action)
{
    _actions.erase(std::string(action));
}

const std::vector<std::string>* InputProfile::actionTokens(std::string_view action) const
{
    auto it = _actions.find(std::string(action));
    return it != _actions.end() ? &it->second : nullptr;
}

void InputProfile::setAxis(std::string_view axis, std::vector<std::string> tokens, float scale)
{
    AxisEntry& entry = _axes[std::string(axis)];
    entry.tokens = std::move(tokens);
    entry.scale = scale;
}

void InputProfile::clearAxis(std::string_view axis)
{
    _axes.erase(std::string(axis));
}

const std::vector<std::string>* InputProfile::axisTokens(std::string_view axis) const
{
    auto it = _axes.find(std::string(axis));
    return it != _axes.end() ? &it->second.tokens : nullptr;
}

float InputProfile::axisScale(std::string_view axis) const
{
    auto it = _axes.find(std::string(axis));
    return it != _axes.end() ? it->second.scale : 1.0f;
}

std::vector<std::string> InputProfile::actionNames() const
{
    std::vector<std::string> names;
    names.reserve(_actions.size());
    for (const auto& [actionName, _] : _actions) {
        names.push_back(actionName);
    }
    return names;
}

std::vector<std::string> InputProfile::axisNames() const
{
    std::vector<std::string> names;
    names.reserve(_axes.size());
    for (const auto& [axisName, _] : _axes) {
        names.push_back(axisName);
    }
    return names;
}

void InputProfile::clear()
{
    _actions.clear();
    _axes.clear();
}

int InputProfile::applyTo(InputMapping& mapping) const
{
    int failed = 0;

    for (const auto& [action, tokens] : _actions) {
        std::vector<KeyCode>       keys;
        std::vector<MouseButton>   mouseButtons;
        std::vector<GamepadButton> padButtons;

        for (const std::string& token : tokens) {
            const std::string_view view = token;
            if (startsWith(view, kMousePrefix)) {
                MouseButton button;
                if (mouseButtonFromName(view.substr(kMousePrefix.size()), button)) {
                    mouseButtons.push_back(button);
                } else {
                    ++failed;
                }
            } else if (startsWith(view, kPadPrefix)) {
                GamepadButton button;
                if (gamepadButtonFromName(view.substr(kPadPrefix.size()), button)) {
                    padButtons.push_back(button);
                } else {
                    ++failed;
                }
            } else {
                const KeyCode key = keyCodeFromName(view);
                if (key != KeyCode::Unknown) {
                    keys.push_back(key);
                } else {
                    ++failed;
                }
            }
        }

        if (!keys.empty()) {
            mapping.bindAction(action, keys);
        }
        if (!mouseButtons.empty()) {
            mapping.bindActionMouse(action, mouseButtons);
        }
        if (!padButtons.empty()) {
            mapping.bindActionGamepad(action, padButtons);
        }
    }

    for (const auto& [axis, entry] : _axes) {
        std::vector<InputMapping::KeyPair> pairs;

        for (const std::string& token : entry.tokens) {
            const std::string_view view = token;
            if (startsWith(view, kPadAxisPrefix)) {
                float scale = 1.0f;
                const std::string_view axisPart =
                    splitScale(view.substr(kPadAxisPrefix.size()), scale);
                GamepadAxis gamepadAxis;
                if (gamepadAxisFromName(axisPart, gamepadAxis)) {
                    mapping.bindAxisGamepad(axis, gamepadAxis, scale);
                } else {
                    ++failed;
                }
                continue;
            }

            // Key pair "neg/pos"; either side may be empty.
            InputMapping::KeyPair pair{};
            const size_t slash = view.find('/');
            const std::string_view negName = slash == std::string_view::npos
                ? std::string_view{}
                : view.substr(0, slash);
            const std::string_view posName = slash == std::string_view::npos
                ? view
                : view.substr(slash + 1);

            bool ok = false;
            if (!negName.empty()) {
                pair.negative = keyCodeFromName(negName);
                ok = ok || pair.negative != KeyCode::Unknown;
            }
            if (!posName.empty()) {
                pair.positive = keyCodeFromName(posName);
                ok = ok || pair.positive != KeyCode::Unknown;
            }
            if (ok) {
                pairs.push_back(pair);
            } else {
                ++failed;
            }
        }

        if (!pairs.empty()) {
            mapping.bindAxis(axis, pairs, entry.scale);
        }
    }

    return failed;
}

InputProfile InputProfile::makeDefault()
{
    InputProfile profile;
    profile.name = "Default";
    profile.setAction("Jump", {"Space", "Pad:A"});
    profile.setAction("Fire", {"Mouse:Left", "Pad:RightBumper"});
    profile.setAxis("MoveX", {"A/D", "PadAxis:LeftX"});
    profile.setAxis("MoveY", {"S/W", "PadAxis:LeftY"});
    return profile;
}

} // namespace ayt::device
