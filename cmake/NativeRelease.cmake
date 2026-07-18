set(L2DCAT_RELEASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/native-release")
string(REGEX REPLACE "/+$" "" L2DCAT_RELEASE_URL "${L2DCAT_RELEASE_BASE_URL}")

function(l2dcat_release_failure message_text)
  add_custom_target(native-release
    COMMAND ${CMAKE_COMMAND} -E echo "${message_text}"
    COMMAND ${CMAKE_COMMAND} -E false
    VERBATIM)
endfunction()

if(WIN32)
  if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(platform "windows-i686")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|aarch64)$")
    set(platform "windows-aarch64")
  else()
    set(platform "windows-x86_64")
  endif()
  set(name "l2dcat-${platform}.exe")
  set(output "${L2DCAT_RELEASE_DIR}/${name}")
  if(L2DCAT_SIGNTOOL)
    set(signtool "${L2DCAT_SIGNTOOL}")
  else()
    find_program(signtool NAMES signtool)
  endif()
  if(L2DCAT_CUBISM_ENABLED AND L2DCAT_RELEASE_URL AND
      L2DCAT_WINDOWS_SIGN_IDENTITY AND EXISTS "${signtool}")
    add_custom_target(native-release
      COMMAND ${CMAKE_COMMAND} -E rm -rf "${L2DCAT_RELEASE_DIR}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${L2DCAT_RELEASE_DIR}"
      COMMAND "${signtool}" sign /sha1 "${L2DCAT_WINDOWS_SIGN_IDENTITY}"
        /fd SHA256 /tr http://timestamp.digicert.com /td SHA256
        "$<TARGET_FILE:l2dcat>"
      COMMAND "${signtool}" verify /pa /all "$<TARGET_FILE:l2dcat>"
      COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:l2dcat>" "${output}"
      COMMAND ${CMAKE_COMMAND} -DINPUT=${output}
        -DOUTPUT=${L2DCAT_RELEASE_DIR}/latest-native-${platform}.json
        -DVERSION=${PROJECT_VERSION} -DPLATFORM=${platform}
        -DURL=${L2DCAT_RELEASE_URL}/v${PROJECT_VERSION}/${name}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteUpdateManifest.cmake
      DEPENDS l2dcat VERBATIM)
  else()
    l2dcat_release_failure(
      "Windows release requires independent release URL, Cubism SDK, and signing identity")
  endif()
elseif(APPLE)
  if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64" OR
      CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
    set(platform "darwin-aarch64")
  else()
    set(platform "darwin-x86_64")
  endif()
  set(name "l2dcat-${platform}.app.zip")
  set(output "${L2DCAT_RELEASE_DIR}/${name}")
  if(L2DCAT_CUBISM_ENABLED AND L2DCAT_RELEASE_URL AND L2DCAT_MACOS_SIGN_IDENTITY AND
      L2DCAT_MACOS_NOTARY_PROFILE)
    add_custom_target(native-release
      COMMAND ${CMAKE_COMMAND} -E rm -rf "${L2DCAT_RELEASE_DIR}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${L2DCAT_RELEASE_DIR}"
      COMMAND codesign --deep --force --options runtime --timestamp
        --sign "${L2DCAT_MACOS_SIGN_IDENTITY}" "$<TARGET_BUNDLE_DIR:l2dcat>"
      COMMAND codesign --verify --deep --strict --verbose=2
        "$<TARGET_BUNDLE_DIR:l2dcat>"
      COMMAND /usr/bin/ditto -c -k --sequesterRsrc --keepParent
        "$<TARGET_BUNDLE_DIR:l2dcat>" "${output}"
      COMMAND xcrun notarytool submit "${output}" --keychain-profile
        "${L2DCAT_MACOS_NOTARY_PROFILE}" --wait
      COMMAND ${CMAKE_COMMAND} -E rm -f "${output}"
      COMMAND xcrun stapler staple "$<TARGET_BUNDLE_DIR:l2dcat>"
      COMMAND xcrun stapler validate "$<TARGET_BUNDLE_DIR:l2dcat>"
      COMMAND /usr/bin/ditto -c -k --sequesterRsrc --keepParent
        "$<TARGET_BUNDLE_DIR:l2dcat>" "${output}"
      COMMAND ${CMAKE_COMMAND} -DINPUT=${output}
        -DOUTPUT=${L2DCAT_RELEASE_DIR}/latest-native-${platform}.json
        -DVERSION=${PROJECT_VERSION} -DPLATFORM=${platform}
        -DURL=${L2DCAT_RELEASE_URL}/v${PROJECT_VERSION}/${name}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteUpdateManifest.cmake
      DEPENDS l2dcat VERBATIM)
  else()
    l2dcat_release_failure(
      "macOS release requires independent release URL, Cubism SDK, signing, and notarization")
  endif()
else()
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
    set(platform "linux-aarch64")
    set(appimage_arch "aarch64")
  else()
    set(platform "linux-x86_64")
    set(appimage_arch "x86_64")
  endif()
  if(L2DCAT_LINUXDEPLOY)
    set(linuxdeploy "${L2DCAT_LINUXDEPLOY}")
  else()
    find_program(linuxdeploy NAMES linuxdeploy linuxdeploy.AppImage)
  endif()
  if(L2DCAT_APPIMAGETOOL)
    set(appimagetool "${L2DCAT_APPIMAGETOOL}")
  else()
    find_program(appimagetool NAMES appimagetool appimagetool.AppImage)
  endif()
  find_program(openssl_tool NAMES openssl)
  set(appdir "${L2DCAT_RELEASE_DIR}/l2dcat.AppDir")
  set(name "l2dcat-${platform}.AppImage")
  set(output "${L2DCAT_RELEASE_DIR}/${name}")
  set(payload "${output}.payload")
  set(signature "${output}.sig")
  if(L2DCAT_CUBISM_ENABLED AND L2DCAT_RELEASE_URL AND L2DCAT_LINUX_SIGN_KEY AND
      EXISTS "${L2DCAT_LINUX_UPDATE_PUBLIC_KEY}" AND
      EXISTS "${L2DCAT_LINUX_UPDATE_PRIVATE_KEY}" AND
      EXISTS "${linuxdeploy}" AND EXISTS "${appimagetool}" AND openssl_tool)
    add_custom_target(native-release
      COMMAND ${CMAKE_COMMAND} -E rm -rf "${L2DCAT_RELEASE_DIR}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${L2DCAT_RELEASE_DIR}"
      COMMAND "${linuxdeploy}" --appdir "${appdir}"
        --executable "$<TARGET_FILE:l2dcat>"
        --desktop-file "${CMAKE_CURRENT_SOURCE_DIR}/resources/linux/l2dcat.desktop"
        --icon-file "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/icon.png"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${appdir}/usr/bin/assets"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/assets" "${appdir}/usr/bin/assets"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CUBISM_FRAMEWORK_PATH}/src/Rendering/OpenGL/Shaders/Standard"
        "${appdir}/usr/bin/assets/FrameworkShaders"
      COMMAND ${CMAKE_COMMAND} -E env ARCH=${appimage_arch}
        "${appimagetool}" --sign-key "${L2DCAT_LINUX_SIGN_KEY}"
        "${appdir}" "${output}"
      COMMAND ${CMAKE_COMMAND} -DINPUT=${output} -DOUTPUT=${payload}
        -DVERSION=${PROJECT_VERSION} -DPLATFORM=${platform}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteUpdatePayload.cmake
      COMMAND "${openssl_tool}" pkeyutl -sign -rawin
        -inkey "${L2DCAT_LINUX_UPDATE_PRIVATE_KEY}" -in "${payload}"
        -out "${signature}"
      COMMAND "${openssl_tool}" pkeyutl -verify -pubin -keyform DER
        -inkey "${L2DCAT_LINUX_UPDATE_PUBLIC_KEY}" -rawin -in "${payload}"
        -sigfile "${signature}"
      COMMAND ${CMAKE_COMMAND} -DINPUT=${output}
        -DOUTPUT=${L2DCAT_RELEASE_DIR}/latest-native-${platform}.json
        -DVERSION=${PROJECT_VERSION} -DPLATFORM=${platform}
        -DSIGNATURE_FILE=${signature}
        -DURL=${L2DCAT_RELEASE_URL}/v${PROJECT_VERSION}/${name}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteUpdateManifest.cmake
      COMMAND ${CMAKE_COMMAND} -E rm -f "${payload}" "${signature}"
      DEPENDS l2dcat VERBATIM)
  else()
    l2dcat_release_failure(
      "Linux release requires independent release URL, Cubism SDK, tools, and signing keys")
  endif()
endif()
