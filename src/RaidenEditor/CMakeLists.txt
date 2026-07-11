add_library(RaidenEditor STATIC
  src/EditorMainWindow.cpp
  src/Core/EditorContext.cpp
  src/Core/ProjectDiscovery.cpp
  src/CustomWidgets/CustomTitleBar.cpp
  src/Helpers/FramelessResizeHelper.cpp
  src/Widgets/ProjectLauncherDialog.cpp
  src/Widgets/NewProjectDialog.cpp
  include/RaidenEditor/EditorMainWindow.hpp
  include/RaidenEditor/GizmoMode.hpp
  include/RaidenEditor/Core/EditorContext.hpp
  include/RaidenEditor/Core/ProjectDiscovery.hpp
  include/RaidenEditor/CustomWidgets/CustomTitleBar.hpp
  include/RaidenEditor/Helpers/FramelessResizeHelper.hpp
  include/RaidenEditor/Widgets/ProjectLauncherDialog.hpp
  include/RaidenEditor/Widgets/NewProjectDialog.hpp
  src/Icons/FluentIcons.cpp
  include/RaidenEditor/Icons/FluentIcons.hpp
  editor_resources.qrc
)

raiden_set_compile_options(RaidenEditor)

target_include_directories(RaidenEditor PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${imgui_SOURCE_DIR}
)

target_link_libraries(RaidenEditor PUBLIC
  EngineCore
)

# optional editor executable that embeds the runtime
option(RAIDEN_BUILD_EDITOR "Build the RaidenEditor standalone executable" ON)

if(RAIDEN_BUILD_EDITOR AND NOT RAIDEN_USE_QT)
  message(STATUS "EditorRuntime requires RAIDEN_USE_QT=ON - skipping")
elseif(RAIDEN_BUILD_EDITOR AND RAIDEN_USE_QT)
  find_package(Qt5 REQUIRED COMPONENTS Widgets)

  target_link_libraries(RaidenEditor PUBLIC Qt5::Widgets)
  set_target_properties(RaidenEditor PROPERTIES AUTOMOC ON AUTORCC ON)

  add_executable(EditorRuntime
    src/EditorRuntime.cpp
  )

  raiden_set_compile_options(EditorRuntime)

  # engine private headers (VulkanDevice.hpp etc.) live under src/Engine/src/
  target_include_directories(EditorRuntime PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Engine/src
  )

  if(MSVC)
    target_link_libraries(EditorRuntime PRIVATE RaidenEditor Qt5::Widgets)
    target_link_options(EditorRuntime PRIVATE
      /WHOLEARCHIVE:$<TARGET_FILE:RaidenEditor>
    )
  elseif(APPLE)
    target_link_libraries(EditorRuntime PRIVATE RaidenEditor Qt5::Widgets)
    target_link_options(EditorRuntime PRIVATE
      -Wl,-force_load,$<TARGET_FILE:RaidenEditor>
    )
  else()
    target_link_libraries(EditorRuntime PRIVATE
      -Wl,--whole-archive
      RaidenEditor
      -Wl,--no-whole-archive
      Qt5::Widgets
    )
  endif()
endif()
