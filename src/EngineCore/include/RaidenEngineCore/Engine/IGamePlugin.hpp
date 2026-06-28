#pragma once


#include <RaidenEngineCore/Assets/IAssetManager.hpp>
#include <RaidenEngineCore/ECS/World.hpp>

namespace Raiden::Core {

class IVirtualFileSystem;
class IAssetManager;

}

namespace Raiden::Core {

class IRenderDevice;
class ICommandBuffer;
struct InputState;

class IGamePlugin {
public:
  virtual ~IGamePlugin() = default;
  virtual const char *name() const = 0;
  virtual bool init(IRenderDevice &device, IVirtualFileSystem &vfs, IAssetManager &assets) = 0;
  virtual void update(float deltaTime, const InputState &input) = 0;
  virtual void render(ICommandBuffer &cmd) = 0;
  virtual Raiden::Core::World *getWorld() { return nullptr; }
  virtual void onDebugUI() {}
  virtual void shutdown() = 0;
};

} // namespace Raiden::Core

extern "C" {
Raiden::Core::IGamePlugin *raiden_create_plugin();
void raiden_destroy_plugin(Raiden::Core::IGamePlugin *plugin);
}
