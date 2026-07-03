cmake_minimum_required(VERSION 3.14)

set(GAME_DATAPACK_DIR "${CMAKE_BINARY_DIR}/games/ExampleGame")

add_library(ExampleGame MODULE
  src/Game.cpp
)

target_include_directories(ExampleGame PRIVATE
  ${PROJECT_SOURCE_DIR}/src/Engine/include
)

target_compile_definitions(ExampleGame PRIVATE
  SHADERS_DIR="${PROJECT_BINARY_DIR}/shaders"
)

set_target_properties(ExampleGame PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${GAME_DATAPACK_DIR}/lib"
)

if(WIN32)
  target_sources(ExampleGame PRIVATE plugin_exports.def)
endif()

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/engine.toml
     DESTINATION ${GAME_DATAPACK_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/config/
     DESTINATION ${GAME_DATAPACK_DIR}/config/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/meshes/
     DESTINATION ${GAME_DATAPACK_DIR}/meshes/)

find_program(TOKTX_EXECUTABLE
  NAMES toktx
  HINTS ${CMAKE_SOURCE_DIR}/tools/bin
  REQUIRED
)

set(TEXTURE_SOURCES
  textures/checkerboard.png
)

function(convert_texture INPUT_ABS OUTPUT_ABS)
  get_filename_component(OUTPUT_DIR ${OUTPUT_ABS} DIRECTORY)
  file(MAKE_DIRECTORY ${OUTPUT_DIR})

  add_custom_command(
    OUTPUT ${OUTPUT_ABS}
    COMMAND ${TOKTX_EXECUTABLE}
            --encode uastc
            --uastc_quality 2
            --zcmp 5
            --genmipmap
            ${OUTPUT_ABS}
            ${INPUT_ABS}
    DEPENDS ${INPUT_ABS}
    COMMENT "Converting ${INPUT_ABS} -> ${OUTPUT_ABS}"
    VERBATIM
  )
endfunction()

foreach(TEX ${TEXTURE_SOURCES})
  get_filename_component(STEM ${TEX} NAME_WE)
  set(INPUT  "${CMAKE_CURRENT_SOURCE_DIR}/${TEX}")
  set(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/textures/${STEM}.ktx2")
  convert_texture("${INPUT}" "${OUTPUT}")
  list(APPEND TEXTURE_OUTPUTS "${OUTPUT}")
endforeach()

set(SKYBOX_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/textures/skybox.ktx2")
set(SKYBOX_OUTPUT "${GAME_DATAPACK_DIR}/textures/skybox.ktx2")

add_custom_command(
  OUTPUT ${SKYBOX_OUTPUT}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SKYBOX_INPUT} ${SKYBOX_OUTPUT}
  DEPENDS ${SKYBOX_INPUT}
  COMMENT "Copying skybox texture"
  VERBATIM
)
list(APPEND TEXTURE_OUTPUTS ${SKYBOX_OUTPUT})

if(TEXTURE_OUTPUTS)
  add_custom_target(ExampleGameTextures ALL
    DEPENDS ${TEXTURE_OUTPUTS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GAME_DATAPACK_DIR}/textures
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${TEXTURE_OUTPUTS}
            ${GAME_DATAPACK_DIR}/textures/
    COMMENT "Copying textures to datapack"
    VERBATIM
  )

  add_dependencies(ExampleGame ExampleGameTextures)
endif()

target_link_libraries(ExampleGame PRIVATE EngineCore glm::glm)
