add_executable(EngineRuntime
  src/main.cpp
)

target_include_directories(EngineRuntime PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# export symbols so the game plugin (dlopen/LoadLibrary) can resolve them.
set_target_properties(EngineRuntime PROPERTIES
  ENABLE_EXPORTS ON
)

# Engine private headers (VulkanDevice.hpp etc.) live under src/Engine/src/
target_include_directories(EngineRuntime PRIVATE
  ${CMAKE_SOURCE_DIR}/src/Engine/src
)

# copy runtime DLLs (SDL3, OpenAL, etc.) next to the executable
if(WIN32)
  add_custom_command(TARGET EngineRuntime POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:EngineRuntime>
      $<TARGET_FILE_DIR:EngineRuntime>
    COMMAND_EXPAND_LISTS
  )
endif()

if(MSVC)
  target_link_libraries(EngineRuntime PRIVATE EngineCore)
  # MSVC: /WHOLEARCHIVE includes all objects from the static lib
  target_link_options(EngineRuntime PRIVATE
    /WHOLEARCHIVE:$<TARGET_FILE:EngineCore>
  )
elseif(APPLE)
  target_link_libraries(EngineRuntime PRIVATE EngineCore)
  # macOS: -force_load is the equivalent of --whole-archive for one library
  target_link_options(EngineRuntime PRIVATE
    -Wl,-force_load,$<TARGET_FILE:EngineCore>
  )
else()
  # Linux: --whole-archive / --no-whole-archive wrapping forces all
  # EngineCore objects into the executable so dlsym can find them.
  target_link_libraries(EngineRuntime PRIVATE
    -Wl,--whole-archive
    EngineCore
    -Wl,--no-whole-archive
  )
endif()
