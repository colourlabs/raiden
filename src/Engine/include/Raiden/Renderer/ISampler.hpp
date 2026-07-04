#pragma once

namespace Raiden::Renderer {

class ISampler {
public:
  ISampler() = default;
  ISampler(const ISampler&) = delete;
  ISampler& operator=(const ISampler&) = delete;
  ISampler(ISampler&&) = delete;
  ISampler& operator=(ISampler&&) = delete;
  
  virtual ~ISampler() = default;
};

} // namespace Raiden::Renderer
