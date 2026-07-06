#include <RaidenEditor/EditorPlugin.hpp>

#include <Raiden/Assets/IAssetManager.hpp>
#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/Logger.hpp>
#include <Raiden/Platform/IPlatform.hpp>
#include <Raiden/Renderer/ICommandBuffer.hpp>
#include <Raiden/Renderer/IPipeline.hpp>
#include <Raiden/Renderer/IRenderDevice.hpp>
#include <Raiden/Renderer/RenderTypes.hpp>

#include <RaidenUI/CSS/Selector.hpp>
#include <RaidenUI/Core/Signal.hpp>
#include <RaidenUI/DOM/Element.hpp>
#include <RaidenUI/DOM/XmlParser.hpp>
#include <RaidenUI/Layout/Solver.hpp>
#include <RaidenUI/Render/FontFace.hpp>

#include <ranges>

#include <SDL3/SDL_scancode.h>
#include <imgui.h>

static const Raiden::Core::Logger s_logger("EditorPlugin");

namespace RaidenEditor {

static const RaidenUI::ElementNode *hitTest(const RaidenUI::ElementNode *node,
                                            float mx, float my) {
  if (!node->visible) {
    return nullptr;
  }
  if (mx >= node->computedX && mx <= node->computedX + node->computedWidth &&
      my >= node->computedY && my <= node->computedY + node->computedHeight) {
    for (const auto &it : std::ranges::reverse_view(node->children)) {
      if (const auto *found = hitTest(it.get(), mx, my)) {
        return found;
      }
    }
    return node;
  }
  return nullptr;
}

void drawOutline(RaidenUI::QuadBatcher &batcher, float x, float y, float w,
                 float h, uint32_t color) {
  if (w <= 0 || h <= 0) {
    return;
  }
  batcher.addQuad(x, y, w, 1, color);
  batcher.addQuad(x, y + h - 1, w, 1, color);
  batcher.addQuad(x, y + 1, 1, h - 2, color);
  batcher.addQuad(x + w - 1, y + 1, 1, h - 2, color);
}

void drawElementTree(const RaidenUI::ElementNode *node,
                     const RaidenUI::ElementNode *&hovered,
                     const RaidenUI::ElementNode *&selected) {
  std::string label = node->tag;
  auto idIt = node->attrs.find("id");
  if (idIt != node->attrs.end()) {
    label += "#" + idIt->second;
  }
  auto classIt = node->attrs.find("class");
  if (classIt != node->attrs.end()) {
    label += "." + classIt->second;
  }

  ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None;
  if (node->children.empty()) {
    flags |= ImGuiTreeNodeFlags_Leaf;
  }
  if (node == selected) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }

  bool open = ImGui::TreeNodeEx(node, flags, "%s", label.c_str());
  if (ImGui::IsItemHovered()) {
    hovered = node;
    if (ImGui::IsMouseClicked(0)) {
      selected = node;
    }
  }

  if (open) {
    for (const auto &child : node->children) {
      drawElementTree(child.get(), hovered, selected);
    }
    ImGui::TreePop();
  }
}

bool EditorPlugin::init(Raiden::Renderer::IRenderDevice &device,
                        Raiden::Core::IVirtualFileSystem &vfs,
                        Raiden::Assets::IAssetManager &assets,
                        Raiden::Platform::IPlatform *platform,
                        Raiden::Audio::IAudioDevice *audio) {
  (void)audio;
  device_ = &device;
  assets_ = &assets;
  platform_ = platform;
  m_vfs = &vfs;

  s_logger.info("Initializing Raiden Editor...");

  // load stylesheet
  try {
    std::string css = vfs.readAll("game://editor/css/editor.css");
    stylesheet_ = RaidenUI::parseCss(css);
    s_logger.info("Loaded editor stylesheet ({} rules).",
                  stylesheet_.rules.size());
  } catch (const std::exception &e) {
    s_logger.warn("Failed to load editor CSS: {}", e.what());
  }

  // load UI layout
  try {
    std::string uiXml = vfs.readAll("game://editor/layouts/editor.rui.xml");
    auto doc = RaidenUI::parseXml(uiXml);
    uiRoot_ = std::move(doc.root);
    s_logger.info("Loaded editor UI layout.");
  } catch (const std::exception &e) {
    s_logger.warn("No editor UI layout found: {}", e.what());
  }

  // create UI pipeline (pre-multiplied alpha blending)
  Raiden::Renderer::PipelineDesc pipelineDesc{
      .shader = {.path = "shaders/ui.slang"},
      .vertexLayout = RaidenUI::getUIVertexLayout(),
      .depthTestEnable = false,
      .depthWriteEnable = false,
      .blendEnable = true,
      .cullMode = Raiden::Renderer::CullMode::None,
      .blendSrcFactor = Raiden::Renderer::BlendFactor::One,
      .blendDstFactor = Raiden::Renderer::BlendFactor::OneMinusSrcAlpha,
  };

  pipeline_ = device.createPipeline(pipelineDesc);
  if (!pipeline_) {
    s_logger.error("Failed to create UI pipeline.");
    return false;
  }

  batcher_ = std::make_unique<RaidenUI::QuadBatcher>(device);

  fontFace_ = std::make_unique<RaidenUI::FontFace>(
      device, vfs, "game://editor/fonts/InterVariable.ttf");

  return true;
}

void EditorPlugin::reloadStylesheet() {
  try {
    std::string css = m_vfs->readAll("game://editor/css/editor.css");
    stylesheet_ = RaidenUI::parseCss(css);
    s_logger.info("Reloaded editor stylesheet ({} rules).",
                  stylesheet_.rules.size());
  } catch (const std::exception &e) {
    s_logger.warn("Failed to reload editor CSS: {}", e.what());
  }
}

void EditorPlugin::update(float deltaTime,
                          const Raiden::Platform::InputState &input) {
  (void)deltaTime;

  if (uiRoot_) {
    events_.update(uiRoot_.get(), static_cast<float>(input.mouseX),
                   static_cast<float>(input.mouseY));
  }

  // toggle debug overlay with F12
  if (input.keysDown[SDL_SCANCODE_F12]) {
    m_debugOverlay = !m_debugOverlay;
  }

  // live CSS reload with F5
  if (input.keysDown[SDL_SCANCODE_F5]) {
    reloadStylesheet();
  }

  // hit-test under mouse
  if (uiRoot_ && m_debugOverlay) {
    m_hoveredNode = hitTest(uiRoot_.get(), static_cast<float>(input.mouseX),
                            static_cast<float>(input.mouseY));
  } else {
    m_hoveredNode = nullptr;
  }
}

void EditorPlugin::onDebugUI() {
  if (!m_debugOverlay || !uiRoot_) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(360, 500), ImGuiCond_FirstUseEver);
  ImGui::Begin("RaidenUI Inspector", &m_debugOverlay);

  if (ImGui::Button("Close")) {
    m_debugOverlay = false;
  }

  ImGui::Separator();

  // element tree
  if (ImGui::TreeNode("Element Tree")) {
    const RaidenUI::ElementNode *treeHovered = nullptr;
    const RaidenUI::ElementNode *treeSelected = m_hoveredNode;
    drawElementTree(uiRoot_.get(), treeHovered, treeSelected);
    if (treeHovered != nullptr) {
      m_hoveredNode = treeHovered;
    }
    if (treeSelected != nullptr) {
      m_hoveredNode = treeSelected;
    }
    ImGui::TreePop();
  }

  // hovered element properties
  if (m_hoveredNode != nullptr) {
    ImGui::Separator();
    ImGui::Text("Tag: %s", m_hoveredNode->tag.c_str());

    auto idIt = m_hoveredNode->attrs.find("id");
    if (idIt != m_hoveredNode->attrs.end()) {
      ImGui::Text("ID: %s", idIt->second.c_str());
    }
    auto classIt = m_hoveredNode->attrs.find("class");
    if (classIt != m_hoveredNode->attrs.end()) {
      ImGui::Text("Class: %s", classIt->second.c_str());
    }

    ImGui::Text("Position: (%.1f, %.1f)", m_hoveredNode->computedX,
                m_hoveredNode->computedY);
    ImGui::Text("Size: %.1f x %.1f", m_hoveredNode->computedWidth,
                m_hoveredNode->computedHeight);
    ImGui::Text("Content: \"%s\"", m_hoveredNode->content.c_str());
    ImGui::Text("Children: %zu", m_hoveredNode->children.size());

    if (ImGui::TreeNode("computed_style", "Computed Style (%zu)",
                        m_hoveredNode->computedStyle.size())) {
      for (const auto &[k, v] : m_hoveredNode->computedStyle) {
        ImGui::Text("%s: %s", k.c_str(), v.c_str());
      }
      ImGui::TreePop();
    }
  }

  ImGui::End();
}

void EditorPlugin::render(Raiden::Renderer::ICommandBuffer &cmd) {
  if (!uiRoot_ || !pipeline_ || !batcher_ || !fontFace_) {
    return;
  }

  int w = 0, h = 0;
  platform_->getWindowSize(w, h);

  uiRoot_->computedWidth = static_cast<float>(w);
  uiRoot_->computedHeight = static_cast<float>(h);

  RaidenUI::MeasureFn measure = [this](const RaidenUI::ElementNode *node,
                                       float) {
    float cssSize = 13.0F;
    auto it = node->computedStyle.find("font-size");
    if (it != node->computedStyle.end() && !it->second.empty()) {
      RaidenUI::Length len = RaidenUI::parseLength(it->second);
      if (len.value > 0) {
        cssSize = len.value;
      }
    }
    auto &atlas = fontFace_->getAtlas(cssSize);
    return atlas.measureText(node->content);
  };

  computeLayout(uiRoot_.get(), stylesheet_, static_cast<float>(w),
                static_cast<float>(h), measure);

  std::array<float, 2> screenSize = {static_cast<float>(w),
                                     static_cast<float>(h)};

  batcher_->begin();
  batcher_->addElementTree(uiRoot_.get(), stylesheet_, *fontFace_);

  if (m_debugOverlay) {
    drawDebugOverlay(uiRoot_.get());
  }

  batcher_->flush(cmd, *pipeline_, screenSize.data(), sizeof(screenSize));
}

void EditorPlugin::drawDebugOverlay(const RaidenUI::ElementNode *node) {
  if (!node->visible) {
    return;
  }

  float x = node->computedX;
  float y = node->computedY;
  float w = node->computedWidth;
  float h = node->computedHeight;

  if (w > 0 && h > 0) {
    bool isHovered = (node == m_hoveredNode);

    if (isHovered) {
      batcher_->addQuad(x, y, w, h, 0x330044AA);
      drawOutline(*batcher_, x, y, w, h, 0xFFFF4444);
    } else {
      drawOutline(*batcher_, x, y, w, h, 0x30FFFFFF);
    }
  }

  for (const auto &child : node->children) {
    drawDebugOverlay(child.get());
  }
}

void EditorPlugin::shutdown() {
  s_logger.info("Shutting down Raiden Editor...");
  fontFace_.reset();
  batcher_.reset();
  pipeline_.reset();
  uiRoot_.reset();
  stylesheet_.rules.clear();
}

} // namespace RaidenEditor
