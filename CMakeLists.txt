cmake_minimum_required(VERSION 3.20)

project(AYDevice)

set(SOURCES
    src/AYWindowManager.cpp
    src/AYDeviceManager.cpp
)

set(HEADERS
    AYDevice.h
    include/AYWindowTypes.h
    include/AYWindowManager.h
    include/AYDeviceManager.h
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
    target_link_libraries(${PROJECT_NAME} PUBLIC user32 gdi32)
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC AYCore)

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>: /W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>: -Wall -Wextra -Wpedantic>
)

add_subdirectory(unittest)
