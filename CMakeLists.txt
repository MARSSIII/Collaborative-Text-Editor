cmake_minimum_required(VERSION 3.20)
project(CollaborativeTextEditor
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "Business logic for a collaborative text editor (ITMO MultiC++)"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wshadow -Wconversion
    -Wno-unused-parameter
)

set(SANITIZE "" CACHE STRING "Enable sanitizer (thread, address, undefined)")
if(SANITIZE)
    add_compile_options(-fsanitize=${SANITIZE} -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${SANITIZE})
endif()

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Dependencies for server
cmake_policy(SET CMP0167 NEW)
find_package(Boost CONFIG)
find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    find_package(nlohmann_json PATHS /opt/homebrew/lib/cmake /usr/local/lib/cmake QUIET)
endif()

add_subdirectory(src)
add_subdirectory(demo)

if(Boost_FOUND AND nlohmann_json_FOUND)
    add_subdirectory(server)
    message(STATUS "Server target enabled (Boost + nlohmann_json found)")
else()
    message(STATUS "Server target SKIPPED (install Boost and nlohmann_json via vcpkg)")
endif()

add_subdirectory(tests)
