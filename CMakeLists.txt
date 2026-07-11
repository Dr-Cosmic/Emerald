# Emerald programming language - cross-platform build (Windows, macOS, Linux).
cmake_minimum_required(VERSION 3.10)
project(Emerald VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(emerald
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/value.cpp
    src/environment.cpp
    src/interpreter.cpp
    src/builtins.cpp
    src/json.cpp
    src/sql.cpp
)

target_include_directories(emerald PRIVATE src)

if(MSVC)
    target_compile_options(emerald PRIVATE /W3 /EHsc /utf-8)
    target_compile_definitions(emerald PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(emerald PRIVATE -Wall -Wextra)
endif()

# Older GCC (< 9, e.g. stock RHEL 8) ships std::filesystem in a separate library.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(emerald PRIVATE stdc++fs)
endif()

install(TARGETS emerald DESTINATION bin)

# ------------------------------------------------------------------
# Tests: run every tests/*.em file and compare with tests/*.expected.
# ------------------------------------------------------------------
enable_testing()
file(GLOB EMERALD_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.em")
foreach(test_file ${EMERALD_TESTS})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_test(
        NAME ${test_name}
        COMMAND ${CMAKE_COMMAND}
            -DEMERALD_BIN=$<TARGET_FILE:emerald>
            -DTEST_FILE=${test_file}
            -DEXPECTED_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/${test_name}.expected
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_test.cmake
    )
endforeach()
