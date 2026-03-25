cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0135 NEW)

project(fifoupdates VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_COVERAGE "Enable compiler coverage instrumentation" OFF)

include(CTest)

# Keep the coverage build in a single source-root directory so VS Code and
# command-line workflows resolve the same test binary and CTest metadata.
set(COVERAGE_BUILD_DIR "${CMAKE_SOURCE_DIR}/build-coverage")

if(ENABLE_COVERAGE)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang"
       OR NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "Coverage is only supported with GCC or Clang.")
    endif()

    add_library(coverage_config INTERFACE)
    target_compile_options(coverage_config INTERFACE -O0 -g --coverage)
    target_link_options(coverage_config INTERFACE --coverage)
endif()

add_library(xr17v358 STATIC
    src/ring_buffer.c
    src/xr17v358.c
)

target_include_directories(xr17v358
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(ENABLE_COVERAGE)
    target_link_libraries(xr17v358 PUBLIC coverage_config)
endif()

add_library(hw_abstraction STATIC
    src/hw_abstraction.c
)

target_include_directories(hw_abstraction
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(hw_abstraction
    PUBLIC
        xr17v358
)

if(ENABLE_COVERAGE)
    target_link_libraries(hw_abstraction PUBLIC coverage_config)
endif()

add_library(hw_test_helpers STATIC
    src/hw_test_helpers.c
)

target_include_directories(hw_test_helpers
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(hw_test_helpers
    PUBLIC
        xr17v358
)

if(ENABLE_COVERAGE)
    target_link_libraries(hw_test_helpers PUBLIC coverage_config)
endif()

if(BUILD_TESTING)
find_package(GTest)

    add_executable(xr17v358_tests
        tests/xr17v358_tests.cpp
    )

    target_include_directories(xr17v358_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(xr17v358_tests
        PRIVATE
            xr17v358
            hw_test_helpers
            GTest::gtest_main
    )

    if(ENABLE_COVERAGE)
        target_link_libraries(xr17v358_tests PRIVATE coverage_config)

        find_program(GCOVR_EXECUTABLE gcovr REQUIRED)
        add_custom_target(coverage_report
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/coverage
            COMMAND
                ${GCOVR_EXECUTABLE}
                --root ${CMAKE_CURRENT_SOURCE_DIR}
                --filter ${CMAKE_CURRENT_SOURCE_DIR}/src
                --exclude ${CMAKE_CURRENT_BINARY_DIR}/_deps
                --html-details ${CMAKE_CURRENT_BINARY_DIR}/coverage/index.html
                --xml ${CMAKE_CURRENT_BINARY_DIR}/coverage/coverage.xml
                --xml-pretty
                --print-summary
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            DEPENDS xr17v358_tests
            COMMENT "Running tests and generating coverage report"
            VERBATIM
        )
    endif()

    include(GoogleTest)
    gtest_discover_tests(xr17v358_tests)
endif()

if(ENABLE_COVERAGE)
    add_custom_target(coverage
        DEPENDS coverage_report
    )
else()
    set(COVERAGE_CONFIGURE_COMMAND
        ${CMAKE_COMMAND}
        -S ${CMAKE_SOURCE_DIR}
        -B ${COVERAGE_BUILD_DIR}
        -G ${CMAKE_GENERATOR}
        -DENABLE_COVERAGE=ON
        -DBUILD_TESTING=ON
    )

    if(CMAKE_BUILD_TYPE)
        list(APPEND COVERAGE_CONFIGURE_COMMAND -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})
    endif()

    if(EXISTS "${CMAKE_BINARY_DIR}/_deps/googletest-src/CMakeLists.txt")
        list(APPEND COVERAGE_CONFIGURE_COMMAND
            -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=${CMAKE_BINARY_DIR}/_deps/googletest-src
        )
    endif()

    if(CMAKE_GENERATOR_PLATFORM)
        list(APPEND COVERAGE_CONFIGURE_COMMAND -A ${CMAKE_GENERATOR_PLATFORM})
    endif()

    if(CMAKE_GENERATOR_TOOLSET)
        list(APPEND COVERAGE_CONFIGURE_COMMAND -T ${CMAKE_GENERATOR_TOOLSET})
    endif()

    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_BUILD_DIR}
        COMMAND ${CMAKE_COMMAND} -E rm -f ${COVERAGE_BUILD_DIR}/CMakeCache.txt
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${COVERAGE_BUILD_DIR}/CMakeFiles
        COMMAND ${COVERAGE_CONFIGURE_COMMAND}
        COMMAND ${CMAKE_COMMAND} --build ${COVERAGE_BUILD_DIR} --target coverage
        COMMENT "Configuring and running the coverage build"
        VERBATIM
    )
endif()
