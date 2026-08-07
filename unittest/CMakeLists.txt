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

target_include_directories(${TEST_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # INT-03: AYAppEventHost.h (EventBusHostScope) lives here.
    ${CMAKE_CURRENT_SOURCE_DIR}/../AYApplication/interface
)

target_link_libraries(${TEST_NAME} PRIVATE
    AYDevice
    AYDeviceConfig
    AYDeviceSubSystem
    AYTest
    # INT-03: EventBusHostScope is owned by the bridge; AYApplication
    # provides the type + (transitively) AYEventSystem for the bus.
    AYApplication
)

ay_configure_test_tmpdir(${TEST_NAME})
add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
