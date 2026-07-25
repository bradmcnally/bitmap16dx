cmake_minimum_required(VERSION 3.20)

project(bitmap16dx LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BITMAP16_DEFAULT_WIDTH 240 CACHE STRING
    "Default desktop simulator framebuffer width")
set(BITMAP16_DEFAULT_HEIGHT 135 CACHE STRING
    "Default desktop simulator framebuffer height")
option(BITMAP16_CARDPUTER_ZERO_DEVICE
       "Build the SDL shell for the Cardputer Zero Linux runtime" OFF)

find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
endif()

add_executable(bitmap16dx_desktop
  platform_desktop/main_desktop.cpp
  platform_desktop/matrix_simulator.cpp
  platform_desktop/png_export.cpp
  platform_desktop/workspace.cpp
  src/core/canvas.cpp
  src/core/canvas_view.cpp
  src/core/charging_view.cpp
  src/core/help_view.cpp
  src/core/memory_view.cpp
  src/core/led_mapping.cpp
  src/core/palette_view.cpp
  src/core/preview_view.cpp
  src/core/settings_view.cpp
  src/core/editor.cpp
  src/core/palette.cpp
  src/core/sketch.cpp
  src/core/sketch_codec.cpp
  src/core/settings.cpp
)

target_include_directories(bitmap16dx_desktop PRIVATE src)
target_compile_definitions(bitmap16dx_desktop PRIVATE
  BITMAP16_DEFAULT_WIDTH=${BITMAP16_DEFAULT_WIDTH}
  BITMAP16_DEFAULT_HEIGHT=${BITMAP16_DEFAULT_HEIGHT}
)
if(BITMAP16_CARDPUTER_ZERO_DEVICE)
  target_compile_definitions(
    bitmap16dx_desktop PRIVATE BITMAP16_CARDPUTER_ZERO_DEVICE=1)
endif()

if(BUILD_TESTING)
  add_executable(bitmap16dx_workspace_test
    platform_desktop/test_workspace.cpp
    platform_desktop/png_export.cpp
    platform_desktop/workspace.cpp
    src/core/editor.cpp
    src/core/palette.cpp
    src/core/settings.cpp
    src/core/sketch.cpp
    src/core/sketch_codec.cpp
  )
  target_include_directories(
    bitmap16dx_workspace_test PRIVATE src platform_desktop)
  if(MSVC)
    target_compile_options(bitmap16dx_workspace_test PRIVATE /W4)
  else()
    target_compile_options(
      bitmap16dx_workspace_test PRIVATE -Wall -Wextra -Werror)
  endif()
  add_test(NAME workspace_persistence COMMAND bitmap16dx_workspace_test)
endif()

if(TARGET SDL2::SDL2)
  target_link_libraries(bitmap16dx_desktop PRIVATE SDL2::SDL2)
elseif(TARGET PkgConfig::SDL2)
  target_link_libraries(bitmap16dx_desktop PRIVATE PkgConfig::SDL2)
endif()

if(MSVC)
  target_compile_options(bitmap16dx_desktop PRIVATE /W4)
else()
  target_compile_options(bitmap16dx_desktop PRIVATE -Wall -Wextra -Werror)
endif()

if(BITMAP16_CARDPUTER_ZERO_DEVICE)
  install(
    TARGETS bitmap16dx_desktop
    RUNTIME DESTINATION share/APPLaunch/apps/bitmap16dx
  )
  install(
    PROGRAMS platform_zero/run_bitmap16dx.sh
    DESTINATION share/APPLaunch/bin
    RENAME bitmap16dx
  )
  install(
    FILES platform_zero/bitmap16dx.desktop
    DESTINATION share/APPLaunch/applications
  )
  install(
    FILES img/bitmap16dx.png
    DESTINATION share/APPLaunch/share/images
    RENAME bitmap16dx.png
  )

  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(CPACK_GENERATOR DEB)
    set(CPACK_PACKAGE_NAME bitmap16dx)
    set(CPACK_PACKAGE_VERSION 0.7.1)
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "Portable pixel sketch station for Cardputer Zero")
    set(CPACK_PACKAGE_CONTACT "BitMap16 DX contributors")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE arm64)
    set(CPACK_DEBIAN_PACKAGE_SECTION APPLaunch)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libsdl2-2.0-0, libwayland-client0")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    include(CPack)
  else()
    message(STATUS
      "Cardputer Zero DEB packaging is enabled only on an ARM64 host")
  endif()
endif()
