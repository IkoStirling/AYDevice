#include "AYTest.h"
#include "AYDevice/InputProfile.h"
#include "AYDevice/InputProfileConfig.h"

#include <AYConfig.h>

using namespace ayt::device;

TEST_SUITE(AYDevice_InputProfileConfig)

TEST_CASE(test_profile_config_store_load_roundtrip) {
    InputProfile original;
    original.name = "MyScheme";
    original.setAction("Jump", {"Space", "Pad:A"});
    original.setAction("Fire", {"Mouse:Left"});
    original.setAxis("MoveX", {"A/D", "PadAxis:LeftX"}, 1.5f);

    ayt::config::Config config;
    input_profile_config::store(original, config);

    // Reload into a fresh profile.
    InputProfile loaded;
    const int count = input_profile_config::load(config, loaded);
    CHECK(count == 3);  // 2 actions + 1 axis
    CHECK(loaded.name == "MyScheme");

    const auto* jump = loaded.actionTokens("Jump");
    CHECK(jump != nullptr);
    CHECK(jump->size() == 2);
    CHECK((*jump)[0] == "Space");
    CHECK((*jump)[1] == "Pad:A");

    const auto* moveX = loaded.axisTokens("MoveX");
    CHECK(moveX != nullptr);
    CHECK(moveX->size() == 2);
    CHECK(loaded.axisScale("MoveX") == 1.5f);
}

TEST_CASE(test_profile_config_json_roundtrip) {
    InputProfile original = InputProfile::makeDefault();

    ayt::config::Config config;
    input_profile_config::store(original, config);

    // Serialize to JSON and back through a second Config.
    const std::string json = config.toJSON();
    CHECK(!json.empty());

    ayt::config::Config reparsed;
    reparsed.fromJSON(json);

    InputProfile loaded;
    input_profile_config::load(reparsed, loaded);
    CHECK(loaded.actionTokens("Jump") != nullptr);
    CHECK(loaded.axisTokens("MoveX") != nullptr);
}

TEST_CASE(test_profile_config_load_clears_existing) {
    ayt::config::Config config;
    config.setString("Input.Actions.OnlyOne", "Enter");

    InputProfile profile;
    profile.setAction("Stale", {"Space"});  // should be wiped by load()

    input_profile_config::load(config, profile);
    CHECK(profile.actionTokens("Stale") == nullptr);
    CHECK(profile.actionTokens("OnlyOne") != nullptr);
}

TEST_CASE(test_profile_config_hand_edited_spaces) {
    // Simulate a hand-edited config with spaces around CSV tokens.
    ayt::config::Config config;
    config.setString("Input.Actions.Jump", "Space, Pad:A , W");

    InputProfile profile;
    input_profile_config::load(config, profile);

    const auto* jump = profile.actionTokens("Jump");
    CHECK(jump != nullptr);
    CHECK(jump->size() == 3);
    CHECK((*jump)[1] == "Pad:A");  // trimmed
}

TEST_SUITE_END
