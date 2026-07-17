cmake_minimum_required(VERSION 3.24)
project(BongoCatNative VERSION 1.1.0 LANGUAGES C CXX)

include(FetchContent)
include(CTest)

option(BONGO_FETCH_DEPS "Download pinned open-source dependencies" ON)
option(BONGO_REQUIRE_CUBISM "Fail configuration when Cubism SDK is absent" OFF)
option(BONGO_WARNINGS_AS_ERRORS "Treat native source warnings as errors" OFF)
set(BONGO_MACOS_SIGN_IDENTITY "" CACHE STRING "codesign identity for macOS releases")
set(BONGO_MACOS_NOTARY_PROFILE "" CACHE STRING "notarytool keychain profile")
set(BONGO_WINDOWS_SIGN_IDENTITY "" CACHE STRING "Windows signing certificate SHA-1")
set(BONGO_SIGNTOOL "" CACHE FILEPATH "Path to signtool.exe")
set(BONGO_LINUX_SIGN_KEY "" CACHE STRING "GPG key ID for AppImage signing")
set(BONGO_LINUX_UPDATE_PUBLIC_KEY "" CACHE FILEPATH "Ed25519 public key in DER format")
set(BONGO_LINUX_UPDATE_PRIVATE_KEY "" CACHE FILEPATH "Ed25519 private key for releases")
set(BONGO_RELEASE_BASE_URL "" CACHE STRING "Independent release download base URL")
set(BONGO_UPDATE_MANIFEST_BASE_URL "" CACHE STRING "Independent update manifest base URL")
set(BONGO_LINUXDEPLOY "" CACHE FILEPATH "Path to linuxdeploy AppImage")
set(BONGO_APPIMAGETOOL "" CACHE FILEPATH "Path to appimagetool AppImage")
set(BONGO_CUBISM_SDK "${CMAKE_CURRENT_SOURCE_DIR}/vendor/CubismSdkForNative"
  CACHE PATH "Path to Cubism SDK for Native")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(bongo_warnings INTERFACE)
if(MSVC)
  target_compile_options(bongo_warnings INTERFACE /W4 /permissive-)
  if(BONGO_WARNINGS_AS_ERRORS)
    target_compile_options(bongo_warnings INTERFACE /WX)
  endif()
else()
  target_compile_options(bongo_warnings INTERFACE -Wall -Wextra)
  if(BONGO_WARNINGS_AS_ERRORS)
    target_compile_options(bongo_warnings INTERFACE -Werror)
  endif()
endif()

if(BONGO_FETCH_DEPS)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
  set(SDL_RENDER OFF CACHE BOOL "" FORCE)
  set(SDL_GPU OFF CACHE BOOL "" FORCE)
  set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
  set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
  set(MINIAUDIO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(MINIAUDIO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(MINIAUDIO_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3 URL https://github.com/libsdl-org/SDL/archive/402fc52af4e731184ad6a704068b5ccd27d8f1b8.tar.gz
    URL_HASH SHA256=e413151af71c23d316b6076a96a999342142afa792394eaf8a542a03503fc491)
  FetchContent_Declare(yyjson URL https://github.com/ibireme/yyjson/archive/ac8f6074e1fbc43ec496aa1404b460d08b55d7a5.tar.gz
    URL_HASH SHA256=bfc16e407ddb303c98e333920d5e01386afa42a15e0a750251342e04b074e736)
  FetchContent_Declare(stb URL https://github.com/nothings/stb/archive/31c1ad37456438565541f4919958214b6e762fb4.tar.gz
    URL_HASH SHA256=e4e3bba9c572a4a4148373a914d88ea0f0d11de8cc2c66739926e7eca0223319)
  FetchContent_Declare(miniaudio URL https://github.com/mackron/miniaudio/archive/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d.tar.gz
    URL_HASH SHA256=1a3a79b80fc6f0b0cc155e28b954a598e0ddfa2db64e2afa8466be88c476fa55)
  FetchContent_Declare(nuklear URL https://github.com/Immediate-Mode-UI/Nuklear/archive/8109cfbabe04f8705408c5d8ab1a6cd48649ccda.tar.gz
    URL_HASH SHA256=23e5e1b12e897f1d568eb703aa313b7224c9b75e1118764ceba477d13b8e39f4)
  FetchContent_MakeAvailable(SDL3 yyjson stb miniaudio nuklear)
else()
  find_package(SDL3 CONFIG REQUIRED COMPONENTS SDL3-static)
  find_package(yyjson CONFIG REQUIRED)
endif()

find_package(OpenGL REQUIRED)
if(NOT WIN32)
  find_package(CURL REQUIRED)
endif()
if(UNIX AND NOT APPLE)
  find_package(OpenSSL REQUIRED)
endif()

set(BONGO_LIVE2D_SOURCES src/live2d/live2d_stub.c)
set(BONGO_CUBISM_ENABLED OFF)
set(CUBISM_CORE_HEADER "${BONGO_CUBISM_SDK}/Core/include/Live2DCubismCore.h")
if(EXISTS "${CUBISM_CORE_HEADER}")
  if(WIN32 AND NOT MSVC)
    if(BONGO_REQUIRE_CUBISM)
      message(FATAL_ERROR "Cubism Core for Windows requires Visual Studio 2022")
    endif()
    message(WARNING "Cubism SDK found but MinGW is unsupported; using diagnostic backend")
  else()
    include(cmake/Cubism.cmake)
    set(BONGO_LIVE2D_SOURCES
      src/live2d/cubism_bridge.cpp
      src/live2d/cubism_model_load.cpp
      src/live2d/cubism_model_update.cpp)
    set(BONGO_CUBISM_ENABLED ON)
  endif()
elseif(BONGO_REQUIRE_CUBISM)
  message(FATAL_ERROR "Cubism SDK missing at ${BONGO_CUBISM_SDK}")
else()
  message(WARNING "Cubism SDK missing; building the diagnostic backend")
endif()

add_library(bongo_core STATIC
  src/core/config_defaults.c
  src/core/config_io.c
  src/core/error.c
  src/core/file.c
  src/core/i18n.c
  src/core/input_state.c
  src/core/sha256.c
  src/core/model_catalog.c
  src/core/model_behavior.c
  src/core/path.c
  src/core/update_manifest.c)
target_sources(bongo_core PRIVATE src/core/shortcut.c)
target_include_directories(bongo_core PUBLIC include)
target_link_libraries(bongo_core PUBLIC yyjson PRIVATE bongo_warnings)
if(WIN32)
  target_sources(bongo_core PRIVATE src/core/windows_utf8.c)
endif()

add_library(bongo_runtime STATIC
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
  ${BONGO_LIVE2D_SOURCES})
target_include_directories(bongo_runtime PUBLIC include)
target_include_directories(bongo_runtime PRIVATE ${stb_SOURCE_DIR} ${miniaudio_SOURCE_DIR}
  ${nuklear_SOURCE_DIR})
target_link_libraries(bongo_runtime PUBLIC bongo_core SDL3::SDL3-static OpenGL::GL miniaudio)
target_link_libraries(bongo_runtime PRIVATE bongo_warnings)
if(MSVC)
  set_source_files_properties(src/media/stb_image_impl.c src/ui/nuklear_impl.c
    PROPERTIES COMPILE_OPTIONS "/W0")
else()
  set_source_files_properties(src/media/stb_image_impl.c src/ui/nuklear_impl.c
    PROPERTIES COMPILE_OPTIONS "-w")
endif()
if(BONGO_CUBISM_ENABLED)
  target_link_libraries(bongo_runtime PUBLIC Framework Live2DCubismCore glew_s)
  target_compile_definitions(bongo_runtime PUBLIC BONGO_HAS_CUBISM=1)
endif()
target_compile_definitions(bongo_runtime PRIVATE
  BONGO_DEV_ASSET_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/resources/assets"
  BONGO_DEV_LOCALE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/resources/assets/locales"
  BONGO_UPDATE_MANIFEST_BASE_URL="${BONGO_UPDATE_MANIFEST_BASE_URL}")

if(WIN32)
  enable_language(RC)
  target_sources(bongo_runtime PRIVATE src/platform/windows.c)
  target_sources(bongo_runtime PRIVATE src/platform/windows_assets.c)
  target_sources(bongo_runtime PRIVATE src/platform/windows_http.c)
  target_sources(bongo_runtime PRIVATE src/platform/windows_update.c)
  target_link_libraries(bongo_runtime PRIVATE crypt32 dwmapi shell32 user32 winhttp wintrust)
elseif(APPLE)
  enable_language(OBJC)
  target_sources(bongo_runtime PRIVATE src/platform/macos.m src/platform/macos_input.m
    src/platform/macos_keys.m src/platform/macos_menu.m src/platform/macos_update.m
    src/platform/posix_http.c)
  target_link_libraries(bongo_runtime PRIVATE "-framework Cocoa"
    "-framework ApplicationServices" "-framework Security" CURL::libcurl)
else()
  find_package(X11 REQUIRED)
  if(NOT TARGET X11::Xi OR NOT TARGET X11::Xfixes)
    message(FATAL_ERROR "Linux native build requires XInput2 and Xfixes development packages")
  endif()
  include(cmake/LinuxUpdateKey.cmake)
  target_include_directories(bongo_runtime PRIVATE "${BONGO_GENERATED_DIR}")
  target_sources(bongo_runtime PRIVATE src/platform/linux.c src/platform/linux_menu.c
    src/platform/linux_x11.c src/platform/linux_update_verify.c src/platform/posix_http.c)
  target_link_libraries(bongo_runtime PRIVATE CURL::libcurl OpenSSL::Crypto
    X11::X11 X11::Xi X11::Xfixes)
endif()

add_executable(BongoCat src/main.c)
target_link_libraries(BongoCat PRIVATE bongo_runtime)
target_link_libraries(BongoCat PRIVATE bongo_warnings)
if(APPLE)
  set_source_files_properties(resources/icons/icon.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
  target_sources(BongoCat PRIVATE resources/icons/icon.icns)
  set_target_properties(BongoCat PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME BongoCat
    MACOSX_BUNDLE_GUI_IDENTIFIER app.bongocat.native
    MACOSX_BUNDLE_ICON_FILE icon.icns
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in")
  add_custom_command(TARGET BongoCat POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "$<TARGET_BUNDLE_CONTENT_DIR:BongoCat>/MacOS/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/assets"
      "$<TARGET_BUNDLE_CONTENT_DIR:BongoCat>/MacOS/assets"
    VERBATIM)
endif()

if(WIN32)
  target_sources(bongo_runtime PRIVATE src/platform/windows_keys.c)
  set(BONGO_ASSET_STAGE "${CMAKE_CURRENT_BINARY_DIR}/embedded-assets")
  set(BONGO_ASSET_PACK "${CMAKE_CURRENT_BINARY_DIR}/bongo-assets.tar")
  file(GLOB_RECURSE BONGO_ASSET_INPUTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/assets/*")
  add_custom_command(OUTPUT "${BONGO_ASSET_PACK}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${BONGO_ASSET_STAGE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BONGO_ASSET_STAGE}/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/assets" "${BONGO_ASSET_STAGE}/assets"
    COMMAND ${CMAKE_COMMAND} -E chdir "${BONGO_ASSET_STAGE}"
      ${CMAKE_COMMAND} -E tar cf "${BONGO_ASSET_PACK}" --format=gnutar assets
    DEPENDS ${BONGO_ASSET_INPUTS}
    VERBATIM)
  add_custom_target(bongo_asset_pack DEPENDS "${BONGO_ASSET_PACK}")
  file(TO_CMAKE_PATH "${BONGO_ASSET_PACK}" BONGO_ASSET_PACK_RC)
  file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/icon.ico" BONGO_ICON_RC)
  file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/windows.manifest" BONGO_MANIFEST_RC)
  configure_file(cmake/windows_resources.rc.in
    "${CMAKE_CURRENT_BINARY_DIR}/windows_resources.rc" @ONLY)
  set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/windows_resources.rc"
    APPEND PROPERTY OBJECT_DEPENDS "${BONGO_ASSET_PACK}")
  target_sources(BongoCat PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/windows_resources.rc")
  add_dependencies(BongoCat bongo_asset_pack)
  set_target_properties(BongoCat PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

include(cmake/NativeRelease.cmake)

if(BONGO_CUBISM_ENABLED)
  add_custom_command(TARGET BongoCat POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${BONGO_CUBISM_SDK}/Framework/src/Rendering/OpenGL/Shaders/Standard
      $<TARGET_FILE_DIR:BongoCat>/FrameworkShaders)
endif()

add_custom_target(check-lines ALL
  COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckLines.cmake
  VERBATIM)

if(BUILD_TESTING)
  add_executable(bongo_core_tests
    tests/test_main.c tests/test_config.c tests/test_input.c tests/test_models.c
    tests/test_shortcut.c tests/test_update.c)
  target_link_libraries(bongo_core_tests PRIVATE bongo_core)
  target_compile_definitions(bongo_core_tests PRIVATE
    BONGO_NATIVE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME core COMMAND bongo_core_tests)
  add_executable(bongo_i18n_tests tests/test_i18n.c)
  target_link_libraries(bongo_i18n_tests PRIVATE bongo_core)
  target_compile_definitions(bongo_i18n_tests PRIVATE
    BONGO_NATIVE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME i18n COMMAND bongo_i18n_tests)
  add_executable(bongo_ui_tests tests/test_nuklear.c src/ui/nuklear_impl.c)
  target_include_directories(bongo_ui_tests PRIVATE ${nuklear_SOURCE_DIR})
  add_test(NAME ui COMMAND bongo_ui_tests)
  add_executable(bongo_app_state_tests tests/test_app_state.c src/core/app_state.c)
  target_link_libraries(bongo_app_state_tests PRIVATE bongo_warnings)
  target_include_directories(bongo_app_state_tests PRIVATE include)
  add_test(NAME app-state COMMAND bongo_app_state_tests)
  if(UNIX AND NOT APPLE)
    add_executable(bongo_linux_update_tests tests/test_linux_update.c
      src/platform/linux_update_verify.c)
    target_include_directories(bongo_linux_update_tests PRIVATE include src/platform
      "${BONGO_GENERATED_DIR}")
    target_link_libraries(bongo_linux_update_tests PRIVATE bongo_core OpenSSL::Crypto)
    add_test(NAME linux-update-signature COMMAND bongo_linux_update_tests)
  endif()
endif()

install(TARGETS BongoCat RUNTIME DESTINATION . BUNDLE DESTINATION .)
if(UNIX AND NOT APPLE)
  install(DIRECTORY resources/assets DESTINATION .)
endif()
