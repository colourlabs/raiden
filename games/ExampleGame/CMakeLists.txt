cmake_minimum_required(VERSION 3.21)

raiden_add_game(ExampleGame
  SOURCES
    src/Game.cpp
  
  ASSETS
    engine.toml
    config/
    meshes/
    default.scene.json
    textures/skybox.ktx2
  
  TEXTURES
    textures/checkerboard.png
)
