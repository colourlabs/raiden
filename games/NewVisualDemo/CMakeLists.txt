cmake_minimum_required(VERSION 3.21)

raiden_add_game(NewVisualDemo
  SOURCES
    src/Game.cpp
    src/Systems/CameraControllerSystem.cpp
    src/Systems/TransformUpdateSystem.cpp

  ASSETS
    engine.toml
    config/
    meshes/
    textures/
    scenes/

  TEXTURES
    textures/rocky_terrain/rocky_terrain_02_diff_4k.jpg
    textures/rocky_terrain/rocky_terrain_02_nor_gl_4k.jpg
    textures/rocky_terrain/rocky_terrain_02_arm_4k.jpg
    textures/red_brick/red_brick_diff_4k.jpg
    textures/red_brick/red_brick_nor_gl_4k.jpg
    textures/red_brick/red_brick_arm_4k.jpg
    textures/wine_barrel/wine_barrel_01_diff_4k.jpg
    textures/wine_barrel/wine_barrel_01_nor_gl_4k.jpg
    textures/wine_barrel/wine_barrel_01_arm_4k.jpg

  LINEAR_TEXTURES
    textures/rocky_terrain/rocky_terrain_02_nor_gl_4k.jpg
    textures/rocky_terrain/rocky_terrain_02_arm_4k.jpg
    textures/red_brick/red_brick_nor_gl_4k.jpg
    textures/red_brick/red_brick_arm_4k.jpg
    textures/wine_barrel/wine_barrel_01_nor_gl_4k.jpg
    textures/wine_barrel/wine_barrel_01_arm_4k.jpg

  INCLUDES
    ${imgui_SOURCE_DIR}
)
