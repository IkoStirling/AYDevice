#pragma once
// AYDeviceInputProvider.h - INT-02 (2026-07-15)
//
// Adapter: Logia `LogiaRuntimeBridge::InputProvider` -> AYDevice
// `InputMapping`. Held by ScriptSubSystem via setInputProvider();
// raw pointer to DeviceManager (caller-owned lifetime — EditorApp
// member / DeviceSubSystem internal storage).
//
// Reads isActionPressed / isActionJustPressed by Action name
// string. Logia script form:
//   script PlayerController {
//       on_update() {
//           if (input.is_pressed("jump")) { jump() }
//       }
//   }
//
// Lives in AYDeviceSubSystem target (not the core AYDevice library)
// because the core stays free of the AYScript dependency by design —
// see AYDevice/CMakeLists.txt § AYDeviceSubSystem comment block.

#include <AYScriptRuntimeBridge.h>

namespace ayt::device {

class DeviceManager;

class DeviceInputProvider final
    : public ayt::script::LogiaRuntimeBridge::InputProvider {
public:
    // Caller-owned `mgr` lifetime. nullptr is tolerated and means
    // every query returns false (matches MockInputProvider's
    // permissive "unknown key" behavior — the production invariant
    // matters during Editor transient state right after shutdown
    // begins).
    explicit DeviceInputProvider(DeviceManager* mgr) noexcept;

    bool isPressed(const std::string& action) const override;
    bool isJustPressed(const std::string& action) const override;

private:
    DeviceManager* _mgr; // not owned
};

} // namespace ayt::device