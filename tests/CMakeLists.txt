add_executable(yakuza0_headtracking_tests
    test_main.cpp
    view_math_tests.cpp
)

target_include_directories(yakuza0_headtracking_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(yakuza0_headtracking_tests PRIVATE cameraunlock)

add_test(NAME yakuza0_headtracking_tests COMMAND yakuza0_headtracking_tests)

# The canonical config conversion's differential test: the dev pre-release's
# reader (byte copies in config_differential/oracle/src) and startup code
# against the frozen reader in src/legacy_config/.
set(YAKUZA0_DIFFERENTIAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/config_differential)
add_executable(yakuza0_config_differential
    ${YAKUZA0_DIFFERENTIAL_DIR}/differential_tests.cpp
    ${YAKUZA0_DIFFERENTIAL_DIR}/oracle/oracle_reader.cpp
    ${CMAKE_SOURCE_DIR}/src/legacy_config/legacy_config.cpp
)
target_include_directories(yakuza0_config_differential PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${YAKUZA0_DIFFERENTIAL_DIR}
)
target_compile_definitions(yakuza0_config_differential PRIVATE YAKUZA0_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(yakuza0_config_differential PRIVATE cameraunlock)
add_test(NAME yakuza0_config_differential COMMAND yakuza0_config_differential)

# What the differential test compiles is pinned by hash, line endings aside: the
# oracle's byte copies must stay the dev build's (git show 24bc0ea:src/<file>),
# the core sources both readers compile must hold the code the dev build was
# built with (core 3465659), and the frozen reader is never edited.
function(yakuza0_require_hash path expected)
    file(READ "${path}" text)
    string(REPLACE "\r" "" text "${text}")
    string(SHA256 actual "${text}")
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "${path} has changed (SHA-256 of its text ${actual}, pinned "
                            "${expected}). The config differential test compares against "
                            "exactly this file; see tests/CMakeLists.txt.")
    endif()
endfunction()
set(YAKUZA0_CORE_DIR ${CMAKE_SOURCE_DIR}/cameraunlock-core/cpp)
foreach(pinned
        "${YAKUZA0_DIFFERENTIAL_DIR}/oracle/src/mod_config.cpp|9f5031ee5a3423d4e41908ad992802c75daa225dfaa5ca7c7c79f0d1ccbd6ee1"
        "${YAKUZA0_DIFFERENTIAL_DIR}/oracle/src/mod_config.h|f40430d11fb26c97e920bb57eef7122d8e61395db42732a44d99f9d9fa0c9b71"
        "${YAKUZA0_DIFFERENTIAL_DIR}/oracle/src/logging.h|a411a77ba8c799d7886499c1b83c60a73a6871b59c3a3cce10a1b2893567226d"
        "${YAKUZA0_CORE_DIR}/include/cameraunlock/config/ini_reader.h|a7ffb44210ff59672fa97e8e5feaa2cb3e81938fcc0334a384c68bc371b3857a"
        "${YAKUZA0_CORE_DIR}/src/config/ini_reader.cpp|e01515c2656aaf533bae4350dc45b702c3e3d4043935743dcc9bd5ea581fbe1c"
        "${YAKUZA0_CORE_DIR}/include/cameraunlock/logging/file_log.h|43bdd2ef8554c78e5f440333463750c13b95110fe672b0b6e273244df9e7d169"
        "${YAKUZA0_CORE_DIR}/src/logging/file_log.cpp|73c53c2baa06bbfebe8211f62678aa2b60cb95f604743d3686951ba56b87ea47"
        "${CMAKE_SOURCE_DIR}/src/legacy_config/legacy_config.h|c3cfe156d4f4039a69bed64df4812f79667da9072002ec8ed56ffd8bcd8af9a9"
        "${CMAKE_SOURCE_DIR}/src/legacy_config/legacy_config.cpp|dbbc3a90b0a909182387bc13ce0a380bb7b2f298bff7a907052aaef12ee60bf1")
    string(REPLACE "|" ";" pair "${pinned}")
    list(GET pair 0 pinned_path)
    list(GET pair 1 pinned_hash)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${pinned_path}")
    yakuza0_require_hash("${pinned_path}" "${pinned_hash}")
endforeach()
