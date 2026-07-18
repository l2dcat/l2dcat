option(L2DCAT_OPTIMIZE_RELEASE_SIZE
  "Enable conservative size and dead-code optimization for Release builds" ON)

# Apply before FetchContent so static dependencies use the same settings. LTO
# stays disabled because not every supported third-party archive is LTO-safe.
if(L2DCAT_OPTIMIZE_RELEASE_SIZE)
  if(MSVC OR CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
    add_compile_options($<$<CONFIG:Release>:/O1>
      $<$<CONFIG:Release>:/Gy> $<$<CONFIG:Release>:/Gw>)
    add_link_options($<$<CONFIG:Release>:/OPT:REF>
      $<$<CONFIG:Release>:/OPT:ICF>)
  elseif(CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    add_compile_options($<$<CONFIG:Release>:-Os>
      $<$<CONFIG:Release>:-ffunction-sections>
      $<$<CONFIG:Release>:-fdata-sections>)
    if(APPLE)
      add_link_options($<$<CONFIG:Release>:-Wl,-dead_strip>)
    else()
      add_link_options($<$<CONFIG:Release>:-Wl,--gc-sections>
        $<$<CONFIG:Release>:-s>)
    endif()
  endif()
endif()
