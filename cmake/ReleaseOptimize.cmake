option(L2DCAT_OPTIMIZE_RELEASE_SIZE
  "Enable conservative size and dead-code optimization for Release builds" ON)
option(L2DCAT_OPTIMIZE_RELEASE_IPO
  "Enable compiler link-time optimization for native project targets" ON)

include(CheckIPOSupported)
if(L2DCAT_OPTIMIZE_RELEASE_IPO)
  set(L2DCAT_TRY_COMPILE_CONFIGURATION "${CMAKE_TRY_COMPILE_CONFIGURATION}")
  set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
  check_ipo_supported(RESULT L2DCAT_IPO_SUPPORTED OUTPUT L2DCAT_IPO_ERROR
    LANGUAGES C CXX)
  set(CMAKE_TRY_COMPILE_CONFIGURATION "${L2DCAT_TRY_COMPILE_CONFIGURATION}")
  if(NOT L2DCAT_IPO_SUPPORTED)
    message(STATUS "Native IPO unavailable; continuing without it: ${L2DCAT_IPO_ERROR}")
  endif()
endif()

function(l2dcat_enable_release_ipo)
  foreach(target IN LISTS ARGN)
    if(L2DCAT_OPTIMIZE_RELEASE_IPO AND L2DCAT_IPO_SUPPORTED AND TARGET "${target}")
      set_property(TARGET "${target}" PROPERTY
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    endif()
  endforeach()
endfunction()

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

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
