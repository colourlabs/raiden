cmake_minimum_required(VERSION 3.21)

raiden_add_game(VisualDemos
  SOURCES
    src/VisualDemo.cpp

  ASSETS
    engine.toml
    config/
    meshes/
    textures/

  TEXTURES
    textures/grass/grass_albedo.png
    textures/grass/grass_normal.png
    textures/grass/grass_roughness.png
    textures/grass/grass_ao.png
    textures/oak-wood-bare/oak-wood-bare_albedo.png
    textures/oak-wood-bare/oak-wood-bare_normal-dx.png
    textures/oak-wood-bare/oak-wood-bare_roughness.png
    textures/oak-wood-bare/oak-wood-bare_ao.png

  INCLUDES
    ${imgui_SOURCE_DIR}
)
