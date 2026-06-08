cmake_minimum_required(VERSION 3.24) # Lowered slightly for maximum IDE compatibility
project(app_code)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG        5.5 # Pinned to a stable release for predictability
)

# Disable extras to keep build times fast
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(raylib)

add_executable(app_code main.cpp)
target_link_libraries(app_code PRIVATE raylib)