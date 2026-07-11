#include <RaidenUI/CSS/Properties.hpp>
#include <RaidenUI/Render/FontAtlas.hpp>
#include <RaidenUI/Render/QuadBatcher.hpp>

#include <Raiden/Renderer/IBuffer.hpp>
#include <Raiden/Renderer/IPipeline.hpp>
#include <Raiden/Renderer/RenderTypes.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <numbers>
#include <string>

constexpr float pi = std::numbers::pi_v<float>;

namespace RaidenUI {

using namespace ::Raiden::Renderer;

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

void QuadBatcher::addRoundedRect(float x, float y, float w, float h,
                                  float radius, uint32_t color) {
  if (radius <= 0 || w <= 0 || h <= 0) {
    addQuad(x, y, w, h, color);
    return;
  }

  float maxR = std::min(w, h) * 0.5F;
  radius = std::min(radius, maxR);

  // center rectangle
  addQuad(x + radius, y + radius, w - (radius * 2), h - (radius * 2), color);

  // edge rectangles
  addQuad(x + radius, y, w - (radius * 2), radius, color);
  addQuad(x + radius, y + h - radius, w - (radius * 2), radius, color);
  addQuad(x, y + radius, radius, h - (radius * 2), color);
  addQuad(x + w - radius, y + radius, radius, h - (radius * 2), color);

  // corner triangle fans (4 segments each)
  auto addCorner = [&](float cx, float cy, float startRad, float endRad) {
    int segs = 4;
    for (int i = 0; i < segs; ++i) {
      float t0 = startRad + ((endRad - startRad) * static_cast<float>(i) /
                                static_cast<float>(segs));
      float t1 = startRad + ((endRad - startRad) * static_cast<float>(i + 1) /
                                static_cast<float>(segs));
      float x1 = cx + (radius * std::cos(t0));
      float y1 = cy + (radius * std::sin(t0));
      float x2 = cx + (radius * std::cos(t1));
      float y2 = cy + (radius * std::sin(t1));

      UIQuad quad;
      quad.verts[0] = {.x = cx, .y = cy, .u = 0, .v = 0, .color = color};
      quad.verts[1] = {.x = x1, .y = y1, .u = 0, .v = 0, .color = color};
      quad.verts[2] = {.x = x2, .y = y2, .u = 0, .v = 0, .color = color};
      quad.verts[3] = {.x = x2, .y = y2, .u = 0, .v = 0, .color = color};
      quad.texture = m_whiteTexture.get();
      m_bgQuads.push_back(quad);
    }
  };

  addCorner(x + radius, y + radius, pi,
            static_cast<float>(pi * 1.5)); // TL
  addCorner(x + w - radius, y + radius, static_cast<float>(pi * 1.5),
            static_cast<float>(pi * 2.0)); // TR
  addCorner(x + w - radius, y + h - radius, 0.0F,
            static_cast<float>(pi * 0.5)); // BR
  addCorner(x + radius, y + h - radius, static_cast<float>(pi * 0.5),
            static_cast<float>(pi)); // BL
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
                         FontFace &fontFace, QuadBatcher &batcher,
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
      float borderRadius = 0;
      auto brIt = style.find("border-radius");
      if (brIt != style.end() && !brIt->second.empty()) {
        borderRadius = parseLength(brIt->second).value;
      }
      if (borderRadius > 0) {
        batcher.addRoundedRect(node->computedX, node->computedY,
                               node->computedWidth, node->computedHeight,
                               borderRadius, color);
      } else {
        batcher.addQuad(node->computedX, node->computedY, node->computedWidth,
                        node->computedHeight, color);
      }
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
    uint32_t textColor = 0xFFFFFFFF;
    if (textColorProp != style.end() && !textColorProp->second.empty()) {
      textColor = parseCssColor(textColorProp->second);
    }

    float fontSize = 13.0F;
    auto fsIt = style.find("font-size");
    if (fsIt != style.end() && !fsIt->second.empty()) {
      fontSize = parseLength(fsIt->second).value;
    }
    const FontAtlas &fontAtlas = fontFace.getAtlas(fontSize);

    // text-transform
    std::string displayText = node->content;
    auto ttIt = style.find("text-transform");
    if (ttIt != style.end() && ttIt->second == "uppercase") {
      for (auto &ch : displayText) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
      }
    }

    // letter-spacing
    float letterSpacing = 0;
    auto lsIt = style.find("letter-spacing");
    if (lsIt != style.end() && !lsIt->second.empty()) {
      letterSpacing = parseLength(lsIt->second).value;
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
    padIt = style.find("padding-bottom");
    if (padIt != style.end()) {
      pad.bottom = parseLength(padIt->second).value;
    }
    padIt = style.find("padding-left");
    if (padIt != style.end()) {
      pad.left = parseLength(padIt->second).value;
    }
    padIt = style.find("padding-right");
    if (padIt != style.end()) {
      pad.right = parseLength(padIt->second).value;
    }

    float contentW = std::max(0.0F, node->computedWidth - pad.left - pad.right);
    float contentH = std::max(0.0F, node->computedHeight - pad.top - pad.bottom);

    // measure text width (including letter-spacing)
    float textW = 0;
    for (size_t i = 0; i < displayText.size(); ++i) {
      if (const GlyphInfo *g =
              fontAtlas.glyph(static_cast<char32_t>(displayText[i]))) {
        textW += g->advance;
        if (i + 1 < displayText.size()) {
          textW += letterSpacing;
        }
      }
    }
    float textH = fontAtlas.lineHeight();

    int justifyContent = 0; // FlexStart
    auto jcIt = style.find("justify-content");
    if (jcIt != style.end()) {
      if (jcIt->second == "center") {
        justifyContent = 1;
      } else if (jcIt->second == "flex-end") {
        justifyContent = 2;
      }
    }

    int alignItems = -1; // auto
    auto aiIt = style.find("align-items");
    if (aiIt != style.end()) {
      if (aiIt->second == "center") {
        alignItems = 2;
      } else if (aiIt->second == "flex-end") {
        alignItems = 1;
      } else if (aiIt->second == "stretch") {
        alignItems = 3;
      } else if (aiIt->second == "flex-start") {
        alignItems = 0;
      }
    }
    // align-self overrides
    auto asIt = style.find("align-self");
    if (asIt != style.end()) {
      if (asIt->second == "center") {
        alignItems = 2;
      } else if (asIt->second == "flex-end") {
        alignItems = 1;
      } else if (asIt->second == "stretch") {
        alignItems = 3;
      } else if (asIt->second == "flex-start") {
        alignItems = 0;
      }
    }
    if (alignItems < 0) {
      alignItems = 3; // default stretch
    }

    float textX = pad.left;
    if (justifyContent == 1) {
      textX = pad.left + (contentW - textW) * 0.5F;
    } else if (justifyContent == 2) {
      textX = pad.left + (contentW - textW);
    }

    float baseY = pad.top;
    if (alignItems == 2) {
      baseY = pad.top + (contentH - textH) * 0.5F;
    } else if (alignItems == 1) {
      baseY = pad.top + (contentH - textH);
    }

    float x = node->computedX + textX;
    float y = node->computedY + baseY + fontAtlas.ascent();

    for (char i : displayText) {
      if (const GlyphInfo *g =
              fontAtlas.glyph(static_cast<char32_t>(i))) {
        if (g->width > 0 && g->height > 0) {
          float gx = x + g->bearingX;
          float gy = y + g->bearingY;

          float snappedX = std::floor(gx + 0.5F);
          float snappedY = std::floor(gy + 0.5F);

          batcher.addGlyphQuad(snappedX, snappedY, g->width, g->height,
                               textColor, g->u0, g->v0, g->u1, g->v1,
                               fontAtlas.texture());
        }
        x += g->advance + letterSpacing;
      }
    }
  }

  for (auto &child : node->children) {
    collectQuads(child.get(), stylesheet, fontFace, batcher, &style);
  }
}

void QuadBatcher::addElementTree(ElementNode *root,
                                 const CssStylesheet &stylesheet,
                                 FontFace &fontFace) {
  collectQuads(root, stylesheet, fontFace, *this);
}

} // namespace RaidenUI
