#include <RaidenUI/CSS/Properties.hpp>
#include <RaidenUI/Render/FontAtlas.hpp>
#include <RaidenUI/Render/QuadBatcher.hpp>

#include <Raiden/Renderer/IBuffer.hpp>
#include <Raiden/Renderer/IPipeline.hpp>
#include <Raiden/Renderer/RenderTypes.hpp>

#include <algorithm>
#include <cstring>
#include <string>

namespace RaidenUI {

using namespace Raiden::Renderer;

// vertex layout

VertexLayout getUIVertexLayout() {
  VertexLayout layout;

  layout.stride = sizeof(UIVertex);
  layout.attributes = {
      {.location = 0,
       .format = Format::R32G32_Float,
       .offset = offsetof(UIVertex, x)},
      {.location = 1,
       .format = Format::R32G32_Float,
       .offset = offsetof(UIVertex, u)},
      {.location = 2,
       .format = Format::R8G8B8A8_UNORM,
       .offset = offsetof(UIVertex, color)},
  };

  return layout;
}

// QuadBatcher

QuadBatcher::QuadBatcher(IRenderDevice &device) : m_device(&device) {
  growBuffers(4096);

  TextureDesc desc;
  desc.width = 1;
  desc.height = 1;
  desc.format = Format::R8G8B8A8_UNORM;
  desc.type = TextureType::Texture2D;
  m_whiteTexture = m_device->createTexture(desc);
  if (m_whiteTexture) {
    uint32_t whitePixel = 0xFFFFFFFF;
    m_whiteTexture->upload(&whitePixel, sizeof(whitePixel));
  }
}

QuadBatcher::~QuadBatcher() = default;

void QuadBatcher::begin() { m_quads.clear(); }

void QuadBatcher::addQuad(float x, float y, float w, float h, uint32_t color,
                          const ITexture *texture) {
  UIQuad quad;
  quad.verts[0] = {.x = x, .y = y, .u = 0, .v = 0, .color = color};
  quad.verts[1] = {.x = x + w, .y = y, .u = 1, .v = 0, .color = color};
  quad.verts[2] = {.x = x + w, .y = y + h, .u = 1, .v = 1, .color = color};
  quad.verts[3] = {.x = x, .y = y + h, .u = 0, .v = 1, .color = color};
  quad.texture = (texture != nullptr) ? texture : m_whiteTexture.get();

  m_quads.push_back(quad);
}

void QuadBatcher::addGlyphQuad(float x, float y, float w, float h,
                               uint32_t color, float u0, float v0, float u1,
                               float v1, const ITexture *texture) {
  UIQuad quad;
  quad.verts[0] = {.x = x, .y = y, .u = u0, .v = v1, .color = color};
  quad.verts[1] = {.x = x + w, .y = y, .u = u1, .v = v1, .color = color};
  quad.verts[2] = {.x = x + w, .y = y + h, .u = u1, .v = v0, .color = color};
  quad.verts[3] = {.x = x, .y = y + h, .u = u0, .v = v0, .color = color};
  quad.texture = (texture != nullptr) ? texture : m_whiteTexture.get();
  m_quads.push_back(quad);
}

void QuadBatcher::growBuffers(size_t minVerts) {
  size_t newCap = std::max(m_bufferCapacity * 2, minVerts);
  newCap = ((newCap + 1023) / 1024) * 1024;

  BufferDesc vbDesc;
  vbDesc.size = newCap * sizeof(UIVertex);
  vbDesc.usage = BufferUsage::Vertex;
  vbDesc.access = MemoryAccess::CpuToGpu;

  BufferDesc ibDesc;
  ibDesc.size = (newCap / 4) * 6 * sizeof(uint16_t);
  ibDesc.usage = BufferUsage::Index;
  ibDesc.access = MemoryAccess::CpuToGpu;
  ibDesc.indexType = IndexType::Uint16;

  m_vertexBuffer = m_device->createBuffer(vbDesc);
  m_indexBuffer = m_device->createBuffer(ibDesc);
  m_bufferCapacity = newCap;
}

void QuadBatcher::flush(ICommandBuffer &cmd, const IPipeline &pipeline,
                        const void *pushData, uint32_t pushSize) {
  if (m_quads.empty()) {
    return;
  }

  size_t vertCount = m_quads.size() * 4;
  size_t idxCount = m_quads.size() * 6;

  if (vertCount > m_bufferCapacity) {
    growBuffers(vertCount);
  }

  // build vertex and index data
  std::vector<UIVertex> verts;
  std::vector<uint16_t> indices;
  verts.reserve(vertCount);
  indices.reserve(idxCount);

  for (const auto &quad : m_quads) {
    auto base = static_cast<uint32_t>(verts.size());

    for (const auto &v : quad.verts) {
      verts.push_back(v);
    }

    indices.push_back(static_cast<uint16_t>(base));
    indices.push_back(static_cast<uint16_t>(base + 1));
    indices.push_back(static_cast<uint16_t>(base + 2));
    indices.push_back(static_cast<uint16_t>(base));
    indices.push_back(static_cast<uint16_t>(base + 2));
    indices.push_back(static_cast<uint16_t>(base + 3));
  }

  // upload vertex data
  {
    auto *dst = static_cast<uint8_t *>(m_vertexBuffer->map());
    if (dst != nullptr) {
      std::memcpy(dst, verts.data(), verts.size() * sizeof(UIVertex));
      m_vertexBuffer->unmap();
    }
  }

  // upload index data
  {
    auto *dst = static_cast<uint8_t *>(m_indexBuffer->map());
    if (dst != nullptr) {
      std::memcpy(dst, indices.data(), indices.size() * sizeof(uint16_t));
      m_indexBuffer->unmap();
    }
  }

  // draw batched by texture
  cmd.bindPipeline(pipeline);

  if (pushData != nullptr && pushSize > 0) {
    cmd.pushConstants(0, pushSize, pushData);
  }

  cmd.bindVertexBuffer(*m_vertexBuffer);
  cmd.bindIndexBuffer(*m_indexBuffer);

  size_t quadStart = 0;
  const ITexture *currentTex = nullptr;

  for (size_t i = 0; i < m_quads.size(); ++i) {
    const ITexture *tex = m_quads[i].texture;

    if (tex != currentTex) {
      size_t batchCount = i - quadStart;

      if (batchCount > 0) {
        if (currentTex != nullptr) {
          cmd.bindTexture(0, *currentTex);
        }

        cmd.drawIndexedInstanced(static_cast<uint32_t>(batchCount * 6), 1,
                                 static_cast<uint32_t>(quadStart * 6));
      }

      currentTex = tex;
      quadStart = i;
    }
  }

  // flush remaining batch
  size_t remaining = m_quads.size() - quadStart;
  if (remaining > 0) {
    if (currentTex != nullptr) {
      cmd.bindTexture(0, *currentTex);
    }

    cmd.drawIndexedInstanced(static_cast<uint32_t>(remaining * 6), 1,
                             static_cast<uint32_t>(quadStart * 6));
  }
}

static void collectQuads(ElementNode *node, const CssStylesheet &stylesheet,
                         const FontAtlas &fontAtlas, QuadBatcher &batcher) {
  if (!node->visible) {
    return;
  }

  ComputedStyle style = resolveStyle(node, stylesheet);

  auto bgColor = style.find("background-color");
  auto bg = style.find("background");
  const std::string *colorStr = nullptr;

  if (bgColor != style.end() && !bgColor->second.empty()) {
    colorStr = &bgColor->second;
  } else if (bg != style.end() && !bg->second.empty()) {
    colorStr = &bg->second;
  }

  if (colorStr != nullptr) {
    uint32_t color = parseCssColor(*colorStr);
    if (color != 0) {
      batcher.addQuad(node->computedX, node->computedY, node->computedWidth,
                      node->computedHeight, color);
    }
  }

  // Draw node text content if any
  if (!node->content.empty()) {
    auto textColorProp = style.find("color");
    uint32_t textColor = 0xFFFFFFFF; // default to white
    if (textColorProp != style.end() && !textColorProp->second.empty()) {
      textColor = parseCssColor(textColorProp->second);
    }

    float x = node->computedX;
    float y = node->computedY + fontAtlas.ascent();

    for (char c : node->content) {
      if (const GlyphInfo *g = fontAtlas.glyph(static_cast<char32_t>(c))) {
        float gx = x + g->bearingX;
        float gy = y + g->bearingY;
        batcher.addGlyphQuad(gx, gy, g->width, g->height, textColor, g->u0,
                             g->v0, g->u1, g->v1, fontAtlas.texture());
        x += g->advance;
      }
    }
  }

  for (auto &child : node->children) {
    collectQuads(child.get(), stylesheet, fontAtlas, batcher);
  }
}

void QuadBatcher::addElementTree(ElementNode *root,
                                 const CssStylesheet &stylesheet,
                                 const FontAtlas &fontAtlas) {
  collectQuads(root, stylesheet, fontAtlas, *this);
}

} // namespace RaidenUI
