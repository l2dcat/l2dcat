set(L2DCAT_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${L2DCAT_GENERATED_DIR}")

if(EXISTS "${L2DCAT_LINUX_UPDATE_PUBLIC_KEY}")
  file(READ "${L2DCAT_LINUX_UPDATE_PUBLIC_KEY}" L2DCAT_KEY_HEX HEX)
  string(LENGTH "${L2DCAT_KEY_HEX}" L2DCAT_KEY_HEX_LENGTH)
  math(EXPR L2DCAT_KEY_SIZE "${L2DCAT_KEY_HEX_LENGTH} / 2")
  string(REGEX REPLACE "(..)" "0x\\1," L2DCAT_KEY_VALUES "${L2DCAT_KEY_HEX}")
else()
  set(L2DCAT_KEY_SIZE 0)
  set(L2DCAT_KEY_VALUES "0")
endif()

file(WRITE "${L2DCAT_GENERATED_DIR}/linux_update_key.h"
  "#include <stddef.h>\nstatic const unsigned char l2dcat_linux_update_key[] = {${L2DCAT_KEY_VALUES}};\nstatic const size_t l2dcat_linux_update_key_size = ${L2DCAT_KEY_SIZE};\n")
