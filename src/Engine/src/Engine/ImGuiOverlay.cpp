#include <Raiden/Engine/ImGuiOverlay.hpp>
#include <Raiden/Logger.hpp>

#include <SDL3/SDL_scancode.h>
#include <imgui.h>
#include <implot.h>

namespace Raiden::Engine {

using namespace ::Raiden::Platform;
using namespace ::Raiden::Renderer;

static const ::Raiden::Core::Logger s_logger("Raiden::Engine::ImGuiOverlay");

ImGuiOverlay::~ImGuiOverlay() { shutdown(); }

bool ImGuiOverlay::init(std::unique_ptr<IImGuiBackend> backend) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = nullptr;

  ImGui::StyleColorsDark();

  backend_ = std::move(backend);

  if (!backend_->init()) {
    s_logger.error("Failed to init ImGui backend");
    return false;
  }

  s_logger.info("ImGui overlay initialized");
  return true;
}

void ImGuiOverlay::newFrame(const InputState &input, int displayW,
                            int displayH, float dt,
                            const ProfilerFrameData &profiler,
                            const std::function<void()> &pluginDebugUI) {
  backend_->newFrame();

  ImGuiIO &io = ImGui::GetIO();
  io.DisplaySize = ImVec2(static_cast<float>(displayW),
                          static_cast<float>(displayH));
  io.DeltaTime = dt > 0.0f ? dt : 1.0f / 60.0f;

  io.MousePos = ImVec2(static_cast<float>(input.mouseX),
                       static_cast<float>(input.mouseY));
  io.MouseDown[0] = input.mouseButtons[0];
  io.MouseDown[1] = input.mouseButtons[2];
  io.MouseDown[2] = input.mouseButtons[1];
  io.MouseWheel = input.scrollY;

  auto key = [&](ImGuiKey imguiKey, int sdlScan) {
    io.AddKeyEvent(imguiKey, input.keysDown[sdlScan]);
  };

  key(ImGuiKey_Tab, SDL_SCANCODE_TAB);
  key(ImGuiKey_LeftArrow, SDL_SCANCODE_LEFT);
  key(ImGuiKey_RightArrow, SDL_SCANCODE_RIGHT);
  key(ImGuiKey_UpArrow, SDL_SCANCODE_UP);
  key(ImGuiKey_DownArrow, SDL_SCANCODE_DOWN);
  key(ImGuiKey_Enter, SDL_SCANCODE_RETURN);
  key(ImGuiKey_Escape, SDL_SCANCODE_ESCAPE);
  key(ImGuiKey_Space, SDL_SCANCODE_SPACE);
  key(ImGuiKey_Backspace, SDL_SCANCODE_BACKSPACE);
  key(ImGuiKey_Delete, SDL_SCANCODE_DELETE);
  key(ImGuiKey_A, SDL_SCANCODE_A);
  key(ImGuiKey_C, SDL_SCANCODE_C);
  key(ImGuiKey_V, SDL_SCANCODE_V);
  key(ImGuiKey_X, SDL_SCANCODE_X);
  key(ImGuiKey_Y, SDL_SCANCODE_Y);
  key(ImGuiKey_Z, SDL_SCANCODE_Z);

  io.KeyCtrl =
      input.keysDown[SDL_SCANCODE_LCTRL] || input.keysDown[SDL_SCANCODE_RCTRL];
  io.KeyShift = input.keysDown[SDL_SCANCODE_LSHIFT] ||
                input.keysDown[SDL_SCANCODE_RSHIFT];
  io.KeyAlt =
      input.keysDown[SDL_SCANCODE_LALT] || input.keysDown[SDL_SCANCODE_RALT];
  io.KeySuper =
      input.keysDown[SDL_SCANCODE_LGUI] || input.keysDown[SDL_SCANCODE_RGUI];

  ImGui::NewFrame();

  pushPerfData(profiler, dt);

  // profiler window
  ImGui::Begin("Profiler");

  ImGui::Text("CPU: %.2f ms  |  GPU: %.2f ms  |  FPS: %.0f",
              profiler.cpuFrameTimeMs, profiler.gpuFrameTimeMs,
              dt > 0.0f ? 1.0f / dt : 0.0f);
  ImGui::Text("Draw calls: %u  |  Triangles: %u", profiler.drawCalls,
              profiler.triangles);

  ImGui::Separator();

  ImGui::Checkbox("Frame time", &showFrameTime_);
  ImGui::Checkbox("FPS", &showFps_);
  ImGui::Checkbox("Draw calls && Triangles", &showDrawCalls_);

  ImGui::End();

  // subwindows
  if (showFrameTime_) {
    ImGui::Begin("Frame Time", &showFrameTime_);
    if (ImPlot::BeginPlot("##frameTime", ImVec2(-1, 200))) {
      ImPlot::SetupAxes(nullptr, "ms");
      ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 33);
      ImPlot::SetupAxisLimits(ImAxis_X1, 0, kHistorySize, ImGuiCond_Always);
      int count = historyCount_;
      ImPlot::PlotLine("CPU", cpuTimes_.data(), count);
      ImPlot::PlotLine("GPU", gpuTimes_.data(), count);
      ImPlot::EndPlot();
    }
    ImGui::End();
  }

  if (showFps_) {
    ImGui::Begin("FPS", &showFps_);
    if (ImPlot::BeginPlot("##fps", ImVec2(-1, 160))) {
      ImPlot::SetupAxes(nullptr, nullptr);
      ImPlot::SetupAxisLimits(ImAxis_X1, 0, kHistorySize, ImGuiCond_Always);
      ImPlot::PlotLine("FPS", fps_.data(), historyCount_);
      ImPlot::EndPlot();
    }
    ImGui::End();
  }

  if (showDrawCalls_) {
    ImGui::Begin("Draw Calls && Triangles", &showDrawCalls_);
    if (ImPlot::BeginPlot("##drawCalls", ImVec2(-1, 180))) {
      ImPlot::SetupAxes(nullptr, nullptr);
      ImPlot::SetupAxisLimits(ImAxis_X1, 0, kHistorySize, ImGuiCond_Always);
      ImPlot::PlotLine("Draws", drawCalls_.data(), historyCount_);
      ImPlot::PlotLine("Tris", triangles_.data(), historyCount_);
      ImPlot::EndPlot();
    }
    ImGui::End();
  }

  // plugin debug UI
  if (pluginDebugUI) {
    pluginDebugUI();
  }
}

void ImGuiOverlay::endFrame() {
  ImGui::Render();
}

void ImGuiOverlay::renderDrawData(ICommandBuffer &cmd) {
  backend_->renderDrawData(cmd);
}

void ImGuiOverlay::pushPerfData(const ProfilerFrameData &profiler, float dt) {
  int idx = historyCount_ % kHistorySize;
  cpuTimes_[idx] = profiler.cpuFrameTimeMs;
  gpuTimes_[idx] = profiler.gpuFrameTimeMs;
  fps_[idx] = dt > 0.0f ? 1.0f / dt : 0.0f;
  drawCalls_[idx] = profiler.drawCalls;
  triangles_[idx] = profiler.triangles;
  if (historyCount_ < kHistorySize)
    historyCount_++;
}

void ImGuiOverlay::shutdown() {
  if (backend_) {
    backend_->waitIdle();
    backend_->shutdown();
    backend_.reset();
  }
  ImPlot::DestroyContext();
  ImGui::DestroyContext();
}

} // namespace Raiden::Engine
