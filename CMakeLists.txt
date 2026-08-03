cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0077 NEW)

project(bsky_dada
    VERSION 0.1.0
    DESCRIPTION "A Dadaist oracle bot for Bluesky — word-level markov chain fed by the AT Protocol firehose"
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

option(BSKY_DADA_BUILD_TESTS "Build unit tests" ON)

# Locate the Wolfram SDK source tree.
set(WOLFRAM_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../wolfram" CACHE PATH
    "Path to the Wolfram AT Protocol SDK source tree")

if(NOT EXISTS "${WOLFRAM_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "WOLFRAM_SOURCE_DIR does not point to a Wolfram checkout at "
        "'${WOLFRAM_SOURCE_DIR}'. Set -DWOLFRAM_SOURCE_DIR=/path/to/wolfram")
endif()

# Build only the library — no examples, tests, or C++ wrappers.
set(WOLFRAM_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WOLFRAM_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(WOLFRAM_BUILD_CPP     OFF CACHE BOOL "" FORCE)
set(WOLFRAM_BUILD_SERVER  OFF CACHE BOOL "" FORCE)

add_subdirectory("${WOLFRAM_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/wolfram")

# Resolve cjson source dir for #include <cJSON.h>
get_target_property(METALBEAR_CJSON_SOURCE_DIR cjson SOURCE_DIR)
if(NOT METALBEAR_CJSON_SOURCE_DIR)
    set(METALBEAR_CJSON_SOURCE_DIR "${WOLFRAM_SOURCE_DIR}/third_party/cjson")
endif()

find_package(Threads REQUIRED)

find_path(UV_INCLUDE_DIR uv.h)
find_library(UV_LIBRARY uv)

if(NOT UV_INCLUDE_DIR OR NOT UV_LIBRARY)
    message(FATAL_ERROR "libuv not found — install with: brew install libuv")
endif()

add_executable(bsky_dada
    src/markov.c
    src/firehose_collector.c
    src/dada_bot.c
    src/main.c
)
target_link_libraries(bsky_dada PRIVATE wolfram Threads::Threads ${UV_LIBRARY})
target_include_directories(bsky_dada PRIVATE src ${METALBEAR_CJSON_SOURCE_DIR} ${UV_INCLUDE_DIR})

if(BSKY_DADA_BUILD_TESTS)
    enable_testing()

    add_library(dada_markov STATIC src/markov.c)
    target_include_directories(dada_markov PUBLIC src)

    add_executable(test_markov
        test/test_markov.c
    )
    target_link_libraries(test_markov PRIVATE dada_markov Threads::Threads)
    add_test(NAME markov COMMAND test_markov)
endif()
