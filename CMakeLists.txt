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
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(raylib json)
# Treat raylib headers as system headers so their warnings don't drown ours
get_target_property(RAYLIB_INC raylib INTERFACE_INCLUDE_DIRECTORIES)
set_target_properties(raylib PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${RAYLIB_INC}")

file(GLOB_RECURSE VALOR_SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_executable(valor ${VALOR_SOURCES})
target_include_directories(valor PRIVATE src)
target_link_libraries(valor PRIVATE raylib nlohmann_json::nlohmann_json)

# Copy data/ next to the executable after every build, so the game finds it
add_custom_command(TARGET valor POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:valor>/data)

if(MSVC)
  target_compile_options(valor PRIVATE /W4)
else()
  target_compile_options(valor PRIVATE -Wall -Wextra)
endif()

if(MINGW)
  # Bundle the GCC runtime into the .exe so it runs on PCs without MinGW installed
  target_link_options(valor PRIVATE -static -static-libgcc -static-libstdc++)
endif()

if(WIN32)
  # Release builds open as a normal Windows app with no console window
  set_target_properties(valor PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
  if(MSVC)
    target_link_options(valor PRIVATE $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>)
  endif()
endif()

if(APPLE)
  target_link_libraries(valor PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()
