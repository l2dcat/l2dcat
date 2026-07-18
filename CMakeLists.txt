cmake_minimum_required(VERSION 3.24)
project(l2dcat VERSION 1.1.0 LANGUAGES C CXX)

include(FetchContent)
include(CTest)

option(L2DCAT_FETCH_DEPS "Download pinned open-source dependencies" ON)
option(L2DCAT_REQUIRE_CUBISM "Fail configuration when Cubism SDK is absent" OFF)
option(L2DCAT_WARNINGS_AS_ERRORS "Treat native source warnings as errors" OFF)
set(L2DCAT_MACOS_SIGN_IDENTITY "" CACHE STRING "codesign identity for macOS releases")
set(L2DCAT_MACOS_NOTARY_PROFILE "" CACHE STRING "notarytool keychain profile")
set(L2DCAT_WINDOWS_SIGN_IDENTITY "" CACHE STRING "Windows signing certificate SHA-1")
set(L2DCAT_SIGNTOOL "" CACHE FILEPATH "Path to signtool.exe")
set(L2DCAT_LINUX_SIGN_KEY "" CACHE STRING "GPG key ID for AppImage signing")
set(L2DCAT_LINUX_UPDATE_PUBLIC_KEY "" CACHE FILEPATH "Ed25519 public key in DER format")
set(L2DCAT_LINUX_UPDATE_PRIVATE_KEY "" CACHE FILEPATH "Ed25519 private key for releases")
set(L2DCAT_RELEASE_BASE_URL "" CACHE STRING "Independent release download base URL")
set(L2DCAT_UPDATE_MANIFEST_BASE_URL "" CACHE STRING "Independent update manifest base URL")
set(L2DCAT_LINUXDEPLOY "" CACHE FILEPATH "Path to linuxdeploy AppImage")
set(L2DCAT_APPIMAGETOOL "" CACHE FILEPATH "Path to appimagetool AppImage")
set(L2DCAT_CUBISM_SDK "${CMAKE_CURRENT_SOURCE_DIR}/vendor/CubismSdkForNative"
  CACHE PATH "Path to Cubism SDK for Native")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/ReleaseOptimize.cmake)

add_library(l2dcat_warnings INTERFACE)
if(MSVC)
  target_compile_options(l2dcat_warnings INTERFACE /W4 /permissive- /utf-8)
  target_compile_options(l2dcat_warnings INTERFACE
    $<$<COMPILE_LANGUAGE:C>:/experimental:c11atomics>)
  if(L2DCAT_WARNINGS_AS_ERRORS)
    target_compile_options(l2dcat_warnings INTERFACE /WX)
  endif()
else()
  target_compile_options(l2dcat_warnings INTERFACE -Wall -Wextra)
  if(L2DCAT_WARNINGS_AS_ERRORS)
    target_compile_options(l2dcat_warnings INTERFACE -Werror)
  endif()
endif()

include(cmake/Dependencies.cmake)

find_package(OpenGL REQUIRED)
if(NOT WIN32)
  find_package(CURL REQUIRED)
endif()
if(UNIX AND NOT APPLE)
  find_package(OpenSSL REQUIRED)
endif()

set(L2DCAT_LIVE2D_SOURCES src/live2d/live2d_stub.c)
set(L2DCAT_CUBISM_ENABLED OFF)
set(CUBISM_CORE_HEADER "${L2DCAT_CUBISM_SDK}/Core/include/Live2DCubismCore.h")
if(EXISTS "${CUBISM_CORE_HEADER}")
  if(WIN32 AND NOT MSVC)
    if(L2DCAT_REQUIRE_CUBISM)
      message(FATAL_ERROR "Cubism Core for Windows requires Visual Studio 2022")
    endif()
    message(WARNING "Cubism SDK found but MinGW is unsupported; using diagnostic backend")
  else()
    include(cmake/Cubism.cmake)
    set(L2DCAT_LIVE2D_SOURCES
      src/live2d/cubism_bridge.cpp
      src/live2d/cubism_model_load.cpp
      src/live2d/cubism_model_update.cpp)
    set(L2DCAT_CUBISM_ENABLED ON)
  endif()
elseif(L2DCAT_REQUIRE_CUBISM)
  message(FATAL_ERROR "Cubism SDK missing at ${L2DCAT_CUBISM_SDK}")
else()
  message(WARNING "Cubism SDK missing; building the diagnostic backend")
endif()

add_library(l2dcat_core STATIC
  src/core/config_defaults.c
  src/core/config_io.c
  src/core/error.c
  src/core/file.c
  src/core/i18n.c
  src/core/input_state.c
  src/core/mouse_tracking.c
  src/core/sha256.c
  src/core/model_catalog.c
  src/core/model_behavior.c
  src/core/path.c
  src/core/update_manifest.c)
target_sources(l2dcat_core PRIVATE src/core/shortcut.c)
target_include_directories(l2dcat_core PUBLIC include)
target_link_libraries(l2dcat_core PUBLIC yyjson PRIVATE l2dcat_warnings)
include(cmake/CubismAssets.cmake)
if(WIN32)
  target_sources(l2dcat_core PRIVATE src/core/windows_utf8.c)
endif()

add_library(l2dcat_runtime STATIC
  src/core/app_state.c
  src/media/audio.c
  src/media/image.c
  src/media/stb_image_impl.c
  src/render/gl_api.c
  src/render/overlay.c
  src/runtime/app.c
  src/runtime/assets.c
  src/runtime/frame_audit.c
  src/runtime/gamepad.c
  src/runtime/live2d_audit.c
  src/runtime/mouse.c
  src/runtime/model_files.c
  src/runtime/shortcuts.c
  src/runtime/tray.c
  src/runtime/update.c
  src/runtime/window.c
  src/runtime/window_hit.c
  src/ui/nuklear_impl.c
  src/ui/ui_backend.c
  src/ui/ui_font.c
  src/ui/ui_input.c
  src/ui/ui_sidebar.c
  src/ui/ui_theme.c
  src/ui/preferences.c
  src/ui/preferences_model.c
  src/ui/preferences_pages.c
  src/ui/preferences_widgets.c
  ${L2DCAT_LIVE2D_SOURCES})
target_include_directories(l2dcat_runtime PUBLIC include)
target_include_directories(l2dcat_runtime SYSTEM PRIVATE
  ${stb_SOURCE_DIR} ${miniaudio_SOURCE_DIR}
  ${nuklear_SOURCE_DIR})
target_link_libraries(l2dcat_runtime PRIVATE l2dcat_warnings)
target_link_libraries(l2dcat_runtime PUBLIC l2dcat_core SDL3::SDL3-static OpenGL::GL miniaudio)
if(MSVC)
  set_source_files_properties(src/media/stb_image_impl.c src/ui/nuklear_impl.c
    PROPERTIES COMPILE_OPTIONS "/W0")
else()
  set_source_files_properties(src/media/stb_image_impl.c src/ui/nuklear_impl.c
    PROPERTIES COMPILE_OPTIONS "-w")
endif()
if(L2DCAT_CUBISM_ENABLED)
  target_link_libraries(l2dcat_runtime PUBLIC Framework Live2DCubismCore glew_s)
  target_compile_definitions(l2dcat_runtime PUBLIC L2DCAT_HAS_CUBISM=1)
endif()
target_compile_definitions(l2dcat_runtime PRIVATE
  L2DCAT_DEV_ASSET_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/resources/assets"
  L2DCAT_DEV_LOCALE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/resources/assets/locales"
  L2DCAT_UPDATE_MANIFEST_BASE_URL="${L2DCAT_UPDATE_MANIFEST_BASE_URL}")

if(WIN32)
  enable_language(RC)
  target_sources(l2dcat_runtime PRIVATE src/platform/windows.c)
  target_sources(l2dcat_runtime PRIVATE src/platform/windows_assets.c)
  target_sources(l2dcat_runtime PRIVATE src/platform/windows_http.c)
  target_sources(l2dcat_runtime PRIVATE src/platform/windows_update.c)
  target_link_libraries(l2dcat_runtime PRIVATE crypt32 dwmapi shell32 user32 winhttp wintrust)
elseif(APPLE)
  enable_language(OBJC)
  target_sources(l2dcat_runtime PRIVATE src/platform/macos.m src/platform/macos_input.m
    src/platform/macos_keys.m src/platform/macos_menu.m src/platform/macos_update.m
    src/platform/posix_http.c)
  target_link_libraries(l2dcat_runtime PRIVATE "-framework Cocoa"
    "-framework ApplicationServices" "-framework Security" CURL::libcurl)
else()
  find_package(X11 REQUIRED)
  if(NOT TARGET X11::Xi OR NOT TARGET X11::Xfixes)
    message(FATAL_ERROR "Linux native build requires XInput2 and Xfixes development packages")
  endif()
  include(cmake/LinuxUpdateKey.cmake)
  target_include_directories(l2dcat_runtime PRIVATE "${L2DCAT_GENERATED_DIR}")
  target_sources(l2dcat_runtime PRIVATE src/platform/linux.c src/platform/linux_menu.c
    src/platform/linux_x11.c src/platform/linux_update_verify.c src/platform/posix_http.c)
  target_link_libraries(l2dcat_runtime PRIVATE CURL::libcurl OpenSSL::Crypto
    X11::X11 X11::Xi X11::Xfixes m)
  target_link_libraries(l2dcat_core PRIVATE m)
endif()

add_executable(l2dcat src/main.c)
target_link_libraries(l2dcat PRIVATE l2dcat_runtime)
target_link_libraries(l2dcat PRIVATE l2dcat_warnings)
if(APPLE)
  set_source_files_properties(resources/icons/icon.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
  target_sources(l2dcat PRIVATE resources/icons/icon.icns)
  set_target_properties(l2dcat PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME l2dcat
    MACOSX_BUNDLE_GUI_IDENTIFIER app.l2dcat.native
    MACOSX_BUNDLE_ICON_FILE icon.icns
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in")
  add_custom_command(TARGET l2dcat POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "$<TARGET_BUNDLE_CONTENT_DIR:l2dcat>/MacOS/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/assets"
      "$<TARGET_BUNDLE_CONTENT_DIR:l2dcat>/MacOS/assets"
    VERBATIM)
endif()

if(WIN32)
  target_sources(l2dcat_runtime PRIVATE src/platform/windows_keys.c)
  add_executable(l2dcat_asset_packer src/tools/asset_packer.cpp)
  target_link_libraries(l2dcat_asset_packer PRIVATE l2dcat_warnings)
  set(L2DCAT_ASSET_STAGE "${CMAKE_CURRENT_BINARY_DIR}/embedded-assets")
  set(L2DCAT_ASSET_PACK "${CMAKE_CURRENT_BINARY_DIR}/l2dcat-assets.pak")
  file(GLOB_RECURSE L2DCAT_ASSET_INPUTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/assets/*")
  l2dcat_configure_embedded_cubism_assets()
  add_custom_command(OUTPUT "${L2DCAT_ASSET_PACK}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${L2DCAT_ASSET_STAGE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${L2DCAT_ASSET_STAGE}/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/assets" "${L2DCAT_ASSET_STAGE}/assets"
    ${L2DCAT_ASSET_EXTRA_COMMANDS}
    COMMAND "$<TARGET_FILE:l2dcat_asset_packer>"
      "${L2DCAT_ASSET_STAGE}/assets" "${L2DCAT_ASSET_PACK}"
    DEPENDS ${L2DCAT_ASSET_INPUTS} l2dcat_asset_packer
    VERBATIM)
  add_custom_target(l2dcat_asset_pack DEPENDS "${L2DCAT_ASSET_PACK}")
  file(TO_CMAKE_PATH "${L2DCAT_ASSET_PACK}" L2DCAT_ASSET_PACK_RC)
  file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/icon.ico" L2DCAT_ICON_RC)
  set(L2DCAT_WINDOWS_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/windows.manifest")
  if(MSVC)
    set(L2DCAT_MANIFEST_RESOURCE "")
    target_link_options(l2dcat PRIVATE "/MANIFEST:EMBED"
      "/MANIFESTINPUT:${L2DCAT_WINDOWS_MANIFEST}")
  else()
    file(TO_CMAKE_PATH "${L2DCAT_WINDOWS_MANIFEST}" L2DCAT_MANIFEST_RC)
    set(L2DCAT_MANIFEST_RESOURCE "1 RT_MANIFEST \"${L2DCAT_MANIFEST_RC}\"")
  endif()
  configure_file(cmake/windows_resources.rc.in
    "${CMAKE_CURRENT_BINARY_DIR}/windows_resources.rc" @ONLY)
  set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/windows_resources.rc"
    APPEND PROPERTY OBJECT_DEPENDS "${L2DCAT_ASSET_PACK}")
  target_sources(l2dcat PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/windows_resources.rc")
  add_dependencies(l2dcat l2dcat_asset_pack)
  set_target_properties(l2dcat PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
if(MSVC)
  set_property(SOURCE src/main.c APPEND PROPERTY COMPILE_OPTIONS "/experimental:c11atomics")
  get_target_property(L2DCAT_RUNTIME_SOURCES l2dcat_runtime SOURCES)
  foreach(L2DCAT_SOURCE IN LISTS L2DCAT_RUNTIME_SOURCES)
    if(L2DCAT_SOURCE MATCHES "\\.c$")
      set_property(SOURCE "${L2DCAT_SOURCE}" APPEND PROPERTY
        COMPILE_OPTIONS "/experimental:c11atomics")
    endif()
  endforeach()
endif()

include(cmake/NativeRelease.cmake)

l2dcat_stage_cubism_assets(l2dcat)

add_custom_target(check-lines ALL
  COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckLines.cmake
  VERBATIM)

if(BUILD_TESTING)
  add_executable(l2dcat_core_tests
    tests/test_main.c tests/test_config.c tests/test_input.c tests/test_models.c
    tests/test_shortcut.c tests/test_update.c)
  target_link_libraries(l2dcat_core_tests PRIVATE l2dcat_core l2dcat_warnings)
  target_compile_definitions(l2dcat_core_tests PRIVATE
    L2DCAT_NATIVE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME core COMMAND l2dcat_core_tests)
  add_executable(l2dcat_i18n_tests tests/test_i18n.c)
  target_link_libraries(l2dcat_i18n_tests PRIVATE l2dcat_core l2dcat_warnings)
  target_compile_definitions(l2dcat_i18n_tests PRIVATE
    L2DCAT_NATIVE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME i18n COMMAND l2dcat_i18n_tests)
  add_executable(l2dcat_ui_tests tests/test_nuklear.c src/ui/nuklear_impl.c)
  target_include_directories(l2dcat_ui_tests SYSTEM PRIVATE ${nuklear_SOURCE_DIR})
  target_link_libraries(l2dcat_ui_tests PRIVATE l2dcat_warnings)
  add_test(NAME ui COMMAND l2dcat_ui_tests)
  add_executable(l2dcat_app_state_tests tests/test_app_state.c src/core/app_state.c)
  target_link_libraries(l2dcat_app_state_tests PRIVATE l2dcat_warnings)
  target_include_directories(l2dcat_app_state_tests PRIVATE include)
  add_test(NAME app-state COMMAND l2dcat_app_state_tests)
  if(UNIX AND NOT APPLE)
    target_link_libraries(l2dcat_ui_tests PRIVATE m)
    target_link_libraries(l2dcat_app_state_tests PRIVATE m)
    add_executable(l2dcat_linux_update_tests tests/test_linux_update.c
      src/platform/linux_update_verify.c)
    target_include_directories(l2dcat_linux_update_tests PRIVATE include src/platform
      "${L2DCAT_GENERATED_DIR}")
    target_link_libraries(l2dcat_linux_update_tests PRIVATE l2dcat_core
      l2dcat_warnings OpenSSL::Crypto)
    add_test(NAME linux-update-signature COMMAND l2dcat_linux_update_tests)
  endif()
endif()

install(TARGETS l2dcat RUNTIME DESTINATION . BUNDLE DESTINATION .)
if(UNIX AND NOT APPLE)
  install(DIRECTORY resources/assets DESTINATION .)
endif()
