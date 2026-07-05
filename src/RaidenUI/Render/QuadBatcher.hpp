#pragma once

#include <RaidenUI/CSS/Parser.hpp>
#include <RaidenUI/CSS/Selector.hpp>
#include <RaidenUI/DOM/Element.hpp>

#include <Raiden/Renderer/ICommandBuffer.hpp>
#include <Raiden/Renderer/IRenderDevice.hpp>
#include <Raiden/Renderer/ITexture.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace RaidenUI {

struct UIVertex {
  float x;
  float y;
  float u;
  float v;
  uint32_t color;
};

struct UIQuad {
  std::array<UIVertex, 4> verts;
  const Raiden::Renderer::ITexture *texture{nullptr};
};

Raiden::Renderer::VertexLayout getUIVertexLayout();

class FontAtlas;

class QuadBatcher {
public:
  explicit QuadBatcher(Raiden::Renderer::IRenderDevice &device);
  ~QuadBatcher();

  QuadBatcher(const QuadBatcher &) = delete;
  QuadBatcher &operator=(const QuadBatcher &) = delete;

  QuadBatcher(QuadBatcher &&) noexcept;
  QuadBatcher &operator=(QuadBatcher &&) noexcept;

  void begin();
  void addQuad(float x, float y, float w, float h, uint32_t color,
               const Raiden::Renderer::ITexture *texture = nullptr);
  void flush(Raiden::Renderer::ICommandBuffer &cmd,
             const Raiden::Renderer::IPipeline &pipeline,
             const void *pushData = nullptr, uint32_t pushSize = 0);

  void addGlyphQuad(float x, float y, float w, float h, uint32_t color,
                    float u0, float v0, float u1, float v1,
                    const Raiden::Renderer::ITexture *texture);

  [[nodiscard]] size_t quadCount() const { return m_bgQuads.size(); }
  [[nodiscard]] size_t glyphCount() const { return m_glyphQuads.size(); }
  void addElementTree(ElementNode *root, const CssStylesheet &stylesheet,
                      const FontAtlas &fontAtlas);

private:
  void growBuffers(size_t minVerts);
  void flushBatch(Raiden::Renderer::ICommandBuffer &cmd,
                  const Raiden::Renderer::IPipeline &pipeline,
                  const void *pushData, uint32_t pushSize,
                  const std::vector<UIQuad> &quads);

  Raiden::Renderer::IRenderDevice *m_device;
  std::unique_ptr<Raiden::Renderer::IBuffer> m_vertexBuffer;
  std::unique_ptr<Raiden::Renderer::IBuffer> m_indexBuffer;
  std::unique_ptr<Raiden::Renderer::ITexture> m_whiteTexture;
  std::vector<UIQuad> m_bgQuads;
  std::vector<UIQuad> m_glyphQuads;
  size_t m_bufferCapacity{0};
};

} // namespace RaidenUI
