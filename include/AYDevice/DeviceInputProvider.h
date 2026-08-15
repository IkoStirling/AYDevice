#pragma once
// AYDevice/DeviceInputProvider.h - INT-02 (2026-07-15)
//
// Adapter: Logia `LogiaRuntimeBridge::InputProvider` -> AYDevice
// `InputMapping`. Held by ScriptSubSystem via setInputProvider();
// raw pointer to DeviceManager (caller-owned lifetime — EditorApp
// member / DeviceSubSystem internal storage).
//
// Reads isActionPressed / isActionJustPressed / getAxisValue /
// isActionJustReleased by Action / Axis name string. Logia script
// form:
//   script PlayerController {
//       on_update() {
//           if (input.is_pressed("jump")) { jump() }
//           var dx: float = input.axis("move_x")
//           if (input.is_just_released("fire")) { release() }
//       }
//   }
//
// Lives in AYDeviceSubSystem target (not the core AYDevice library)
// because the core stays free of the AYScript dependency by design —
// see AYDevice/CMakeLists.txt § AYDeviceSubSystem comment block.

#include <AYScript/ScriptRuntimeBridge.h>

namespace ayt::device {

class DeviceManager;

class DeviceInputProvider final
    : public ayt::script::LogiaRuntimeBridge::InputProvider {
public:
    // Caller-owned `mgr` lifetime. nullptr is tolerated and means
    // every query returns false (matches MockInputProvider's
    // permissive "unknown key" behavior — the production invariant
    // matters during Editor transient state right after shutdown
    // begins). `axis` queries return 0.0f on nullptr.
    explicit DeviceInputProvider(DeviceManager* mgr) noexcept;

    bool isPressed(const std::string& action) const override;
    bool isJustPressed(const std::string& action) const override;
    // INT-03 (2026-07-15): axis + release edge — mirror Logia
    // ambient additions. Both delegate to InputMapping (Phase-2).
    float getAxisValue(const std::string& action) const override;
    bool isJustReleased(const std::string& action) const override;
    // M1 (2026-07-15): 2-axis read. Delegates to
    // InputMapping::getAxis2D when the named 2-axis has been bound
    // via bindAxis2D. On nullptr manager or unbound name, returns
    // false (outX/outY set to 0.0). Caller (Logia bridge) treats
    // false as "fall back to {0, 0}" which is the desired safe
    // default — matches MockInputProvider's permissive posture.
    bool getAxisValue2D(const std::string& action,
                        double& outX, double& outY) const override;

private:
    DeviceManager* _mgr; // not owned
};

} // namespace ayt::device