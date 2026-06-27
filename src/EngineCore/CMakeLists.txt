CPMAddPackage(
  NAME volk
  GITHUB_REPOSITORY zeux/volk
  GIT_TAG 1.4.350
)

CPMAddPackage(
  NAME SDL3
  GITHUB_REPOSITORY libsdl-org/SDL
  GIT_TAG release-3.4.10
)

add_library(EngineCore STATIC
  src/Application.cpp
  src/Logger.cpp
  
  src/Platform/SDL3/SDL3Platform.cpp
  
  src/Renderer/Vulkan/VulkanDevice.cpp
  src/Renderer/Vulkan/VulkanSwapchain.cpp
  src/Renderer/Vulkan/VulkanFrameContext.cpp
  src/Renderer/Vulkan/VulkanRenderPass.cpp
  src/Renderer/Vulkan/VulkanShader.cpp
  src/Renderer/Vulkan/VulkanPipeline.cpp
)

target_compile_definitions(EngineCore PRIVATE
  SHADERS_DIR="${CMAKE_BINARY_DIR}/shaders"
)

target_include_directories(EngineCore PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(EngineCore PUBLIC
  volk
  SDL3::SDL3
)
