set(SLANG_ROOT "" CACHE PATH "Root directory of Slang install (containing bin/slangc)")

if(DEFINED ENV{HOME})
  set(SLANGC_USER_HINT "$ENV{HOME}/.local/bin")
elseif(DEFINED ENV{USERPROFILE})
  set(SLANGC_USER_HINT "$ENV{USERPROFILE}/.local/bin")
endif()

find_program(SLANGC slangc
  HINTS ENV PATH ${SLANG_ROOT}/bin ${SLANGC_USER_HINT} ${CMAKE_SOURCE_DIR}/tools/bin
)

# slangc may need the slang shared libs in LD_LIBRARY_PATH
get_filename_component(SLANGC_DIR ${SLANGC} DIRECTORY)
get_filename_component(SLANGC_PREFIX ${SLANGC_DIR} DIRECTORY)
set(SLANGC_LIB_DIRS
  ${SLANGC_PREFIX}/lib
  ${SLANGC_PREFIX}/lib64
  $ENV{LD_LIBRARY_PATH}
)
string(JOIN ":" SLANGC_ENV_LD_LIBRARY_PATH ${SLANGC_LIB_DIRS})

file(GLOB SHADER_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.slang)
file(GLOB SHADER_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include/*.slangh)

set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

set(SLANGC_WRAPPER ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=${SLANGC_ENV_LD_LIBRARY_PATH})

foreach(SHADER ${SHADER_SOURCES})
  get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
  set(VERT_SPV ${SHADER_OUTPUT_DIR}/${SHADER_NAME}.vert.spv)
  set(FRAG_SPV ${SHADER_OUTPUT_DIR}/${SHADER_NAME}.frag.spv)

  add_custom_command(
    OUTPUT ${VERT_SPV} ${FRAG_SPV}
    COMMAND ${SLANGC_WRAPPER} ${SLANGC} -I ${CMAKE_CURRENT_SOURCE_DIR} -I ${CMAKE_CURRENT_SOURCE_DIR}/include -entry vertexMain -stage vertex -target spirv ${SHADER} -o ${VERT_SPV}
    COMMAND ${SLANGC_WRAPPER} ${SLANGC} -I ${CMAKE_CURRENT_SOURCE_DIR} -I ${CMAKE_CURRENT_SOURCE_DIR}/include -entry fragmentMain -stage fragment -target spirv ${SHADER} -o ${FRAG_SPV}
    DEPENDS ${SHADER} ${SHADER_INCLUDES}
    COMMENT "Compiling ${SHADER_NAME}.slang -> SPIR-V"
  )

  list(APPEND SHADER_SPVS ${VERT_SPV} ${FRAG_SPV})
endforeach()

add_custom_target(raiden_shaders ALL DEPENDS ${SHADER_SPVS})
