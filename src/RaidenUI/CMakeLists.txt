CPMAddPackage(
  NAME pugixml
  GITHUB_REPOSITORY zeux/pugixml
  GIT_TAG v1.14
)

CPMAddPackage(
  NAME stb
  GITHUB_REPOSITORY nothings/stb
  GIT_TAG master
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
  ${stb_SOURCE_DIR}
)

target_link_libraries(RaidenUI PUBLIC EngineCore pugixml)
