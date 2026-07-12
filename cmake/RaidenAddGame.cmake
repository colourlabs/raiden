# raiden_add_game()
#
# creates a game plugin target with all the standard boilerplate.
#
# usage:
#   raiden_add_game(ExampleGame
#     SOURCES src/Game.cpp
#     ASSETS  engine.toml config/ meshes/ default.scene.json
#     TEXTURES textures/checkerboard.png
#   )
#
function(raiden_add_game NAME)
  cmake_parse_arguments(
    GAME                     # prefix
    ""                       # options (no boolean flags)
    ""                       # one-value keywords
    "SOURCES;ASSETS;TEXTURES;INCLUDES;LINK_LIBRARIES;DEFINITIONS" # multi-value
    ${ARGN}
  )

  if(NOT GAME_SOURCES)
    message(FATAL_ERROR "raiden_add_game(${NAME}): SOURCES is required")
  endif()

  set(DATAPACK_DIR "${CMAKE_BINARY_DIR}/games/${NAME}")

  # shared library plugin
  add_library(${NAME} MODULE ${GAME_SOURCES})

  raiden_set_compile_options(${NAME})

  target_include_directories(${NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/src/Engine/include
    ${GAME_INCLUDES}
  )

  target_compile_definitions(${NAME} PRIVATE
    SHADERS_DIR="${PROJECT_BINARY_DIR}/shaders"
    ${GAME_DEFINITIONS}
  )

  set_target_properties(${NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${DATAPACK_DIR}/lib"
  )

  # windows exports
  if(WIN32)
    target_sources(${NAME} PRIVATE
      ${CMAKE_SOURCE_DIR}/cmake/plugin_exports.def
    )
  endif()

  # copy assets to datapack
  foreach(ASSET ${GAME_ASSETS})
    set(SRC "${CMAKE_CURRENT_SOURCE_DIR}/${ASSET}")
    if(NOT EXISTS "${SRC}")
      message(WARNING "raiden_add_game(${NAME}): asset not found: ${ASSET}")
      continue()
    endif()
    if(IS_DIRECTORY "${SRC}")
      add_custom_command(
        OUTPUT "${DATAPACK_DIR}/${ASSET}/.stamp"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DATAPACK_DIR}/${ASSET}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${SRC}" "${DATAPACK_DIR}/${ASSET}"
        COMMAND ${CMAKE_COMMAND} -E touch "${DATAPACK_DIR}/${ASSET}/.stamp"
        DEPENDS "${SRC}"
        COMMENT "Copying ${ASSET} to ${NAME} datapack"
        VERBATIM
      )
      list(APPEND _ASSET_STAMPS "${DATAPACK_DIR}/${ASSET}/.stamp")
    else()
      add_custom_command(
        OUTPUT "${DATAPACK_DIR}/${ASSET}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SRC}" "${DATAPACK_DIR}/${ASSET}"
        DEPENDS "${SRC}"
        COMMENT "Copying ${ASSET} to ${NAME} datapack"
        VERBATIM
      )
      list(APPEND _ASSET_STAMPS "${DATAPACK_DIR}/${ASSET}")
    endif()
  endforeach()

  if(_ASSET_STAMPS)
    add_custom_target(${NAME}Assets ALL DEPENDS ${_ASSET_STAMPS})
    add_dependencies(${NAME} ${NAME}Assets)
  endif()

  # texture conversion
  if(GAME_TEXTURES)
    find_program(TOKTX_EXECUTABLE
      NAMES toktx
      HINTS ${CMAKE_SOURCE_DIR}/tools/bin
    )

    if(TOKTX_EXECUTABLE)
      foreach(TEX ${GAME_TEXTURES})
        get_filename_component(STEM "${TEX}" NAME_WE)
        set(INPUT  "${CMAKE_CURRENT_SOURCE_DIR}/${TEX}")
        set(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/textures/${STEM}.ktx2")

        add_custom_command(
          OUTPUT "${OUTPUT}"
          COMMAND ${TOKTX_EXECUTABLE}
                  --encode uastc
                  --uastc_quality 2
                  --zcmp 5
                  --genmipmap
                  "${OUTPUT}"
                  "${INPUT}"
          DEPENDS "${INPUT}"
          COMMENT "Converting ${TEX} -> ${STEM}.ktx2"
          VERBATIM
        )
        list(APPEND _TEX_OUTPUTS "${OUTPUT}")
      endforeach()

      add_custom_target(${NAME}Textures ALL
        DEPENDS ${_TEX_OUTPUTS}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DATAPACK_DIR}/textures"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${_TEX_OUTPUTS}
                "${DATAPACK_DIR}/textures/"
        COMMENT "Copying textures to ${NAME} datapack"
        VERBATIM
      )
      add_dependencies(${NAME} ${NAME}Textures)
    else()
      message(WARNING "raiden_add_game(${NAME}): toktx not found, skipping texture conversion")
    endif()
  endif()

  # link – RaidenPluginABI is linked with --whole-archive to ensure
  # raiden_abi_version() survives --gc-sections and is in the dynamic symbol table.
  if(WIN32)
    target_link_libraries(${NAME} PRIVATE
      RaidenPluginABI
      EngineCore
      glm::glm
      ${GAME_LINK_LIBRARIES}
    )
  else()
    target_link_libraries(${NAME} PRIVATE
      -Wl,--whole-archive
      RaidenPluginABI
      -Wl,--no-whole-archive
      EngineCore
      glm::glm
      ${GAME_LINK_LIBRARIES}
    )
  endif()
endfunction()
