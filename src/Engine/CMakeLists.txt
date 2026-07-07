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
  OPTIONS
    "GLM_BUILD_LIBRARY OFF"
)

CPMAddPackage(
  NAME tomlplusplus
  GITHUB_REPOSITORY marzer/tomlplusplus
  GIT_TAG v3.4.0
)

CPMAddPackage(
  NAME ktx
  GITHUB_REPOSITORY KhronosGroup/KTX-Software
  GIT_TAG v4.4.2
  OPTIONS
    "KTX_FEATURE_TESTS OFF"
    "KTX_FEATURE_TOOLS OFF"
    "KTX_FEATURE_GL_UPLOAD OFF" # no OpenGL backend as of yet
    "KTX_FEATURE_VK_UPLOAD OFF" # we do our own upload
    "KTX_FEATURE_STATIC_LIBRARY ON"
)

CPMAddPackage(
  NAME fastgltf
  GITHUB_REPOSITORY spnda/fastgltf
  GIT_TAG v0.9.0
)

CPMAddPackage(
  NAME stb
  GITHUB_REPOSITORY nothings/stb
  GIT_TAG master
)

# audio
CPMAddPackage(
  NAME miniaudio
  GITHUB_REPOSITORY mackron/miniaudio
  GIT_TAG 0.11.22
)

CPMAddPackage(
  NAME openal-soft
  GITHUB_REPOSITORY kcat/openal-soft
  GIT_TAG 1.24.2
  OPTIONS
    "ALSOFT_EXAMPLES OFF"
    "ALSOFT_UTILS OFF"
    "ALSOFT_INSTALL OFF"
)

CPMAddPackage(
  NAME opus
  GITHUB_REPOSITORY xiph/opus
  GIT_TAG v1.5.2
  OPTIONS
    "OPUS_BUILD_SHARED_LIBRARY OFF"
    "OPUS_BUILD_TESTING OFF"
    "OPUS_BUILD_PROGRAMS OFF"
)

add_library(EngineCore STATIC
  src/Engine/Application.cpp
  src/Core/Logger.cpp

  src/Jobs/JobSystem.cpp

  src/Core/OSFileSystem.cpp

  src/ECS/World.cpp

  src/Engine/GamePluginLoader.cpp
  src/Engine/ImGuiOverlay.cpp
  src/Engine/VulkanImGuiBackend.cpp

  src/Platform/SDL3/SDL3Platform.cpp

  src/Input/ActionMap.cpp

  src/Audio/OpenALDevice.cpp

  src/Assets/AssetManager.cpp
  src/Assets/KtxLoader.cpp
  src/Assets/GltfLoader.cpp
  src/Assets/StbImageLoader.cpp

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
  src/Renderer/Vulkan/VulkanSampler.cpp
  src/Renderer/Vulkan/VulkanTextureImpl.cpp
  src/Renderer/Vulkan/VulkanCommandBuffer.cpp
  src/Renderer/Vulkan/VulkanDescriptorPool.cpp
  src/Renderer/Vulkan/VulkanMaterial.cpp
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

set_target_properties(EngineCore PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

target_include_directories(EngineCore PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  ${implot_SOURCE_DIR}
  ${stb_SOURCE_DIR}
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

# third-party sources compiled in-tree: suppress strict warnings
set_source_files_properties(
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
  ${implot_SOURCE_DIR}/implot.cpp
  ${implot_SOURCE_DIR}/implot_items.cpp
  PROPERTIES COMPILE_OPTIONS
    "$<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>"
)

raiden_set_compile_options(EngineCore)

target_link_libraries(EngineCore PUBLIC
  volk
  SDL3::SDL3
  glm::glm
  VulkanMemoryAllocator
  tomlplusplus::tomlplusplus
  ktx
  fastgltf::fastgltf
  OpenAL::OpenAL
  miniaudio
)

# mark external dependency headers as SYSTEM (skipping ALIAS/IMPORTED targets with ::)
foreach(dep IN ITEMS volk VulkanMemoryAllocator SDL3::SDL3 glm::glm
                     tomlplusplus::tomlplusplus ktx fastgltf::fastgltf
                     OpenAL::OpenAL miniaudio)
  if(TARGET ${dep} AND NOT ${dep} MATCHES "::")
    set_target_properties(${dep} PROPERTIES SYSTEM ON)
  endif()
endforeach()

# opus support (optional)
if(TARGET opus AND TARGET opusfile)
  target_compile_definitions(EngineCore PRIVATE RAIDEN_HAS_OPUS)
  target_link_libraries(EngineCore PRIVATE opus opusfile)
  target_include_directories(EngineCore PRIVATE ${opus_SOURCE_DIR}/include ${opusfile_SOURCE_DIR}/include)
endif()

target_compile_definitions(EngineCore PRIVATE _CRT_SECURE_NO_WARNINGS)
