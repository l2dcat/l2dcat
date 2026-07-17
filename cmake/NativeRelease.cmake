set(BONGO_RELEASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/native-release")
string(REGEX REPLACE "/+$" "" BONGO_RELEASE_URL "${BONGO_RELEASE_BASE_URL}")

function(bongo_release_failure message_text)
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
  set(name "BongoCat-${platform}.exe")
  set(output "${BONGO_RELEASE_DIR}/${name}")
  if(BONGO_SIGNTOOL)
    set(signtool "${BONGO_SIGNTOOL}")
  else()
    find_program(signtool NAMES signtool)
  endif()
  if(BONGO_CUBISM_ENABLED AND BONGO_RELEASE_URL AND
      BONGO_WINDOWS_SIGN_IDENTITY AND EXISTS "${signtool}")
    add_custom_target(native-release
      COMMAND ${CMAKE_COMMAND} -E rm -rf "${BONGO_RELEASE_DIR}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${BONGO_RELEASE_DIR}"
      COMMAND "${signtool}" sign /sha1 "${BONGO_WINDOWS_SIGN_IDENTITY}"
        /fd SHA256 /tr http://timestamp.digicert.com /td SHA256
        "$<TARGET_FILE:BongoCat>"
      COMMAND "${signtool}" verify /pa /all "$<TARGET_FILE:BongoCat>"
      COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:BongoCat>" "${output}"
      COMMAND ${CMAKE_COMMAND} -DINPUT=${output}
        -DOUTPUT=${BONGO_RELEASE_DIR}/latest-native-${platform}.json
        -DVERSION=${PROJECT_VERSION} -DPLATFORM=${platform}
        -DURL=${BONGO_RELEASE_URL}/v${PROJECT_VERSION}/${name}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteUpdateManifest.cmake
      DEPENDS BongoCat VERBATIM)
  else()
    bongo_release_failure(
      "Windows release requires independent release URL, Cubism SDK, and signing identity")
  endif()
elseif(APPLE)
  if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64" OR
      CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
    set(platform "darwin-aarch64")
  else()
    set(platform "darwin-x86_64")
  endif()
  set(name "BongoCat-${platform}.app.zip")
  set(output "${BONGO_RELEASE_DIR}/${name}")
  if(BONGO_CUBISM_ENABLED AND BONGO_RELEASE_URL AND BONGO_MACOS_SIGN_IDENTITY AND
      BONGO_MACOS_NOTARY_PROFILE)
    add_custom_target(native-release
      COMMAND ${CMAKE_COMMAND} -E rm -rf "${BONGO_RELEASE_DIR}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${BONGO_RELEASE_DIR}"
      COMMAND codesign --deep --force --options runtime --timestamp
        --sign "${BONGO_MACOS_SIGN_IDENTITY}" "$<TARGET_BUNDLE_DIR:BongoCat>"
      COMMAND codesign --verify --deep --strict --verbose=2
        "$<TARGET_BUNDLE_DIR:BongoCat>"
      COMMAND /usr/bin/ditto -c -k --sequesterRsrc --keepParent
        "$<TARGET_BUNDLE_DIR:BongoCat>" "${output}"
      COMMAND xcrun notarytool submit "${output}" --keychain-profile
        "${BONGO_MACOS_NOTARY_PROFILE}" --wait
      COMMAND ${CMAKE_COMMAND} -E rm -f "${output}"
      COMMAND xcrun stapler staple "$<TARGET_BUNDLE_DIR:BongoCat>"
      COMMAND xcrun stapler validate "$<TARGET_BUNDLE_DIR:BongoCat>"
      COMMAND /usr/bin/ditto -c -k --sequesterRsrc --keepParent
        "$<TARGET_BUNDLE_DIR:BongoCat>" "${output}"
      COMMAND ${CMAKE_COMMAND} -DINPUT=${output}
        -DOUTPUT=${BONGO_RELEASE_DIR}/latest-native-${platform}.json
        -DVERSION=${PROJECT_VERSION} -DPLATFORM=${platform}
        -DURL=${BONGO_RELEASE_URL}/v${PROJECT_VERSION}/${name}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteUpdateManifest.cmake
      DEPENDS BongoCat VERBATIM)
  else()
    bongo_release_failure(
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
  if(BONGO_LINUXDEPLOY)
    set(linuxdeploy "${BONGO_LINUXDEPLOY}")
  else()
    find_program(linuxdeploy NAMES linuxdeploy linuxdeploy.AppImage)
  endif()
  if(BONGO_APPIMAGETOOL)
    set(appimagetool "${BONGO_APPIMAGETOOL}")
  else()
    find_program(appimagetool NAMES appimagetool appimagetool.AppImage)
  endif()
  find_program(openssl_tool NAMES openssl)
  set(appdir "${BONGO_RELEASE_DIR}/BongoCat.AppDir")
  set(name "BongoCat-${platform}.AppImage")
  set(output "${BONGO_RELEASE_DIR}/${name}")
  set(payload "${output}.payload")
  set(signature "${output}.sig")
  if(BONGO_CUBISM_ENABLED AND BONGO_RELEASE_URL AND BONGO_LINUX_SIGN_KEY AND
      EXISTS "${BONGO_LINUX_UPDATE_PUBLIC_KEY}" AND
      EXISTS "${BONGO_LINUX_UPDATE_PRIVATE_KEY}" AND
      EXISTS "${linuxdeploy}" AND EXISTS "${appimagetool}" AND openssl_tool)
    add_custom_target(native-release
      COMMAND ${CMAKE_COMMAND} -E rm -rf "${BONGO_RELEASE_DIR}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${BONGO_RELEASE_DIR}"
      COMMAND "${linuxdeploy}" --appdir "${appdir}"
        --executable "$<TARGET_FILE:BongoCat>"
        --desktop-file "${CMAKE_CURRENT_SOURCE_DIR}/resources/linux/BongoCat.desktop"
        --icon-file "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/icon.png"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${appdir}/usr/bin/assets"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/assets" "${appdir}/usr/bin/assets"
      COMMAND ${CMAKE_COMMAND} -E env ARCH=${appimage_arch}
        "${appimagetool}" --sign-key "${BONGO_LINUX_SIGN_KEY}"
        "${appdir}" "${output}"
      COMMAND ${CMAKE_COMMAND} -DINPUT=${output} -DOUTPUT=${payload}
        -DVERSION=${PROJECT_VERSION} -DPLATFORM=${platform}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteUpdatePayload.cmake
      COMMAND "${openssl_tool}" pkeyutl -sign -rawin
        -inkey "${BONGO_LINUX_UPDATE_PRIVATE_KEY}" -in "${payload}"
        -out "${signature}"
      COMMAND "${openssl_tool}" pkeyutl -verify -pubin -keyform DER
        -inkey "${BONGO_LINUX_UPDATE_PUBLIC_KEY}" -rawin -in "${payload}"
        -sigfile "${signature}"
      COMMAND ${CMAKE_COMMAND} -DINPUT=${output}
        -DOUTPUT=${BONGO_RELEASE_DIR}/latest-native-${platform}.json
        -DVERSION=${PROJECT_VERSION} -DPLATFORM=${platform}
        -DSIGNATURE_FILE=${signature}
        -DURL=${BONGO_RELEASE_URL}/v${PROJECT_VERSION}/${name}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteUpdateManifest.cmake
      COMMAND ${CMAKE_COMMAND} -E rm -f "${payload}" "${signature}"
      DEPENDS BongoCat VERBATIM)
  else()
    bongo_release_failure(
      "Linux release requires independent release URL, Cubism SDK, tools, and signing keys")
  endif()
endif()
