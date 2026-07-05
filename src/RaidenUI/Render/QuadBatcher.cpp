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

void QuadBatcher::begin() {
  m_bgQuads.clear();
  m_glyphQuads.clear();
}

void QuadBatcher::addQuad(float x, float y, float w, float h, uint32_t color,
                          const ITexture *texture) {
  UIQuad quad;
  quad.verts[0] = {.x = x, .y = y, .u = 0, .v = 0, .color = color};
  quad.verts[1] = {.x = x + w, .y = y, .u = 1, .v = 0, .color = color};
  quad.verts[2] = {.x = x + w, .y = y + h, .u = 1, .v = 1, .color = color};
  quad.verts[3] = {.x = x, .y = y + h, .u = 0, .v = 1, .color = color};
  quad.texture = (texture != nullptr) ? texture : m_whiteTexture.get();

  m_bgQuads.push_back(quad);
}

void QuadBatcher::addGlyphQuad(float x, float y, float w, float h,
                               uint32_t color, float u0, float v0, float u1,
                               float v1, const ITexture *texture) {
  UIQuad quad;
  quad.verts[0] = {.x = x, .y = y, .u = u0, .v = v0, .color = color};
  quad.verts[1] = {.x = x + w, .y = y, .u = u1, .v = v0, .color = color};
  quad.verts[2] = {.x = x + w, .y = y + h, .u = u1, .v = v1, .color = color};
  quad.verts[3] = {.x = x, .y = y + h, .u = u0, .v = v1, .color = color};
  quad.texture = (texture != nullptr) ? texture : m_whiteTexture.get();
  m_glyphQuads.push_back(quad);
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

void QuadBatcher::flushBatch(ICommandBuffer &cmd, const IPipeline &pipeline,
                             const void *pushData, uint32_t pushSize,
                             const std::vector<UIQuad> &quads) {
  if (quads.empty()) {
    return;
  }

  size_t vertCount = quads.size() * 4;
  size_t idxCount = quads.size() * 6;

  if (vertCount > m_bufferCapacity) {
    growBuffers(vertCount);
  }

  // build vertex and index data
  std::vector<UIVertex> verts;
  std::vector<uint16_t> indices;
  verts.reserve(vertCount);
  indices.reserve(idxCount);

  for (const auto &quad : quads) {
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

  for (size_t i = 0; i < quads.size(); ++i) {
    const ITexture *tex = quads[i].texture;

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
  size_t remaining = quads.size() - quadStart;
  if (remaining > 0) {
    if (currentTex != nullptr) {
      cmd.bindTexture(0, *currentTex);
    }

    cmd.drawIndexedInstanced(static_cast<uint32_t>(remaining * 6), 1,
                             static_cast<uint32_t>(quadStart * 6));
  }
}

void QuadBatcher::flush(ICommandBuffer &cmd, const IPipeline &pipeline,
                        const void *pushData, uint32_t pushSize) {
  m_bgQuads.insert(m_bgQuads.end(), m_glyphQuads.begin(), m_glyphQuads.end());
  flushBatch(cmd, pipeline, pushData, pushSize, m_bgQuads);
}

static void collectQuads(ElementNode *node, const CssStylesheet &stylesheet,
                         const FontAtlas &fontAtlas, QuadBatcher &batcher,
                         const ComputedStyle *parentStyle = nullptr) {
  if (!node->visible) {
    return;
  }

  const ComputedStyle &style = resolveStyle(node, stylesheet, parentStyle);

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

  // borders
  {
    auto parseBorderWidth = [&](const char *prop) -> float {
      auto it = style.find(prop);
      if (it != style.end() && !it->second.empty()) {
        return parseLength(it->second).value;
      }
      return 0;
    };
    auto parseBorderColor = [&](const char *prop) -> uint32_t {
      auto it = style.find(prop);
      if (it != style.end() && !it->second.empty()) {
        return parseCssColor(it->second);
      }
      return 0;
    };

    float bwT = parseBorderWidth("border-top-width");
    float bwR = parseBorderWidth("border-right-width");
    float bwB = parseBorderWidth("border-bottom-width");
    float bwL = parseBorderWidth("border-left-width");

    uint32_t bcT = parseBorderColor("border-top-color");
    uint32_t bcR = parseBorderColor("border-right-color");
    uint32_t bcB = parseBorderColor("border-bottom-color");
    uint32_t bcL = parseBorderColor("border-left-color");

    float x = node->computedX;
    float y = node->computedY;
    float w = node->computedWidth;
    float h = node->computedHeight;

    if (bwT > 0 && bcT != 0) {
      batcher.addQuad(x, y, w, bwT, bcT);
    }
    if (bwB > 0 && bcB != 0) {
      batcher.addQuad(x, y + h - bwB, w, bwB, bcB);
    }
    if (bwL > 0 && bcL != 0) {
      batcher.addQuad(x, y + bwT, bwL, h - bwT - bwB, bcL);
    }
    if (bwR > 0 && bcR != 0) {
      batcher.addQuad(x + w - bwR, y + bwT, bwR, h - bwT - bwB, bcR);
    }
  }

  if (!node->content.empty()) {
    auto textColorProp = style.find("color");
    uint32_t textColor = 0xFFFFFFFF; // default to white
    if (textColorProp != style.end() && !textColorProp->second.empty()) {
      textColor = parseCssColor(textColorProp->second);
    }

    BoxEdges pad;
    auto padIt = style.find("padding");
    if (padIt != style.end()) {
      pad = parseBoxShorthand(padIt->second);
    }
    padIt = style.find("padding-top");
    if (padIt != style.end()) {
      pad.top = parseLength(padIt->second).value;
    }
    padIt = style.find("padding-left");
    if (padIt != style.end()) {
      pad.left = parseLength(padIt->second).value;
    }

    float x = node->computedX + pad.left;
    float y = node->computedY + pad.top + fontAtlas.ascent();

    for (char c : node->content) {
      if (const GlyphInfo *g = fontAtlas.glyph(static_cast<char32_t>(c))) {
        if (g->width > 0 && g->height > 0) {
          float gx = x + g->bearingX;
          float gy = y + g->bearingY;

          float snappedX = std::floor(gx + 0.5F);
          float snappedY = std::floor(gy + 0.5F);

          batcher.addGlyphQuad(snappedX, snappedY, g->width, g->height,
                               textColor, g->u0, g->v0, g->u1, g->v1,
                               fontAtlas.texture());
        }
        x += g->advance;
      }
    }
  }

  for (auto &child : node->children) {
    collectQuads(child.get(), stylesheet, fontAtlas, batcher, &style);
  }
}

void QuadBatcher::addElementTree(ElementNode *root,
                                 const CssStylesheet &stylesheet,
                                 const FontAtlas &fontAtlas) {
  collectQuads(root, stylesheet, fontAtlas, *this);
}

} // namespace RaidenUI
