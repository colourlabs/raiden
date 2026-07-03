#pragma once

#include <Raiden/Engine/IImGuiBackend.hpp>
#include <Raiden/Platform/InputState.hpp>
#include <array>
#include <functional>
#include <memory>

namespace Raiden::Engine {

struct ProfilerFrameData {
  float cpuFrameTimeMs = 0.0f;
  float gpuFrameTimeMs = 0.0f;
  uint32_t drawCalls = 0;
  uint32_t triangles = 0;
};

class ImGuiOverlay {
public:
  static constexpr int kHistorySize = 600;

  ImGuiOverlay() = default;
  ~ImGuiOverlay();

  ImGuiOverlay(const ImGuiOverlay &) = delete;
  ImGuiOverlay &operator=(const ImGuiOverlay &) = delete;
  ImGuiOverlay(ImGuiOverlay &&) = delete;
  ImGuiOverlay &operator=(ImGuiOverlay &&) = delete;

  bool init(std::unique_ptr<IImGuiBackend> backend);
  void newFrame(const ::Raiden::Platform::InputState &input, int displayW, int displayH, float dt,
                const ProfilerFrameData &profiler,
                const std::function<void()> &pluginDebugUI);
  static void endFrame();
  void renderDrawData(::Raiden::Renderer::ICommandBuffer &cmd);
  void shutdown();

private:
  void pushPerfData(const ProfilerFrameData &profiler, float dt);

  std::unique_ptr<IImGuiBackend> backend_;

  int historyCount_ = 0;
  std::array<float, kHistorySize> cpuTimes_{};
  std::array<float, kHistorySize> gpuTimes_{};
  std::array<float, kHistorySize> fps_{};
  std::array<uint32_t, kHistorySize> drawCalls_{};
  std::array<uint32_t, kHistorySize> triangles_{};

  bool showFrameTime_ = false;
  bool showFps_ = false;
  bool showDrawCalls_ = false;
};

} // namespace Raiden::Engine
