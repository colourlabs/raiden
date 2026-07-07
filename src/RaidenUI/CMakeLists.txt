CPMAddPackage(
  NAME freetype
  GITHUB_REPOSITORY freetype/freetype
  GIT_TAG VER-2-13-3
  OPTIONS
    "FT_DISABLE_BZIP2 ON"
    "FT_DISABLE_PNG ON"
    "FT_DISABLE_HARFBUZZ ON"
    "FT_DISABLE_BROTLI ON"
)

CPMAddPackage(
  NAME pugixml
  GITHUB_REPOSITORY zeux/pugixml
  GIT_TAG v1.14
)

add_library(RaidenUI STATIC
  Core/Signal.cpp
  DOM/Element.cpp
  DOM/Reconciler.cpp
  DOM/Events.cpp
  DOM/XmlParser.cpp
  CSS/Parser.cpp
  CSS/Selector.cpp
  CSS/Properties.cpp
  Layout/Flexbox.cpp
  Layout/Solver.cpp
  Render/QuadBatcher.cpp
  Render/FontAtlas.cpp
  Render/FontFace.cpp
  Render/TextureCache.cpp
)

target_include_directories(RaidenUI PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
  $<INSTALL_INTERFACE:include>
)

raiden_set_compile_options(RaidenUI)

target_link_libraries(RaidenUI PUBLIC EngineCore pugixml freetype)
