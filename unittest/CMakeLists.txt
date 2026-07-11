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
)

target_include_directories(${TEST_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(${TEST_NAME} PRIVATE
    AYDevice
    AYDeviceConfig
    AYDeviceSubSystem
    AYTest
)

add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
