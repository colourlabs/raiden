find_package(Freetype REQUIRED)

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
  Render/TextureCache.cpp
)

target_include_directories(RaidenUI PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
  $<INSTALL_INTERFACE:include>
)

raiden_set_compile_options(RaidenUI)

target_link_libraries(RaidenUI PUBLIC EngineCore pugixml Freetype::Freetype)
