#pragma once

#include <Raiden/Engine/IGamePlugin.hpp>

#include <RaidenUI/CSS/Parser.hpp>
#include <RaidenUI/DOM/Events.hpp>
#include <RaidenUI/Render/QuadBatcher.hpp>

#include <memory>

namespace Raiden::Renderer {
class IPipeline;
}

namespace RaidenEditor {

class EditorPlugin final : public Raiden::Engine::IGamePlugin {
public:
  EditorPlugin() = default;
  ~EditorPlugin() override = default;

  EditorPlugin(const EditorPlugin &) = delete;
  EditorPlugin &operator=(const EditorPlugin &) = delete;
  EditorPlugin(EditorPlugin &&) = delete;
  EditorPlugin &operator=(EditorPlugin &&) = delete;

  const char *name() const override { return "Raiden Editor"; }

  bool init(Raiden::Renderer::IRenderDevice &device,
            Raiden::Core::IVirtualFileSystem &vfs,
            Raiden::Assets::IAssetManager &assets,
            Raiden::Platform::IPlatform *platform,
            Raiden::Audio::IAudioDevice *audio = nullptr) override;

  void update(float deltaTime,
              const Raiden::Platform::InputState &input) override;
  void render(Raiden::Renderer::ICommandBuffer &cmd) override;
  void shutdown() override;

private:
  Raiden::Renderer::IRenderDevice *device_ = nullptr;
  Raiden::Assets::IAssetManager *assets_ = nullptr;
  Raiden::Platform::IPlatform *platform_ = nullptr;

  std::unique_ptr<RaidenUI::ElementNode> uiRoot_;
  RaidenUI::CssStylesheet stylesheet_;
  RaidenUI::EventSystem events_;
  std::unique_ptr<RaidenUI::QuadBatcher> batcher_;
  std::unique_ptr<Raiden::Renderer::IPipeline> pipeline_;
};

} // namespace RaidenEditor
