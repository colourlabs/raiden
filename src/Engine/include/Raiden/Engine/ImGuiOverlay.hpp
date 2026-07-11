#pragma once

#include <Raiden/Engine/IImGuiBackend.hpp>
#include <Raiden/Platform/InputState.hpp>
#include <array>
#include <functional>
#include <memory>

namespace Raiden::Engine {

struct ProfilerFrameData {
  float cpuFrameTimeMs = 0.0F;
  float gpuFrameTimeMs = 0.0F;
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
  void newFrame(const ::Raiden::Platform::InputState &input, int displayW,
                int displayH, float dt, const ProfilerFrameData &profiler,
                const std::function<void()> &pluginDebugUI);
  static void endFrame();
  void renderDrawData(::Raiden::Renderer::ICommandBuffer &cmd);
  void shutdown();

  void setCameraMatrices(const float *view, const float *proj, int vpW,
                         int vpH) {
    if (view != nullptr) {
      std::copy(view, view + cameraViewMatrix_.size(), cameraViewMatrix_.begin());
    }
    if (proj != nullptr) {
      std::copy(proj, proj + cameraProjMatrix_.size(), cameraProjMatrix_.begin());
    }
    cameraViewportW_ = vpW;
    cameraViewportH_ = vpH;
  }

  [[nodiscard]] const float *cameraViewMatrix() const {
    return cameraViewMatrix_.data();
  }

  [[nodiscard]] const float *cameraProjMatrix() const {
    return cameraProjMatrix_.data();
  }

  [[nodiscard]] int cameraViewportW() const { return cameraViewportW_; }
  [[nodiscard]] int cameraViewportH() const { return cameraViewportH_; }

  [[nodiscard]] static bool wantsCaptureMouse();

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

  static constexpr float kFpsUpdateInterval = 0.5F;

  float fpsAccumTime_ = 0.0F;
  int fpsAccumFrames_ = 0;
  float avgFps_ = 0.0F;

  float cpuAccumTime_ = 0.0F;
  float gpuAccumTime_ = 0.0F;
  int perfAccumFrames_ = 0;
  float avgCpuMs_ = 0.0F;
  float avgGpuMs_ = 0.0F;

  std::array<float, 16> cameraViewMatrix_{};
  std::array<float, 16> cameraProjMatrix_{};
  int cameraViewportW_ = 0;
  int cameraViewportH_ = 0;
};

} // namespace Raiden::Engine
