add_executable(EngineRuntime
  src/main.cpp
)

target_include_directories(EngineRuntime PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(EngineRuntime PRIVATE EngineCore)
