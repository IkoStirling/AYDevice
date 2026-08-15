cmake_minimum_required(VERSION 3.20)

project(AYDevice
    VERSION 0.1.0
    DESCRIPTION "AYDevice — input/SDL2/window/input-mapping SubSystem"
)

set(SOURCES
    src/AYWindowManager.cpp
    src/AYDeviceManager.cpp
    src/AYKeyboardDevice.cpp
    src/AYMouseDevice.cpp
    src/AYGamepadDevice.cpp
    src/AYTouchDevice.cpp
    src/AYTextInput.cpp
    src/AYInputMapping.cpp
    src/AYInputNames.cpp
    src/AYInputProfile.cpp
)

set(HEADERS
    AYDevice.h
    include/AYDevice/WindowTypes.h
    include/AYDevice/WindowManager.h
    include/AYDevice/DeviceManager.h
    include/AYDevice/InputTypes.h
    include/AYDevice/InputDevice.h
    include/AYDevice/KeyboardDevice.h
    include/AYDevice/MouseDevice.h
    include/AYDevice/GamepadDevice.h
    include/AYDevice/TouchDevice.h
    include/AYDevice/TextInput.h
    include/AYDevice/InputMapping.h
    include/AYDevice/InputNames.h
    include/AYDevice/InputProfile.h
)

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

option(AY_DEVICE_USE_SDL2 "Use SDL2 backend for WindowManager" OFF)

if(AY_DEVICE_USE_SDL2)
    find_package(SDL2 REQUIRED)
    target_compile_definitions(${PROJECT_NAME} PRIVATE AY_DEVICE_USE_SDL2)
    target_link_libraries(${PROJECT_NAME} PUBLIC SDL2::SDL2)
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC user32 gdi32 xinput9_1_0 imm32)
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC AYCore)

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>: /W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>: -Wall -Wextra -Wpedantic>
)

# ===== AYDeviceConfig =====
# Optional bridge: InputProfile <-> AYConfig persistence. Kept in a separate
# target so the core AYDevice library stays free of the AYConfig /
# nlohmann_json dependency chain; only consumers that persist bindings link it.
add_library(AYDeviceConfig STATIC
    src/AYInputProfileConfig.cpp
    include/AYDevice/InputProfileConfig.h
)

target_include_directories(AYDeviceConfig PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(AYDeviceConfig PUBLIC AYDevice AYConfig)

target_compile_options(AYDeviceConfig PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>: /W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>: -Wall -Wextra -Wpedantic>
)

# ===== AYDeviceSubSystem =====
# GameLoop integration: wraps DeviceManager as a "Device" ISubSystem. Kept in a
# separate target so the core AYDevice library stays free of the AYGameLoop
# dependency (the editor uses DeviceManager directly without the loop).
add_library(AYDeviceSubSystem STATIC
    src/AYDeviceSubSystem.cpp
    include/AYDevice/DeviceSubSystem.h
    # INT-02 (2026-07-15): DeviceInputProvider adapter — bridges
    # AYDevice InputMapping -> LogiaRuntimeBridge::InputProvider.
    # Lives in the SubSystem target (not AYDevice core) so the
    # core library stays free of the AYScript dependency by design.
    src/AYDeviceInputProvider.cpp
    include/AYDevice/DeviceInputProvider.h
)

target_include_directories(AYDeviceSubSystem PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    # INT-03 (2026-07-20): Device->EventBus bridge. AYApplication/AppEventHost.h lives in
    # AYApplication/interface — DeviceSubSystem consumes EventBusHostScope
    # to host its post<WindowResize/Close> producer + any future listener
    # ownership (Phase 4 §a8c8be9 lesson applied here too).
    ${CMAKE_CURRENT_SOURCE_DIR}/../AYApplication/interface
)

target_link_libraries(AYDeviceSubSystem PUBLIC AYDevice AYGameLoop
    # AYScript link provides LogiaRuntimeBridge::InputProvider pure-virtual
    # interface that DeviceInputProvider derives from. AYScript's transitive
    # deps (AYReflect, sol2) don't leak because we only consume the input
    # seam — see AYScript/include/AYScript/ScriptRuntimeBridge.h:509.
    PRIVATE AYScript
    # INT-03 (2026-07-20): EventBusHostScope lives in AYApplication
    # (interface/AYApplication/AppEventHost.h). Linking AYApplication pulls the scope
    # type + (transitively) AYEventSystem, which is the bus Device posts to.
    PRIVATE AYApplication)

target_compile_options(AYDeviceSubSystem PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>: /W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>: -Wall -Wextra -Wpedantic>
)

add_subdirectory(unittest)

message(STATUS "AYDevice v0.1.0 (M1 InputMapping + INT-03 bus bridge + D5 top-level HWND) — see ${CMAKE_CURRENT_SOURCE_DIR}/design.md")
