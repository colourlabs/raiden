#pragma once

#include <Raiden/Assets/IAssetManager.hpp>
#include <Raiden/ECS/World.hpp>

namespace Raiden::Core { class IVirtualFileSystem; }
namespace Raiden::Assets { class IAssetManager; }
namespace Raiden::Platform { class IPlatform; struct InputState; }
namespace Raiden::Audio { class IAudioDevice; }
namespace Raiden::Renderer { class IRenderDevice; class ICommandBuffer; }

namespace Raiden::Engine {

class IGamePlugin { // NOLINT
public:
  virtual ~IGamePlugin() = default;
  
  [[nodiscard]] virtual const char *name() const = 0;
  virtual bool init(::Raiden::Renderer::IRenderDevice &device, ::Raiden::Core::IVirtualFileSystem &vfs, ::Raiden::Assets::IAssetManager &assets, ::Raiden::Platform::IPlatform *platform, ::Raiden::Audio::IAudioDevice *audio = nullptr) = 0;
  virtual void update(float deltaTime, const ::Raiden::Platform::InputState &input) = 0;
  virtual void render(::Raiden::Renderer::ICommandBuffer &cmd) = 0;
  virtual Raiden::ECS::World *getWorld() { return nullptr; }
  virtual void onDebugUI() {}
  virtual void shutdown() = 0;
};

} // namespace Raiden::Engine
