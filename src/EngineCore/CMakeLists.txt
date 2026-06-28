CPMAddPackage(
  NAME volk
  GITHUB_REPOSITORY zeux/volk
  GIT_TAG 1.4.350
)

CPMAddPackage(
  NAME VulkanMemoryAllocator
  GITHUB_REPOSITORY GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
  GIT_TAG v3.4.0
)

CPMAddPackage(
  NAME SDL3
  GITHUB_REPOSITORY libsdl-org/SDL
  GIT_TAG release-3.4.10
)

CPMAddPackage(
  NAME glm
  GITHUB_REPOSITORY g-truc/glm
  GIT_TAG 1.0.3
)

CPMAddPackage(
  NAME tomlplusplus
  GITHUB_REPOSITORY marzer/tomlplusplus
  GIT_TAG v3.4.0
)

CPMAddPackage(
  NAME imgui
  GITHUB_REPOSITORY ocornut/imgui
  GIT_TAG v1.91.8
)

CPMAddPackage(
  NAME implot
  GITHUB_REPOSITORY epezent/implot
  GIT_TAG v0.17
)

add_library(EngineCore STATIC
  src/Application.cpp
  src/Logger.cpp
  
  src/Core/OSFileSystem.cpp

  src/ECS/World.cpp
  
  src/Engine/GamePluginLoader.cpp
  src/Engine/ImGuiOverlay.cpp
  src/Engine/VulkanImGuiBackend.cpp
  
  src/Platform/SDL3/SDL3Platform.cpp
  
  src/Input/ActionMap.cpp
  
  src/Renderer/Vulkan/VulkanAllocator.cpp
  src/Renderer/Vulkan/VulkanDevice.cpp
  src/Renderer/Vulkan/VulkanSwapchain.cpp
  src/Renderer/Vulkan/VulkanFrameContext.cpp
  src/Renderer/Vulkan/VulkanRenderPass.cpp
  src/Renderer/Vulkan/VulkanShader.cpp
  src/Renderer/Vulkan/VulkanPipeline.cpp
  src/Renderer/Vulkan/VulkanPipelineImpl.cpp
  src/Renderer/Vulkan/VulkanBuffer.cpp
  src/Renderer/Vulkan/VulkanImage.cpp
  src/Renderer/Vulkan/VulkanTextureImpl.cpp
  src/Renderer/Vulkan/VulkanCommandBuffer.cpp
  src/Renderer/Vulkan/VulkanDescriptorPool.cpp
)

target_compile_definitions(EngineCore PUBLIC
  SHADERS_DIR="${CMAKE_BINARY_DIR}/shaders"
)

target_compile_definitions(EngineCore PRIVATE
  IMGUI_IMPL_VULKAN_USE_VOLK
)

target_include_directories(EngineCore PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_include_directories(EngineCore PRIVATE
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  ${implot_SOURCE_DIR}
)

target_sources(EngineCore PRIVATE
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
  ${implot_SOURCE_DIR}/implot.cpp
  ${implot_SOURCE_DIR}/implot_items.cpp
)

target_link_libraries(EngineCore PUBLIC
  volk
  SDL3::SDL3
  glm::glm
  VulkanMemoryAllocator
  tomlplusplus::tomlplusplus
)
