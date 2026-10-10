cmake_minimum_required(VERSION 3.16)
project(lego_game LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# raylib: use a system install if one is present (>= 5.5), otherwise download
# the pinned 5.5 source tarball once (no git history, no examples/games built).
# ---------------------------------------------------------------------------
find_package(raylib 5.5 QUIET)
if(NOT raylib_FOUND)
  include(FetchContent)
  set(FETCHCONTENT_QUIET OFF)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  # X11 is enough on Linux (Wayland sessions run it through XWayland).
  set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
  FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(raylib)
endif()

add_executable(lego-game
  src/main.cpp
  src/game.cpp
  src/render.cpp
  src/minifig.cpp
  src/weapons.cpp
  src/level.cpp
  src/effects.cpp
  src/audio.cpp
  src/trooper_model.cpp
)

target_link_libraries(lego-game PRIVATE raylib)

if(MSVC)
  target_compile_options(lego-game PRIVATE /W3 /permissive-)
  target_compile_definitions(lego-game PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(lego-game PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
endif()

if(APPLE)
  target_link_libraries(lego-game PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

# Put the executable in build/bin on every platform / generator.
set_target_properties(lego-game PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_BINARY_DIR}/bin>"
)

# Copy runtime assets next to the executable so it runs from build/bin as well.
add_custom_command(TARGET lego-game POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:lego-game>/assets"
  COMMENT "Copying assets next to the executable"
)
