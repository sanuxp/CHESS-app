cmake_minimum_required(VERSION 3.20)
project(chess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(CHESS_BUILD_TESTS "Build the rules tests" ON)
option(CHESS_BUILD_APP "Build the SDL2 desktop app" ON)

add_subdirectory(core)
add_subdirectory(review)

if(CHESS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(CHESS_BUILD_APP AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/app/CMakeLists.txt")
    add_subdirectory(app)
endif()
