#include "AYDevice/InputProfileConfig.h"

#include "AYDevice/InputProfile.h"

#include <AYConfig.h>

#include <string>
#include <string_view>
#include <vector>

namespace ayt::device::input_profile_config {

namespace {

constexpr const char* kNameKey = "Input.Profile.Name";
constexpr std::string_view kActionPrefix = "Input.Actions.";
constexpr std::string_view kAxisPrefix = "Input.Axes.";
constexpr std::string_view kAxisScalePrefix = "Input.AxesScale.";

std::vector<std::string> splitCsv(const std::string& text)
{
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t comma = text.find(',', start);
        const size_t end = comma == std::string::npos ? text.size() : comma;
        std::string_view token(text.data() + start, end - start);
        // Trim surrounding spaces for hand-edited files.
        while (!token.empty() && token.front() == ' ') { token.remove_prefix(1); }
        while (!token.empty() && token.back() == ' ') { token.remove_suffix(1); }
        if (!token.empty()) {
            out.emplace_back(token);
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return out;
}

std::string joinCsv(const std::vector<std::string>& tokens)
{
    std::string out;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i != 0) {
            out += ',';
        }
        out += tokens[i];
    }
    return out;
}

} // namespace

int load(const ayt::config::Config& config, InputProfile& profile)
{
    profile.clear();

    profile.name = config.getString(kNameKey, "Default");

    int count = 0;

    for (const std::string& key : config.getKeysMatching(std::string(kActionPrefix))) {
        const std::string action = key.substr(kActionPrefix.size());
        if (action.empty()) {
            continue;
        }
        std::vector<std::string> tokens = splitCsv(config.getString(key));
        if (!tokens.empty()) {
            profile.setAction(action, std::move(tokens));
            ++count;
        }
    }

    for (const std::string& key : config.getKeysMatching(std::string(kAxisPrefix))) {
        const std::string axis = key.substr(kAxisPrefix.size());
        if (axis.empty()) {
            continue;
        }
        std::vector<std::string> tokens = splitCsv(config.getString(key));
        if (tokens.empty()) {
            continue;
        }
        const std::string scaleKey = std::string(kAxisScalePrefix) + axis;
        const float scale = static_cast<float>(config.getFloat(scaleKey, 1.0));
        profile.setAxis(axis, std::move(tokens), scale);
        ++count;
    }

    return count;
}

void store(const InputProfile& profile, ayt::config::Config& config)
{
    config.setString(kNameKey, profile.name);

    for (const std::string& action : profile.actionNames()) {
        const std::vector<std::string>* tokens = profile.actionTokens(action);
        if (tokens != nullptr) {
            config.setString(std::string(kActionPrefix) + action, joinCsv(*tokens));
        }
    }

    for (const std::string& axis : profile.axisNames()) {
        const std::vector<std::string>* tokens = profile.axisTokens(axis);
        if (tokens != nullptr) {
            config.setString(std::string(kAxisPrefix) + axis, joinCsv(*tokens));
            config.setFloat(std::string(kAxisScalePrefix) + axis, profile.axisScale(axis));
        }
    }
}

} // namespace ayt::device::input_profile_config
