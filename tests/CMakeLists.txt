add_executable(yakuza0_headtracking_tests
    test_main.cpp
    view_math_tests.cpp
)

target_include_directories(yakuza0_headtracking_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(yakuza0_headtracking_tests PRIVATE cameraunlock)

add_test(NAME yakuza0_headtracking_tests COMMAND yakuza0_headtracking_tests)
