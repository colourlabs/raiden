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

static const Raiden::Core::Logger s_logger("EditorPlugin");

namespace RaidenEditor {

bool EditorPlugin::init(Raiden::Renderer::IRenderDevice &device,
                        Raiden::Core::IVirtualFileSystem &vfs,
                        Raiden::Assets::IAssetManager &assets,
                        Raiden::Platform::IPlatform *platform,
                        Raiden::Audio::IAudioDevice *audio) {
  (void)audio;
  device_ = &device;
  assets_ = &assets;
  platform_ = platform;

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

  // create UI pipeline
  Raiden::Renderer::PipelineDesc pipelineDesc;
  pipelineDesc.shader.path = "shaders/ui.slang";
  pipelineDesc.vertexLayout = RaidenUI::getUIVertexLayout();
  pipelineDesc.depthTestEnable = false;
  pipelineDesc.depthWriteEnable = false;
  pipelineDesc.blendEnable = true;
  pipelineDesc.cullMode = Raiden::Renderer::CullMode::None;

  pipeline_ = device.createPipeline(pipelineDesc);
  if (!pipeline_) {
    s_logger.error("Failed to create UI pipeline.");
    return false;
  }

  batcher_ = std::make_unique<RaidenUI::QuadBatcher>(device);

  fontAtlas_ = std::make_unique<RaidenUI::FontAtlas>(
      device, vfs, "game://editor/fonts/InterVariable.ttf", 14.0F);
  if (!*fontAtlas_) {
    s_logger.error("Failed to load editor font atlas.");
    return false;
  }

  return true;
}

void EditorPlugin::update(float deltaTime,
                          const Raiden::Platform::InputState &input) {
  (void)deltaTime;

  if (uiRoot_) {
    events_.update(uiRoot_.get(), static_cast<float>(input.mouseX),
                   static_cast<float>(input.mouseY));
  }
}

void EditorPlugin::render(Raiden::Renderer::ICommandBuffer &cmd) {
  if (!uiRoot_ || !pipeline_ || !batcher_ || !fontAtlas_) {
    return;
  }

  int w = 0, h = 0;
  platform_->getWindowSize(w, h);

  RaidenUI::MeasureFn measure = [this](const RaidenUI::ElementNode *node, float /*avail*/) {
    return fontAtlas_->measureText(node->content);
  };

  computeLayout(uiRoot_.get(), stylesheet_,
               static_cast<float>(w), static_cast<float>(h), measure);

  std::array<float, 2> screenSize = {static_cast<float>(w), static_cast<float>(h)};

  batcher_->begin();
  batcher_->addElementTree(uiRoot_.get(), stylesheet_, *fontAtlas_);
  batcher_->flush(cmd, *pipeline_, screenSize.data(), sizeof(screenSize));
}

void EditorPlugin::shutdown() {
  s_logger.info("Shutting down Raiden Editor...");
  fontAtlas_.reset();
  batcher_.reset();
  pipeline_.reset();
  uiRoot_.reset();
  stylesheet_.rules.clear();
}

} // namespace RaidenEditor
