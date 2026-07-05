add_library(RaidenEditor STATIC
  src/EditorPlugin.cpp
  src/Panels/InspectorPanel.cpp
  src/Panels/SceneTreePanel.cpp
  src/Panels/AssetBrowser.cpp
)

raiden_set_compile_options(RaidenEditor)

target_include_directories(RaidenEditor PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(RaidenEditor PUBLIC
  EngineCore
  RaidenUI
)

# optional editor executable that embeds the runtime
option(RAIDEN_BUILD_EDITOR "Build the RaidenEditor standalone executable" ON)

if(RAIDEN_BUILD_EDITOR)
  add_executable(EditorRuntime
    src/EditorRuntime.cpp
  )

  raiden_set_compile_options(EditorRuntime)

  # Engine private headers (VulkanDevice.hpp etc.) live under src/Engine/src/
  target_include_directories(EditorRuntime PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Engine/src
  )

  if(MSVC)
    target_link_libraries(EditorRuntime PRIVATE RaidenEditor)
    target_link_options(EditorRuntime PRIVATE
      /WHOLEARCHIVE:$<TARGET_FILE:RaidenEditor>
    )
  elseif(APPLE)
    target_link_libraries(EditorRuntime PRIVATE RaidenEditor)
    target_link_options(EditorRuntime PRIVATE
      -Wl,-force_load,$<TARGET_FILE:RaidenEditor>
    )
  else()
    target_link_libraries(EditorRuntime PRIVATE
      -Wl,--whole-archive
      RaidenEditor
      -Wl,--no-whole-archive
    )
  endif()
endif()
