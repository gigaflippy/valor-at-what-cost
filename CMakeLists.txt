cmake_minimum_required(VERSION 3.16)
project(ValorAtWhatCost LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# raylib is downloaded and built automatically the first time you configure.
include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)   # X11 build also runs fine under XWayland
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG        5.5
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(raylib)
# Treat raylib headers as system headers so their warnings don't drown ours
get_target_property(RAYLIB_INC raylib INTERFACE_INCLUDE_DIRECTORIES)
set_target_properties(raylib PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${RAYLIB_INC}")

add_executable(valor src/main.cpp)
target_link_libraries(valor PRIVATE raylib)

if(MSVC)
  target_compile_options(valor PRIVATE /W4)
else()
  target_compile_options(valor PRIVATE -Wall -Wextra)
endif()

if(APPLE)
  target_link_libraries(valor PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()
