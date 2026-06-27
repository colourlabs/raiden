find_program(SLANGC slangc HINTS ENV PATH /home/rgb/.local/bin)
if(NOT SLANGC)
  message(FATAL_ERROR "slangc not found in PATH. Install Slang from https://github.com/shader-slang/slang")
endif()

# slangc may need the slang shared libs in LD_LIBRARY_PATH
get_filename_component(SLANGC_DIR ${SLANGC} DIRECTORY)
get_filename_component(SLANGC_PREFIX ${SLANGC_DIR} DIRECTORY)
set(SLANGC_LIB_DIRS
  ${SLANGC_PREFIX}/lib
  ${SLANGC_PREFIX}/lib64
  $ENV{LD_LIBRARY_PATH}
)
string(JOIN ":" SLANGC_ENV_LD_LIBRARY_PATH ${SLANGC_LIB_DIRS})

set(SHADER_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/triangle.slang
)

set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

set(SLANGC_WRAPPER ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=${SLANGC_ENV_LD_LIBRARY_PATH})

foreach(SHADER ${SHADER_SOURCES})
  get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
  set(VERT_SPV ${SHADER_OUTPUT_DIR}/${SHADER_NAME}.vert.spv)
  set(FRAG_SPV ${SHADER_OUTPUT_DIR}/${SHADER_NAME}.frag.spv)

  add_custom_command(
    OUTPUT ${VERT_SPV} ${FRAG_SPV}
    COMMAND ${SLANGC_WRAPPER} ${SLANGC} -entry vertexMain -stage vertex -target spirv ${SHADER} -o ${VERT_SPV}
    COMMAND ${SLANGC_WRAPPER} ${SLANGC} -entry fragmentMain -stage fragment -target spirv ${SHADER} -o ${FRAG_SPV}
    DEPENDS ${SHADER}
    COMMENT "Compiling ${SHADER_NAME}.slang -> SPIR-V"
  )

  list(APPEND SHADER_SPVS ${VERT_SPV} ${FRAG_SPV})
endforeach()

add_custom_target(raiden_shaders ALL DEPENDS ${SHADER_SPVS})
