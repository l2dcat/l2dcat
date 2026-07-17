if(NOT DEFINED ROOT)
  message(FATAL_ERROR "ROOT is required")
endif()

file(GLOB_RECURSE FILES
  "${ROOT}/src/*.c" "${ROOT}/src/*.cpp" "${ROOT}/include/*.h"
  "${ROOT}/tests/*.c" "${ROOT}/tests/*.cpp" "${ROOT}/cmake/*.cmake")
list(APPEND FILES "${ROOT}/CMakeLists.txt")

set(FAILED "")
foreach(FILE IN LISTS FILES)
  file(STRINGS "${FILE}" LINES)
  list(LENGTH LINES COUNT)
  if(COUNT GREATER 300)
    list(APPEND FAILED "${FILE}: ${COUNT}")
  endif()
endforeach()

if(FAILED)
  list(JOIN FAILED "\n" MESSAGE_TEXT)
  message(FATAL_ERROR "Files over 300 lines:\n${MESSAGE_TEXT}")
endif()

message(STATUS "300-line policy passed")
