include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
)

# Use the shared CRT on MSVC to match gtest defaults.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# Don't let gtest's own install rules leak into tenet's install.
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(tenet_tests
    smoke_test.cpp
    scope_test.cpp
    scope_fail_test.cpp
    scope_success_test.cpp
    defer_test.cpp
    concepts_test.cpp
)

target_link_libraries(tenet_tests PRIVATE
    tenet::tenet
    GTest::gtest_main
)

set_target_properties(tenet_tests PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
    target_compile_options(tenet_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(tenet_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

gtest_discover_tests(tenet_tests)
