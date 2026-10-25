cmake_minimum_required(VERSION 3.20)
project(ShoulderSurfingBE LANGUAGES C CXX)

# Static CRT: the DLL is loaded into Minecraft by ModLoader and must not depend on a VC++ redistributable.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

include(CTest)

if(NOT WIN32)
    message(FATAL_ERROR "Shoulder Surfing Reloaded (BE) targets Windows only. Use the mingw toolchain file to cross-compile.")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SHOULDER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(MINHOOK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/minhook-1.3.3")

add_library(shoulder_minhook STATIC
    "${MINHOOK_DIR}/src/hook.c"
    "${MINHOOK_DIR}/src/buffer.c"
    "${MINHOOK_DIR}/src/trampoline.c"
    "${MINHOOK_DIR}/src/hde/hde32.c"
    "${MINHOOK_DIR}/src/hde/hde64.c"
)
target_include_directories(shoulder_minhook
    PUBLIC "${MINHOOK_DIR}/include"
    PRIVATE "${MINHOOK_DIR}/src" "${MINHOOK_DIR}/src/hde")

# Pure, platform-independent logic (unit tested).
add_library(shoulder_core STATIC
    src/client/modules/camera/CameraMath.cpp
    src/client/config/Settings.cpp
)
target_include_directories(shoulder_core PUBLIC "${SHOULDER_SOURCE_DIR}")

add_library(ShoulderSurfing SHARED
    src/client/ShoulderSurfing.cpp
    src/client/core/Logging.cpp
    src/client/modules/camera/NativeCamera.cpp
    src/client/modules/camera/ViewSettings.cpp
)
target_include_directories(ShoulderSurfing PRIVATE "${SHOULDER_SOURCE_DIR}")
target_link_libraries(ShoulderSurfing PRIVATE shoulder_core shoulder_minhook)
set_target_properties(ShoulderSurfing PROPERTIES OUTPUT_NAME "shoulder-surfing-reloaded" PREFIX "")

if(MSVC)
    target_compile_options(ShoulderSurfing PRIVATE /W4 /permissive-)
else()
    target_compile_options(ShoulderSurfing PRIVATE -Wall -Wextra)
    # Self-contained DLL: no libstdc++/libgcc DLLs next to Minecraft.
    target_link_options(ShoulderSurfing PRIVATE -static -static-libgcc -static-libstdc++)
endif()

if(BUILD_TESTING)
    foreach(test CameraMathTests SettingsTests)
        add_executable(${test} tests/${test}.cpp)
        target_link_libraries(${test} PRIVATE shoulder_core)
        add_test(NAME ${test} COMMAND ${test})
    endforeach()
endif()
