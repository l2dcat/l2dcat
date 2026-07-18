function(l2dcat_configure_embedded_cubism_assets)
  set(L2DCAT_ASSET_EXTRA_COMMANDS PARENT_SCOPE)
  if(NOT L2DCAT_CUBISM_ENABLED)
    return()
  endif()
  file(GLOB_RECURSE shader_inputs CONFIGURE_DEPENDS
    "${CUBISM_FRAMEWORK_PATH}/src/Rendering/OpenGL/Shaders/Standard/*")
  set(L2DCAT_ASSET_INPUTS ${L2DCAT_ASSET_INPUTS} ${shader_inputs} PARENT_SCOPE)
  set(L2DCAT_ASSET_EXTRA_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CUBISM_FRAMEWORK_PATH}/src/Rendering/OpenGL/Shaders/Standard"
      "${L2DCAT_ASSET_STAGE}/assets/FrameworkShaders" PARENT_SCOPE)
endfunction()

function(l2dcat_stage_cubism_assets target)
  if(NOT L2DCAT_CUBISM_ENABLED)
    return()
  endif()
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${L2DCAT_CUBISM_SDK}/Framework/src/Rendering/OpenGL/Shaders/Standard"
      "$<TARGET_FILE_DIR:${target}>/FrameworkShaders")
  if(UNIX AND NOT APPLE)
    install(DIRECTORY
      "${CUBISM_FRAMEWORK_PATH}/src/Rendering/OpenGL/Shaders/Standard/"
      DESTINATION assets/FrameworkShaders)
  endif()
endfunction()
