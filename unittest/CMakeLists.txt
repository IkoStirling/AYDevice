set(TEST_NAME AYDevice_Test)

add_executable(${TEST_NAME}
    TestMain.cpp
    Test_WindowManager.cpp
    Test_Input.cpp
    Test_Gamepad.cpp
    Test_InputProfile.cpp
    Test_InputProfileConfig.cpp
    Test_TouchText.cpp
    Test_DeviceSubSystem.cpp
    # INT-03 (2026-07-20): DeviceSubSystem -> EventBus bridge.
    Test_DeviceEventBridge.cpp
    # D5 (2026-07-26): WindowManager::createTopLevelWindow.
    Test_WindowManager_TopLevel.cpp
    # PR-InputTrace (2026-08-07): raw-input wheel bridge parser.
    Test_RawInputBridge.cpp
)

if(AY_DEVICE_USE_SDL2)
    # Keep backend translation tests isolated from AYDevice_Test's larger
    # GameLoop/EventSystem/AYScript dependency closure so SDL parity checks remain a
    # small incremental build.
    add_executable(AYDevice_SDL_Test TestMain.cpp Test_SdlInput.cpp)
    target_include_directories(AYDevice_SDL_Test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
    target_compile_definitions(AYDevice_SDL_Test PRIVATE AY_DEVICE_USE_SDL2)
    target_link_libraries(AYDevice_SDL_Test PRIVATE AYDevice AYTest SDL2::SDL2)
    ay_add_test(AYDevice_SDL_Test)
endif()

target_include_directories(${TEST_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(${TEST_NAME} PRIVATE
    AYDevice
    AYDeviceConfig
    AYDeviceSubSystem
    AYTest
    AYEventSystem
)

ay_add_test(${TEST_NAME})

# L25 (2026-08-26): AYDevice_Test is intentionally heavy. It pulls
# AYDeviceConfig (links AYConfig + transitive nlohmann_json),
# AYDeviceSubSystem (links AYGameLoop + AYScript for the Logia
# InputProvider seam + AYEventSystem for the EventBus bridge). This is what makes Test_DeviceSubSystem
# + Test_DeviceEventBridge + Test_InputProfileConfig possible. The
# alternative — splitting each bridge into its own executable — was
# considered and rejected: it duplicates TestMain.cpp N times and
# multiplies the bridge dependency link cost by N. Keep one integrated target.

# Fast backend/core regression target. It deliberately excludes Config and
# SubSystem bridge tests so changing a platform backend does not rebuild the
# GameLoop/EventSystem/AYScript dependency closure.

# L24 (2026-08-26): AYDevice_Core_Test scope. Backend input regression
# (Keyboard / Mouse / Gamepad / Text / Touch + WindowManager + the
# D5 top-level HWND path + raw-input wheel bridge parser). It does NOT
# cover InputProfileConfig (needs AYConfig) or DeviceSubSystem
# (needs AYGameLoop + AYScript + AYEventSystem). Use this target for
# the hot inner-loop during backend iterations; use AYDevice_Test for
# full-suite verification.
add_executable(AYDevice_Core_Test
    TestMain.cpp
    Test_WindowManager.cpp
    Test_Input.cpp
    Test_Gamepad.cpp
    Test_TouchText.cpp
    Test_WindowManager_TopLevel.cpp
    Test_RawInputBridge.cpp
)
target_include_directories(AYDevice_Core_Test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(AYDevice_Core_Test PRIVATE AYDevice AYTest)
ay_add_test(AYDevice_Core_Test)
