cmake_minimum_required(VERSION 3.20)

project(bitmap16dx LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BITMAP16_DEFAULT_WIDTH 448 CACHE STRING
    "Default desktop simulator framebuffer width")
set(BITMAP16_DEFAULT_HEIGHT 288 CACHE STRING
    "Default desktop simulator framebuffer height")
option(BITMAP16_CARDPUTER_ZERO_DEVICE
       "Build the SDL shell for the Cardputer Zero Linux runtime" OFF)
option(BITMAP16_CARDPUTER_ZERO
       "Use Cardputer Zero keyboard and UI conventions" OFF)
option(BITMAP16_STEAM_DECK
       "Build the fullscreen Steam Deck SDL runtime" OFF)

if(BITMAP16_STEAM_DECK AND (BITMAP16_CARDPUTER_ZERO OR BITMAP16_CARDPUTER_ZERO_DEVICE))
  message(FATAL_ERROR "Choose either the Steam Deck or Cardputer Zero target, not both")
endif()

# SDL targets share the store manifest's application version. ADV firmware
# keeps its separate release version.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/app-builder.json")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/app-builder.json" BITMAP16_STORE_MANIFEST)
string(JSON BITMAP16_STORE_VERSION GET "${BITMAP16_STORE_MANIFEST}" version)
if(NOT BITMAP16_STORE_VERSION MATCHES "^([0-9]+[.][0-9]+[.][0-9]+)-([0-9]+)$")
  message(FATAL_ERROR "Store version must be major.minor.patch-revision")
endif()
set(BITMAP16_APP_VERSION "${CMAKE_MATCH_1}")
set(BITMAP16_PACKAGE_REVISION "${CMAKE_MATCH_2}")

find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
endif()

# Music and sound effects share SDL_mixer's output device.
find_package(SDL2_mixer 2.6 CONFIG QUIET)
if(NOT TARGET SDL2_mixer::SDL2_mixer)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2_MIXER REQUIRED IMPORTED_TARGET SDL2_mixer>=2.6)
endif()

# Embed the cartridge effects so device packages and standalone builds need no
# runtime asset path, filesystem lookup, or additional audio decoder.
set(BITMAP16_CARTRIDGE_SOUND "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/success_melodic_01b.wav")
set(BITMAP16_CARTRIDGE_SLIDE_SOUND "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/transition_soft_02b.wav")
set(BITMAP16_MENU_OPEN_SOUND "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/enable_02.wav")
set(BITMAP16_MENU_CLOSE_SOUND "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/disable_02.wav")
set(BITMAP16_SKETCH_OPEN_SOUND "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/enable_04a.wav")
set(BITMAP16_SKETCH_SAVE_SOUND "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/success_melodic_03b.wav")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${BITMAP16_CARTRIDGE_SOUND}" "${BITMAP16_CARTRIDGE_SLIDE_SOUND}"
    "${BITMAP16_MENU_OPEN_SOUND}" "${BITMAP16_MENU_CLOSE_SOUND}"
    "${BITMAP16_SKETCH_OPEN_SOUND}" "${BITMAP16_SKETCH_SAVE_SOUND}")
file(READ "${BITMAP16_CARTRIDGE_SOUND}" BITMAP16_CARTRIDGE_SOUND_HEX HEX)
file(READ "${BITMAP16_CARTRIDGE_SLIDE_SOUND}" BITMAP16_CARTRIDGE_SLIDE_HEX HEX)
string(REGEX REPLACE "(..)" "0x\\1," BITMAP16_CARTRIDGE_SOUND_BYTES "${BITMAP16_CARTRIDGE_SOUND_HEX}")
string(REGEX REPLACE "(..)" "0x\\1," BITMAP16_CARTRIDGE_SLIDE_BYTES "${BITMAP16_CARTRIDGE_SLIDE_HEX}")
file(READ "${BITMAP16_MENU_OPEN_SOUND}" menu_open_hex HEX)
file(READ "${BITMAP16_MENU_CLOSE_SOUND}" menu_close_hex HEX)
file(READ "${BITMAP16_SKETCH_OPEN_SOUND}" sketch_open_hex HEX)
file(READ "${BITMAP16_SKETCH_SAVE_SOUND}" sketch_save_hex HEX)
string(REGEX REPLACE "(..)" "0x\\1," BITMAP16_MENU_OPEN_BYTES "${menu_open_hex}")
string(REGEX REPLACE "(..)" "0x\\1," BITMAP16_MENU_CLOSE_BYTES "${menu_close_hex}")
string(REGEX REPLACE "(..)" "0x\\1," BITMAP16_SKETCH_OPEN_BYTES "${sketch_open_hex}")
string(REGEX REPLACE "(..)" "0x\\1," BITMAP16_SKETCH_SAVE_BYTES "${sketch_save_hex}")
set(BITMAP16_NAVIGATION_SOUND_ARRAYS "")
set(BITMAP16_CARTRIDGE_ALTERNATE_ARRAYS "")
foreach(variant a c)
  set(sound "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/success_melodic_01${variant}.wav")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${sound}")
  file(READ "${sound}" sound_hex HEX)
  string(REGEX REPLACE "(..)" "0x\\1," sound_bytes "${sound_hex}")
  string(APPEND BITMAP16_CARTRIDGE_ALTERNATE_ARRAYS
    "inline constexpr unsigned char kCartridgeInsert_${variant}[] = {${sound_bytes}};\n")
endforeach()
foreach(index RANGE 1 5)
  set(sound "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/keys_minimal_0${index}.wav")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${sound}")
  file(READ "${sound}" sound_hex HEX)
  string(REGEX REPLACE "(..)" "0x\\1," sound_bytes "${sound_hex}")
  string(APPEND BITMAP16_NAVIGATION_SOUND_ARRAYS
    "inline constexpr unsigned char kNavigation${index}[] = {${sound_bytes}};\n")
endforeach()
configure_file(platform_desktop/cartridge_sound.h.in generated/cartridge_sound.h @ONLY)
add_library(bitmap16dx_sound_effects STATIC platform_desktop/audio_player.cpp)
if(TARGET SDL2_mixer::SDL2_mixer)
  target_link_libraries(bitmap16dx_sound_effects PUBLIC SDL2_mixer::SDL2_mixer)
else()
  target_link_libraries(bitmap16dx_sound_effects PUBLIC PkgConfig::SDL2_MIXER)
endif()
target_include_directories(bitmap16dx_sound_effects PUBLIC platform_desktop)
target_include_directories(bitmap16dx_sound_effects PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
if(TARGET SDL2::SDL2)
  target_link_libraries(bitmap16dx_sound_effects PUBLIC SDL2::SDL2)
else()
  target_link_libraries(bitmap16dx_sound_effects PUBLIC PkgConfig::SDL2)
endif()
if(NOT MSVC)
  target_compile_options(bitmap16dx_sound_effects PRIVATE -Wall -Wextra -Werror)
endif()

# Keep compressed music as files and stream one track at a time.
file(GLOB BITMAP16_MUSIC_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/music/*.mp3")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${BITMAP16_MUSIC_FILES})
file(REMOVE_RECURSE "${CMAKE_CURRENT_BINARY_DIR}/music")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/music")
if(BITMAP16_MUSIC_FILES)
  file(COPY ${BITMAP16_MUSIC_FILES} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/music")
endif()
# Credits travel with music in standalone builds, Zero packages and Deck installs.
configure_file(assets/audio/CREDITS.md music/CREDITS.md COPYONLY)

add_executable(bitmap16dx_desktop
  platform_desktop/main_desktop.cpp
  platform_desktop/palette_browser.cpp
  platform_desktop/matrix_simulator.cpp
  platform_desktop/png_export.cpp
  platform_desktop/steam_screenshot.cpp
  platform_desktop/workspace.cpp
  src/core/canvas.cpp
  src/core/canvas_view.cpp
  src/core/charging_view.cpp
  src/core/dialog_view.cpp
  src/core/help_view.cpp
  src/core/memory_view.cpp
  src/core/led_mapping.cpp
  src/core/palette_view.cpp
  src/core/preview_view.cpp
  src/core/settings_view.cpp
  src/core/editor.cpp
  src/core/palette.cpp
  src/core/sketch.cpp
  src/core/sketch_workflow.cpp
  src/core/sketch_codec.cpp
  src/core/settings.cpp
)

target_link_libraries(bitmap16dx_desktop PRIVATE bitmap16dx_sound_effects)
target_include_directories(bitmap16dx_desktop PRIVATE src)
target_compile_definitions(bitmap16dx_desktop PRIVATE
  BITMAP16_DEFAULT_WIDTH=${BITMAP16_DEFAULT_WIDTH}
  BITMAP16_DEFAULT_HEIGHT=${BITMAP16_DEFAULT_HEIGHT}
  BITMAP16_APP_VERSION="${BITMAP16_APP_VERSION}"
  BITMAP16_SDL_AUDIO=1
)
if(NOT BITMAP16_CARDPUTER_ZERO AND NOT BITMAP16_STEAM_DECK)
  target_compile_definitions(bitmap16dx_desktop PRIVATE BITMAP16_PC=1)
endif()
if(BITMAP16_CARDPUTER_ZERO)
  target_compile_definitions(
    bitmap16dx_desktop PRIVATE BITMAP16_CARDPUTER_ZERO=1 ENABLE_LED_MATRIX=0)
endif()
if(BITMAP16_CARDPUTER_ZERO OR BITMAP16_STEAM_DECK)
  target_compile_definitions(bitmap16dx_desktop PRIVATE BITMAP16_LOSPEC=1)
  find_package(CURL REQUIRED)
  find_package(Threads REQUIRED)
  set(BITMAP16_LOSPEC_KEY_FILE "$ENV{HOME}/.local/share/bitmap16dx/lospec-key.txt"
      CACHE FILEPATH "Local release key file (never committed)")
  set(BITMAP16_LOSPEC_FEED_URL "https://bitmap16dx-palettes.brad-9f9.workers.dev/palettes.json" CACHE STRING
      "Optional HTTPS URL of a shared cached Lospec feed")
  set(BITMAP16_LOSPEC_KEY_BYTES "")
  if(NOT BITMAP16_LOSPEC_FEED_URL AND EXISTS "${BITMAP16_LOSPEC_KEY_FILE}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${BITMAP16_LOSPEC_KEY_FILE}")
    file(READ "${BITMAP16_LOSPEC_KEY_FILE}" BITMAP16_RELEASE_KEY)
    string(STRIP "${BITMAP16_RELEASE_KEY}" BITMAP16_RELEASE_KEY)
    string(HEX "${BITMAP16_RELEASE_KEY}" BITMAP16_RELEASE_KEY_HEX)
    string(REGEX REPLACE "(..)" "0x\\1," BITMAP16_LOSPEC_KEY_BYTES "${BITMAP16_RELEASE_KEY_HEX}")
  endif()
  if(BITMAP16_LOSPEC_FEED_URL AND NOT BITMAP16_LOSPEC_FEED_URL MATCHES "^https://")
    message(FATAL_ERROR "BITMAP16_LOSPEC_FEED_URL must use HTTPS")
  endif()
  string(HEX "${BITMAP16_LOSPEC_FEED_URL}" BITMAP16_FEED_URL_HEX)
  string(REGEX REPLACE "(..)" "0x\\1," BITMAP16_LOSPEC_FEED_BYTES "${BITMAP16_FEED_URL_HEX}")
  configure_file(platform_desktop/lospec_config.h.in generated/lospec_config.h @ONLY)
  add_library(bitmap16dx_lospec platform_desktop/lospec.cpp src/core/palette.cpp)
  target_include_directories(bitmap16dx_lospec PUBLIC platform_desktop src)
  target_include_directories(bitmap16dx_lospec PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
  target_include_directories(bitmap16dx_lospec SYSTEM PRIVATE third_party/picojson)
  target_link_libraries(bitmap16dx_lospec PUBLIC CURL::libcurl Threads::Threads)
  target_link_libraries(bitmap16dx_desktop PRIVATE bitmap16dx_lospec)
  if(NOT MSVC)
    # GCC can report optimizer warnings from the vendored PicoJSON templates
    # even with the header marked SYSTEM. Keep those visible without blocking
    # the device's release build; other application targets retain -Werror.
    target_compile_options(bitmap16dx_lospec PRIVATE -Wall -Wextra)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(bitmap16dx_lospec PRIVATE -Werror)
    endif()
  endif()
endif()
if(BITMAP16_CARDPUTER_ZERO_DEVICE)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "The Cardputer Zero device preset requires Linux. Use the zero preset for simulation.")
  endif()
  target_sources(bitmap16dx_desktop PRIVATE
    platform_zero/native_io.cpp platform_zero/keyboard_devices.cpp
    platform_zero/keyboard_state.cpp platform_zero/system_volume.cpp
    platform_zero/imu.cpp src/core/shake_detector.cpp)
  target_compile_definitions(
    bitmap16dx_desktop PRIVATE BITMAP16_CARDPUTER_ZERO_DEVICE=1)
endif()
if(BITMAP16_STEAM_DECK)
  target_compile_definitions(
    bitmap16dx_desktop PRIVATE BITMAP16_STEAM_DECK=1)
endif()

if(BUILD_TESTING)
  add_executable(bitmap16dx_menu_audio_test platform_desktop/tests/test_menu_audio.cpp)
  target_include_directories(bitmap16dx_menu_audio_test PRIVATE platform_desktop)
  add_test(NAME menu_audio COMMAND bitmap16dx_menu_audio_test)
  add_executable(bitmap16dx_sound_effects_test platform_desktop/tests/test_audio_player.cpp)
  target_link_libraries(bitmap16dx_sound_effects_test PRIVATE bitmap16dx_sound_effects)
  target_compile_definitions(bitmap16dx_sound_effects_test PRIVATE
    BITMAP16_MUSIC_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/music")
  if(NOT MSVC)
    target_compile_options(bitmap16dx_sound_effects_test PRIVATE -Wall -Wextra -Werror)
  endif()
  add_test(NAME sound_effects COMMAND bitmap16dx_sound_effects_test)
  add_test(NAME sound_effects_unavailable COMMAND bitmap16dx_sound_effects_test --unavailable)
  add_executable(bitmap16dx_music_settings_test platform_desktop/tests/test_music_settings.cpp
    src/core/settings.cpp src/core/settings_view.cpp src/core/canvas.cpp)
  target_include_directories(bitmap16dx_music_settings_test PRIVATE src)
  if(NOT MSVC)
    target_compile_options(bitmap16dx_music_settings_test PRIVATE -Wall -Wextra -Werror)
  endif()
  add_test(NAME music_settings COMMAND bitmap16dx_music_settings_test)
  add_executable(bitmap16dx_zero_keyboard_test
    platform_zero/tests/test_keyboard.cpp platform_zero/keyboard_state.cpp)
  target_include_directories(bitmap16dx_zero_keyboard_test PRIVATE platform_zero)
  if(TARGET SDL2::SDL2)
    target_link_libraries(bitmap16dx_zero_keyboard_test PRIVATE SDL2::SDL2)
  else()
    target_link_libraries(bitmap16dx_zero_keyboard_test PRIVATE PkgConfig::SDL2)
  endif()
  if(NOT MSVC)
    target_compile_options(bitmap16dx_zero_keyboard_test PRIVATE -Wall -Wextra -Werror)
  endif()
  add_test(NAME zero_usb_keyboard COMMAND bitmap16dx_zero_keyboard_test)
  add_executable(bitmap16dx_zero_help_test
    platform_zero/tests/test_help.cpp src/core/help_view.cpp src/core/canvas.cpp)
  target_include_directories(bitmap16dx_zero_help_test PRIVATE src)
  target_compile_definitions(bitmap16dx_zero_help_test PRIVATE
    BITMAP16_CARDPUTER_ZERO=1 BITMAP16_CARDPUTER_ZERO_DEVICE=1)
  if(NOT MSVC)
    target_compile_options(bitmap16dx_zero_help_test PRIVATE -Wall -Wextra -Werror)
  endif()
  add_test(NAME zero_keyboard_help COMMAND bitmap16dx_zero_help_test)
  if(BITMAP16_STEAM_DECK)
    add_executable(bitmap16dx_steamdeck_help_test
      platform_desktop/tests/test_steamdeck_help.cpp src/core/help_view.cpp src/core/canvas.cpp)
    target_include_directories(bitmap16dx_steamdeck_help_test PRIVATE src)
    target_compile_definitions(bitmap16dx_steamdeck_help_test PRIVATE BITMAP16_STEAM_DECK=1)
    add_test(NAME steamdeck_help COMMAND bitmap16dx_steamdeck_help_test)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
      add_test(NAME steamdeck_update_install COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/platform_steamdeck/tests/test_update_and_build.py")
    endif()
  endif()
  add_executable(bitmap16dx_zero_esc_test platform_zero/tests/test_esc_hold.cpp)
  target_include_directories(bitmap16dx_zero_esc_test PRIVATE platform_zero)
  add_test(NAME zero_escape_hold COMMAND bitmap16dx_zero_esc_test)
  add_executable(bitmap16dx_zero_symbols_test platform_zero/tests/test_symbol_keys.cpp)
  target_include_directories(bitmap16dx_zero_symbols_test PRIVATE platform_zero)
  add_test(NAME zero_symbol_keys COMMAND bitmap16dx_zero_symbols_test)
  if(UNIX)
    add_executable(bitmap16dx_zero_volume_test
      platform_zero/tests/test_system_volume.cpp platform_zero/system_volume.cpp)
    target_include_directories(bitmap16dx_zero_volume_test PRIVATE platform_zero)
    target_compile_options(bitmap16dx_zero_volume_test PRIVATE -Wall -Wextra -Werror)
    add_test(NAME zero_system_volume COMMAND bitmap16dx_zero_volume_test)
    add_executable(bitmap16dx_zero_imu_test
      platform_zero/tests/test_imu.cpp platform_zero/imu.cpp src/core/shake_detector.cpp
      src/core/editor.cpp src/core/sketch.cpp src/core/palette.cpp
      src/core/settings_view.cpp src/core/settings.cpp src/core/canvas.cpp)
    target_include_directories(bitmap16dx_zero_imu_test PRIVATE src platform_zero)
    target_compile_definitions(bitmap16dx_zero_imu_test PRIVATE
      BITMAP16_CARDPUTER_ZERO=1 BITMAP16_CARDPUTER_ZERO_DEVICE=1)
    target_compile_options(bitmap16dx_zero_imu_test PRIVATE -Wall -Wextra -Werror)
    add_test(NAME zero_imu COMMAND bitmap16dx_zero_imu_test)
  endif()
  add_executable(bitmap16dx_display_warmth_test
    platform_desktop/tests/test_display_warmth.cpp
    src/core/canvas.cpp
    src/core/settings.cpp
    src/core/settings_view.cpp
  )
  target_include_directories(bitmap16dx_display_warmth_test PRIVATE src)
  target_compile_definitions(bitmap16dx_display_warmth_test PRIVATE
    BITMAP16_CARDPUTER_ZERO=1 BITMAP16_CARDPUTER_ZERO_DEVICE=1)
  if(NOT MSVC)
    target_compile_options(bitmap16dx_display_warmth_test PRIVATE -Wall -Wextra -Werror)
  endif()
  add_test(NAME display_warmth COMMAND bitmap16dx_display_warmth_test)
  add_executable(bitmap16dx_history_test
    platform_desktop/tests/test_history.cpp
    src/core/canvas.cpp
    src/core/canvas_view.cpp
    src/core/editor.cpp
    src/core/palette.cpp
    src/core/sketch.cpp
  )
  target_include_directories(bitmap16dx_history_test PRIVATE src)
  if(NOT BITMAP16_CARDPUTER_ZERO AND NOT BITMAP16_STEAM_DECK)
    target_compile_definitions(bitmap16dx_history_test PRIVATE BITMAP16_PC=1)
  endif()
  add_test(NAME editor_history COMMAND bitmap16dx_history_test)
  add_executable(bitmap16dx_workspace_test
    platform_desktop/tests/test_workspace.cpp
    platform_desktop/png_export.cpp
    platform_desktop/steam_screenshot.cpp
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
  add_executable(bitmap16dx_sketch_workflow_test
    platform_desktop/tests/test_sketch_workflow.cpp
    platform_desktop/workspace.cpp platform_desktop/png_export.cpp platform_desktop/steam_screenshot.cpp
    src/core/sketch_workflow.cpp src/core/editor.cpp src/core/palette.cpp
    src/core/settings.cpp src/core/sketch.cpp src/core/sketch_codec.cpp)
  target_include_directories(bitmap16dx_sketch_workflow_test PRIVATE src platform_desktop)
  if(NOT MSVC)
    target_compile_options(bitmap16dx_sketch_workflow_test PRIVATE -Wall -Wextra -Werror)
  endif()
  add_test(NAME sketch_replacement_workflow COMMAND bitmap16dx_sketch_workflow_test)
  if(BITMAP16_CARDPUTER_ZERO OR BITMAP16_STEAM_DECK)
    if(BITMAP16_CARDPUTER_ZERO)
      target_compile_definitions(bitmap16dx_workspace_test PRIVATE
        BITMAP16_CARDPUTER_ZERO=1 BITMAP16_CARDPUTER_ZERO_DEVICE=1)
    else()
      target_compile_definitions(bitmap16dx_workspace_test PRIVATE BITMAP16_STEAM_DECK=1)
    endif()
    add_executable(bitmap16dx_lospec_test platform_desktop/tests/test_lospec.cpp
      platform_desktop/png_export.cpp src/core/canvas.cpp src/core/palette_view.cpp
      src/core/sketch.cpp)
    target_compile_definitions(bitmap16dx_lospec_test PRIVATE BITMAP16_LOSPEC=1)
    if(NOT MSVC)
      target_compile_options(bitmap16dx_lospec_test PRIVATE -Wall -Wextra -Werror)
    endif()
    target_link_libraries(bitmap16dx_lospec_test PRIVATE bitmap16dx_lospec)
    add_test(NAME lospec_palettes COMMAND bitmap16dx_lospec_test)
    add_executable(bitmap16dx_palette_browser_test
      platform_desktop/tests/test_palette_browser.cpp
      platform_desktop/palette_browser.cpp
      platform_desktop/workspace.cpp
      platform_desktop/png_export.cpp
      platform_desktop/steam_screenshot.cpp
      src/core/editor.cpp src/core/sketch.cpp src/core/sketch_codec.cpp
      src/core/settings.cpp src/core/canvas.cpp src/core/palette_view.cpp)
    target_compile_definitions(bitmap16dx_palette_browser_test PRIVATE BITMAP16_LOSPEC=1)
    if(BITMAP16_CARDPUTER_ZERO)
      target_compile_definitions(bitmap16dx_palette_browser_test PRIVATE BITMAP16_CARDPUTER_ZERO=1)
    else()
      target_compile_definitions(bitmap16dx_palette_browser_test PRIVATE BITMAP16_STEAM_DECK=1)
    endif()
    if(NOT MSVC)
      target_compile_options(bitmap16dx_palette_browser_test PRIVATE -Wall -Wextra -Werror)
    endif()
    target_link_libraries(bitmap16dx_palette_browser_test PRIVATE bitmap16dx_lospec)
    add_test(NAME palette_browser_workflow COMMAND bitmap16dx_palette_browser_test)
    set_tests_properties(palette_browser_workflow PROPERTIES TIMEOUT 15)
  endif()
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

if(BITMAP16_CARDPUTER_ZERO AND NOT BITMAP16_CARDPUTER_ZERO_DEVICE)
  add_executable(bitmap16dx_store_assets EXCLUDE_FROM_ALL
    platform_zero/tools/store_assets.cpp platform_desktop/png_export.cpp
    src/core/canvas.cpp src/core/canvas_view.cpp src/core/palette_view.cpp
    src/core/palette.cpp src/core/sketch.cpp)
  target_include_directories(bitmap16dx_store_assets PRIVATE src platform_desktop)
  target_compile_definitions(bitmap16dx_store_assets PRIVATE BITMAP16_CARDPUTER_ZERO=1)
endif()

if(BITMAP16_CARDPUTER_ZERO_DEVICE)
  install(
    TARGETS bitmap16dx_desktop
    RUNTIME DESTINATION share/APPLaunch/apps/bitmap16dx
  )
  install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/music"
    DESTINATION share/APPLaunch/apps/bitmap16dx)
  install(
    PROGRAMS platform_zero/launcher/run_bitmap16dx.sh
    DESTINATION share/APPLaunch/bin
    RENAME bitmap16dx
  )
  install(
    FILES platform_zero/launcher/bitmap16dx.desktop
    DESTINATION share/APPLaunch/applications
  )
  install(
    FILES platform_zero/launcher/bitmap16dx.png
    DESTINATION share/APPLaunch/share/images
    RENAME bitmap16dx.png
  )
  install(FILES LICENSE DESTINATION share/doc/bitmap16dx RENAME copyright)
  install(FILES third_party/picojson/picojson.h
    DESTINATION share/doc/bitmap16dx/licenses RENAME picojson-license.h)

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
     CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(CPACK_PACKAGE_VERSION "${BITMAP16_APP_VERSION}")
    set(CPACK_DEBIAN_PACKAGE_RELEASE "${BITMAP16_PACKAGE_REVISION}")
    set(CPACK_GENERATOR DEB)
    set(CPACK_PACKAGE_NAME bitmap16dx)
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "Pixel-art sketchbook for Cardputer Zero")
    set(CPACK_PACKAGE_CONTACT "Brad McNally")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE arm64)
    set(CPACK_DEBIAN_PACKAGE_SECTION APPLaunch)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libsdl2-2.0-0, libsdl2-mixer-2.0-0, libcurl4 | libcurl4t64, ca-certificates")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    include(CPack)
  else()
    message(STATUS
      "Cardputer Zero DEB packaging requires a Linux ARM64 target")
  endif()
endif()
