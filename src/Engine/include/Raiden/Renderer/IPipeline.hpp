#pragma once

namespace Raiden::Renderer {

class IPipeline {
public:
  IPipeline() = default;
  virtual ~IPipeline() = default;

  IPipeline(const IPipeline&) = delete;
  IPipeline& operator=(const IPipeline&) = delete;
  IPipeline(IPipeline&&) = delete;
  IPipeline& operator=(IPipeline&&) = delete;
};

} // namespace Raiden::Renderer
