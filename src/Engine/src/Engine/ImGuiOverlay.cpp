#include "ImGuiFonts/InterFont.hpp"

#include <Raiden/Engine/ImGuiOverlay.hpp>
#include <Raiden/Logger.hpp>

#include <SDL3/SDL_scancode.h>
#include <imgui.h>
#include <implot.h>

namespace Raiden::Engine {

using namespace ::Raiden::Platform;
using namespace ::Raiden::Renderer;

static const ::Raiden::Core::Logger s_logger("Raiden::Engine::ImGuiOverlay");

namespace {

void applyRaidenTheme() {
  ImGuiIO &io = ImGui::GetIO();

  io.Fonts->Clear();
  ImFont *font = io.Fonts->AddFontFromMemoryCompressedBase85TTF(
      InterVariable_compressed_data_base85, 14.0F);
  if (font == nullptr) {
    s_logger.warn(
        "Failed to load embedded Inter font, falling back to default");
    io.Fonts->AddFontDefault();
  }

  ImGui::StyleColorsDark();

  auto &colors = ImGui::GetStyle().Colors;

  colors[ImGuiCol_WindowBg] = ImVec4{0.07F, 0.06F, 0.06F, 1.0F};
  colors[ImGuiCol_ChildBg] = ImVec4{0.07F, 0.06F, 0.06F, 1.0F};
  colors[ImGuiCol_PopupBg] = ImVec4{0.07F, 0.06F, 0.06F, 0.96F};

  colors[ImGuiCol_MenuBarBg] = ImVec4{0.35F, 0.06F, 0.06F, 1.0F};

  colors[ImGuiCol_Border] = ImVec4{0.3F, 0.13F, 0.13F, 0.5F};
  colors[ImGuiCol_BorderShadow] = ImVec4{0.0F, 0.0F, 0.0F, 0.0F};

  colors[ImGuiCol_Text] = ImVec4{0.95F, 0.95F, 0.95F, 1.0F};
  colors[ImGuiCol_TextDisabled] = ImVec4{0.5F, 0.5F, 0.5F, 1.0F};

  colors[ImGuiCol_Header] = ImVec4{0.35F, 0.06F, 0.06F, 1.0F};
  colors[ImGuiCol_HeaderHovered] = ImVec4{0.45F, 0.09F, 0.09F, 1.0F};
  colors[ImGuiCol_HeaderActive] = ImVec4{0.4F, 0.08F, 0.08F, 1.0F};

  colors[ImGuiCol_Button] = ImVec4{0.35F, 0.06F, 0.06F, 1.0F};
  colors[ImGuiCol_ButtonHovered] = ImVec4{0.45F, 0.09F, 0.09F, 1.0F};
  colors[ImGuiCol_ButtonActive] = ImVec4{0.4F, 0.08F, 0.08F, 1.0F};
  colors[ImGuiCol_CheckMark] = ImVec4{0.98F, 0.58F, 0.58F, 1.0F};

  colors[ImGuiCol_SliderGrab] = ImVec4{0.61F, 0.37F, 0.37F, 0.54F};
  colors[ImGuiCol_SliderGrabActive] = ImVec4{0.98F, 0.58F, 0.58F, 0.54F};

  colors[ImGuiCol_FrameBg] = ImVec4{0.16F, 0.09F, 0.09F, 1.0F};
  colors[ImGuiCol_FrameBgHovered] = ImVec4{0.24F, 0.11F, 0.11F, 1.0F};
  colors[ImGuiCol_FrameBgActive] = ImVec4{0.3F, 0.13F, 0.13F, 1.0F};

  colors[ImGuiCol_Tab] = ImVec4{0.16F, 0.05F, 0.05F, 1.0F};
  colors[ImGuiCol_TabHovered] = ImVec4{0.45F, 0.09F, 0.09F, 1.0F};
  colors[ImGuiCol_TabActive] = ImVec4{0.35F, 0.06F, 0.06F, 1.0F};
  colors[ImGuiCol_TabUnfocused] = ImVec4{0.08F, 0.04F, 0.04F, 1.0F};
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4{0.16F, 0.05F, 0.05F, 1.0F};

  colors[ImGuiCol_TitleBg] = ImVec4{0.2F, 0.04F, 0.04F, 1.0F};
  colors[ImGuiCol_TitleBgActive] = ImVec4{0.35F, 0.06F, 0.06F, 1.0F};
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4{0.15F, 0.03F, 0.03F, 1.0F};

  colors[ImGuiCol_ScrollbarBg] = ImVec4{0.07F, 0.06F, 0.06F, 1.0F};
  colors[ImGuiCol_ScrollbarGrab] = ImVec4{0.35F, 0.06F, 0.06F, 1.0F};
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4{0.45F, 0.09F, 0.09F, 1.0F};
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4{0.55F, 0.11F, 0.11F, 1.0F};

  colors[ImGuiCol_Separator] = ImVec4{0.4F, 0.16F, 0.16F, 1.0F};
  colors[ImGuiCol_SeparatorHovered] = ImVec4{0.98F, 0.58F, 0.58F, 1.0F};
  colors[ImGuiCol_SeparatorActive] = ImVec4{1.0F, 0.58F, 0.58F, 1.0F};

  colors[ImGuiCol_ResizeGrip] = ImVec4{0.4F, 0.16F, 0.16F, 0.3F};
  colors[ImGuiCol_ResizeGripHovered] = ImVec4{0.6F, 0.2F, 0.2F, 0.5F};
  colors[ImGuiCol_ResizeGripActive] = ImVec4{0.7F, 0.22F, 0.22F, 0.7F};

  auto &style = ImGui::GetStyle();
  style.TabRounding = 0;
  style.ScrollbarRounding = 0;
  style.WindowRounding = 0;
  style.GrabRounding = 0;
  style.FrameRounding = 0;
  style.PopupRounding = 0;
  style.ChildRounding = 0;

  style.WindowPadding = ImVec2{8.0F, 6.0F};
  style.FramePadding = ImVec2{4.0F, 3.0F};
  style.ItemSpacing = ImVec2{6.0F, 4.0F};
  style.ItemInnerSpacing = ImVec2{4.0F, 3.0F};
  style.WindowBorderSize = 1.0F;
  style.PopupBorderSize = 1.0F;
  style.FrameBorderSize = 0.0F;
  style.IndentSpacing = 12.0F;
}

void statItem(const char *label, const char *fmt, ...) {
  ImGui::TextDisabled("%s", label);
  ImGui::SameLine();

  va_list args;
  va_start(args, fmt);
  ImGui::TextV(fmt, args);
  va_end(args);
}

void applyRaidenPlotTheme() {
  ImPlot::StyleColorsDark();

  auto &plotColors = ImPlot::GetStyle().Colors;
  plotColors[ImPlotCol_FrameBg] = ImVec4{0.0F, 0.0F, 0.0F, 0.0F};
  plotColors[ImPlotCol_PlotBg] = ImVec4{0.09F, 0.06F, 0.06F, 1.0F};
  plotColors[ImPlotCol_PlotBorder] = ImVec4{0.3F, 0.13F, 0.13F, 0.5F};
  plotColors[ImPlotCol_LegendBg] = ImVec4{0.09F, 0.06F, 0.06F, 0.9F};
  plotColors[ImPlotCol_LegendBorder] = ImVec4{0.3F, 0.13F, 0.13F, 0.5F};
  plotColors[ImPlotCol_AxisGrid] = ImVec4{0.4F, 0.16F, 0.16F, 0.3F};
  plotColors[ImPlotCol_AxisText] = ImVec4{0.85F, 0.75F, 0.75F, 1.0F};

  auto &plotStyle = ImPlot::GetStyle();
  plotStyle.PlotBorderSize = 1.0F;
  plotStyle.PlotPadding = ImVec2{8.0F, 8.0F};
  plotStyle.LegendPadding = ImVec2{6.0F, 4.0F};
  plotStyle.FillAlpha = 0.25F;
  plotStyle.LineWeight = 1.5F;
}

} // namespace

ImGuiOverlay::~ImGuiOverlay() { shutdown(); }

bool ImGuiOverlay::init(std::unique_ptr<IImGuiBackend> backend) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  applyRaidenPlotTheme();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = nullptr;

  applyRaidenTheme();

  backend_ = std::move(backend);

  if (!backend_->init()) {
    s_logger.error("Failed to init ImGui backend");
    return false;
  }

  s_logger.info("ImGui overlay initialized");
  return true;
}

void ImGuiOverlay::newFrame(const InputState &input, int displayW, int displayH,
                            float dt, const ProfilerFrameData &profiler,
                            const std::function<void()> &pluginDebugUI) {
  backend_->newFrame();

  ImGuiIO &io = ImGui::GetIO();
  io.DisplaySize =
      ImVec2(static_cast<float>(displayW), static_cast<float>(displayH));
  io.DeltaTime = dt > 0.0F ? dt : 1.0F / 60.0F;

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

  statItem("CPU (avg)", "%.2f ms", avgCpuMs_);
  ImGui::SameLine(0.0F, 20.0F);
  statItem("GPU (avg)", "%.2f ms", avgGpuMs_);
  ImGui::SameLine(0.0F, 20.0F);
  statItem("FPS (avg)", "%.0f", avgFps_);

  statItem("Draw calls", "%u", profiler.drawCalls);
  ImGui::SameLine(0.0F, 20.0F);
  statItem("Triangles", "%u", profiler.triangles);

  ImGui::Separator();

  ImGui::Checkbox("Frame time", &showFrameTime_);
  ImGui::Checkbox("FPS", &showFps_);
  ImGui::Checkbox("Draw calls & Triangles", &showDrawCalls_);

  ImGui::End();

  // subwindows
  if (showFrameTime_) {
    ImGui::Begin("Frame Time", &showFrameTime_);
    if (ImPlot::BeginPlot("##frameTime", ImVec2{-1, 200})) {
      ImPlot::SetupAxes(nullptr, "ms", ImPlotAxisFlags_NoGridLines,
                        ImPlotAxisFlags_None);
      ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 33);
      ImPlot::SetupAxisLimits(ImAxis_X1, 0, kHistorySize, ImGuiCond_Always);

      ImPlot::SetNextLineStyle(ImVec4{0.98F, 0.58F, 0.58F, 1.0F});
      ImPlot::SetNextFillStyle(ImVec4{0.98F, 0.58F, 0.58F, 1.0F});
      ImPlot::PlotShaded("CPU", cpuTimes_.data(), historyCount_);
      ImPlot::PlotLine("CPU", cpuTimes_.data(), historyCount_);

      ImPlot::SetNextLineStyle(ImVec4{0.6F, 0.75F, 0.85F, 1.0F});
      ImPlot::SetNextFillStyle(ImVec4{0.6F, 0.75F, 0.85F, 1.0F});
      ImPlot::PlotShaded("GPU", gpuTimes_.data(), historyCount_);
      ImPlot::PlotLine("GPU", gpuTimes_.data(), historyCount_);

      ImPlot::EndPlot();
    }
    ImGui::End();
  }

  if (showFps_) {
    ImGui::Begin("FPS", &showFps_);
    if (ImPlot::BeginPlot("##fps", ImVec2{-1, 160})) {
      ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoGridLines,
                        ImPlotAxisFlags_None);
      ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 240,
                              ImGuiCond_Always); // cap instead of auto-fit
      ImPlot::SetupAxisLimits(ImAxis_X1, 0, kHistorySize, ImGuiCond_Always);

      ImPlot::SetNextLineStyle(ImVec4{0.98F, 0.58F, 0.58F, 1.0F});
      ImPlot::SetNextFillStyle(ImVec4{0.98F, 0.58F, 0.58F, 1.0F});
      ImPlot::PlotShaded("FPS", fps_.data(), historyCount_);
      ImPlot::PlotLine("FPS", fps_.data(), historyCount_);
      ImPlot::EndPlot();
    }
    ImGui::End();
  }

  if (showDrawCalls_) {
    ImGui::Begin("Draw Calls & Triangles", &showDrawCalls_);
    if (ImPlot::BeginPlot("##drawCalls", ImVec2{-1, 180})) {
      ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoGridLines,
                        ImPlotAxisFlags_None);
      ImPlot::SetupAxisLimits(ImAxis_X1, 0, kHistorySize, ImGuiCond_Always);

      ImPlot::SetNextLineStyle(ImVec4{0.98F, 0.58F, 0.58F, 1.0F});
      ImPlot::SetNextFillStyle(ImVec4{0.98F, 0.58F, 0.58F, 1.0F});
      ImPlot::PlotShaded("Draws", drawCalls_.data(), historyCount_);
      ImPlot::PlotLine("Draws", drawCalls_.data(), historyCount_);

      ImPlot::SetNextLineStyle(ImVec4{0.6F, 0.75F, 0.85F, 1.0F});
      ImPlot::SetNextFillStyle(ImVec4{0.6F, 0.75F, 0.85F, 1.0F});
      ImPlot::PlotShaded("Tris", triangles_.data(), historyCount_);
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

void ImGuiOverlay::endFrame() { ImGui::Render(); }

void ImGuiOverlay::renderDrawData(ICommandBuffer &cmd) {
  backend_->renderDrawData(cmd);
}

void ImGuiOverlay::pushPerfData(const ProfilerFrameData &profiler, float dt) {
  int idx = historyCount_ % kHistorySize;

  cpuTimes_[idx] = profiler.cpuFrameTimeMs;
  gpuTimes_[idx] = profiler.gpuFrameTimeMs;

  fpsAccumTime_ += dt;
  fpsAccumFrames_++;

  cpuAccumTime_ += profiler.cpuFrameTimeMs;
  gpuAccumTime_ += profiler.gpuFrameTimeMs;
  perfAccumFrames_++;

  if (fpsAccumTime_ >= kFpsUpdateInterval) {
    avgFps_ = static_cast<float>(fpsAccumFrames_) / fpsAccumTime_;
    fpsAccumTime_ = 0.0F;
    fpsAccumFrames_ = 0;

    avgCpuMs_ = cpuAccumTime_ / static_cast<float>(perfAccumFrames_);
    avgGpuMs_ = gpuAccumTime_ / static_cast<float>(perfAccumFrames_);
    cpuAccumTime_ = 0.0F;
    gpuAccumTime_ = 0.0F;
    perfAccumFrames_ = 0;
  }

  fps_[idx] = avgFps_;

  drawCalls_[idx] = profiler.drawCalls;
  triangles_[idx] = profiler.triangles;

  if (historyCount_ < kHistorySize) {
    historyCount_++;
  }
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
