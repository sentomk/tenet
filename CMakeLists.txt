cmake_minimum_required(VERSION 3.21)

project(tenet
    VERSION 0.1.0
    DESCRIPTION "A modern C++ foundation library"
    HOMEPAGE_URL "https://github.com/your-name/tenet"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(TENET_HEADER_ONLY "Use tenet in header-only mode" ON)
option(TENET_BUILD_TESTS "Build tenet tests" ${PROJECT_IS_TOP_LEVEL})
option(TENET_INSTALL "Generate install rules" ${PROJECT_IS_TOP_LEVEL})

# ---------------------------------------------------------------------------
# Library target (fmt-style: header-only or compiled, switched by option)
# ---------------------------------------------------------------------------
if(TENET_HEADER_ONLY)
    add_library(tenet INTERFACE)
    target_compile_features(tenet INTERFACE cxx_std_20)
    target_include_directories(tenet INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(tenet INTERFACE TENET_HEADER_ONLY=1)
else()
    add_library(tenet STATIC)
    target_compile_features(tenet PUBLIC cxx_std_20)
    target_sources(tenet PRIVATE src/tenet.cpp)
    target_include_directories(tenet PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    set_target_properties(tenet PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        CXX_EXTENSIONS OFF
    )
endif()

add_library(tenet::tenet ALIAS tenet)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(TENET_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Install & package config (enables find_package(tenet))
# ---------------------------------------------------------------------------
if(TENET_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS tenet
        EXPORT tenet-targets
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(EXPORT tenet-targets
        NAMESPACE tenet::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tenet
    )

    configure_package_config_file(
        cmake/tenet-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/tenet-config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tenet
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/tenet-config-version.cmake
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/tenet-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/tenet-config-version.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tenet
    )
endif()
