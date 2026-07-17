set(BONGO_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${BONGO_GENERATED_DIR}")

if(EXISTS "${BONGO_LINUX_UPDATE_PUBLIC_KEY}")
  file(READ "${BONGO_LINUX_UPDATE_PUBLIC_KEY}" BONGO_KEY_HEX HEX)
  string(LENGTH "${BONGO_KEY_HEX}" BONGO_KEY_HEX_LENGTH)
  math(EXPR BONGO_KEY_SIZE "${BONGO_KEY_HEX_LENGTH} / 2")
  string(REGEX REPLACE "(..)" "0x\\1," BONGO_KEY_VALUES "${BONGO_KEY_HEX}")
else()
  set(BONGO_KEY_SIZE 0)
  set(BONGO_KEY_VALUES "0")
endif()

file(WRITE "${BONGO_GENERATED_DIR}/linux_update_key.h"
  "#include <stddef.h>\nstatic const unsigned char bongo_linux_update_key[] = {${BONGO_KEY_VALUES}};\nstatic const size_t bongo_linux_update_key_size = ${BONGO_KEY_SIZE};\n")
