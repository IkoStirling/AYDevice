#pragma once
// AYInputProfileConfig.h - Bridge InputProfile <-> AYConfig ([Input.*] keys)

#include <string>

namespace ayt::config { class Config; }

namespace ayt::device {

class InputProfile;

// Loads / stores an InputProfile through AYConfig, using stable dot-keys:
//
//   Input.Profile.Name       = "Default"
//   Input.Actions.<Action>   = "Space,Pad:A"        (comma-joined tokens)
//   Input.Axes.<Axis>        = "A/D,PadAxis:LeftX"   (comma-joined tokens)
//   Input.AxesScale.<Axis>   = 1.0                    (optional; default 1.0)
//
// Values go through Config's string/float API so they are human-readable and
// hand-editable in both JSON and INI, and survive across layers (Engine
// default -> User override). This is the only unit that depends on AYConfig;
// the core input stack stays config-format agnostic.
namespace input_profile_config {

// Read every Input.Actions.* / Input.Axes.* key from config into profile.
// Existing profile contents are cleared first. Returns the number of bindings
// (actions + axes) loaded.
int load(const ayt::config::Config& config, InputProfile& profile);

// Write the profile into config under the Input.* keys. Does not save to disk;
// the caller owns config.save() / saveToFile().
void store(const InputProfile& profile, ayt::config::Config& config);

} // namespace input_profile_config

} // namespace ayt::device
