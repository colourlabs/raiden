cmake_minimum_required(VERSION 3.14)

# example game, builds as a shared library loaded by EngineRuntime
set(GAME_DATAPACK_DIR "${CMAKE_BINARY_DIR}/games/ExampleGame")

add_library(ExampleGame MODULE
  src/Game.cpp
)

target_include_directories(ExampleGame PRIVATE
  ${PROJECT_SOURCE_DIR}/src/EngineCore/include
  ${PROJECT_BINARY_DIR}/_deps/glm-src
)

target_compile_definitions(ExampleGame PRIVATE
  SHADERS_DIR="${PROJECT_BINARY_DIR}/shaders"
)

# output the plugin .so directly into the datapack
set_target_properties(ExampleGame PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${GAME_DATAPACK_DIR}/lib"
)

# copy static datapack assets (runs at configure time)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/engine.toml
     DESTINATION ${GAME_DATAPACK_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/config/
     DESTINATION ${GAME_DATAPACK_DIR}/config/)
