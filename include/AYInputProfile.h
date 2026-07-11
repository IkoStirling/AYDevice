#pragma once
// AYInputProfile.h - Serializable player key bindings (rebind + apply to mapping)

#include "AYInputTypes.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ayt::device {

class InputMapping;

// A structured, serializable snapshot of Action / Axis bindings that a player
// can customize and persist. Kept free of any config-format dependency:
//   - applyTo(mapping)          -> push bindings into the live InputMapping
//   - actionTokens / axis*      -> neutral string tokens for a storage bridge
//
// Token grammar (see AYInputNames):
//   Action source : "Space" | "Mouse:Left" | "Pad:A"
//   Axis key pair : "A/D"   (negative/positive KeyCode names; empty side omitted)
//   Axis gamepad  : "PadAxis:LeftX" or "PadAxis:LeftX*0.5" (optional scale)
class InputProfile {
public:
    std::string name = "Default";

    // ===== Action bindings =====
    void setAction(std::string_view action, std::vector<std::string> tokens);
    void addActionToken(std::string_view action, std::string token);
    void clearAction(std::string_view action);
    const std::vector<std::string>* actionTokens(std::string_view action) const;

    // ===== Axis bindings =====
    void setAxis(std::string_view axis, std::vector<std::string> tokens, float scale = 1.0f);
    void clearAxis(std::string_view axis);
    const std::vector<std::string>* axisTokens(std::string_view axis) const;
    float axisScale(std::string_view axis) const;

    // ===== Enumeration =====
    std::vector<std::string> actionNames() const;
    std::vector<std::string> axisNames() const;
    bool empty() const { return _actions.empty() && _axes.empty(); }
    void clear();

    // Parse tokens and push all bindings into a live mapping. Unparseable tokens
    // are skipped. Returns the count of tokens that failed to parse.
    int applyTo(InputMapping& mapping) const;

    // A sensible default profile (WASD move, Space jump, mouse fire) for
    // bootstrapping when no user profile exists.
    static InputProfile makeDefault();

private:
    struct AxisEntry {
        std::vector<std::string> tokens;
        float                    scale = 1.0f;
    };

    std::unordered_map<std::string, std::vector<std::string>> _actions;
    std::unordered_map<std::string, AxisEntry>                _axes;
};

} // namespace ayt::device
