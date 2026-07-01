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
