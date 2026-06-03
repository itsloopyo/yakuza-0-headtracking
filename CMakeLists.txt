cmake_minimum_required(VERSION 3.20)
project(Yakuza0HeadTracking VERSION 0.0.0 LANGUAGES C CXX ASM_MASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Must be called here, not only inside subdirectories, or ctest run from the
# build root cannot see any registered tests.
enable_testing()

add_subdirectory(cameraunlock-core/cpp ${CMAKE_BINARY_DIR}/cameraunlock-core)

set(MOD_SOURCES
    src/dllmain.cpp
    src/mod.cpp
    src/mod_config.cpp
    src/camera_telemetry.cpp
    src/camera_hook.cpp
    camera_hook.asm
)

add_library(Yakuza0HeadTracking SHARED ${MOD_SOURCES})

set_target_properties(Yakuza0HeadTracking PROPERTIES
    OUTPUT_NAME "Yakuza0HeadTracking"
    SUFFIX ".asi"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

target_link_libraries(Yakuza0HeadTracking PRIVATE cameraunlock)

target_include_directories(Yakuza0HeadTracking PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(WIN32)
    target_link_libraries(Yakuza0HeadTracking PRIVATE ws2_32)
endif()

add_subdirectory(tests)
