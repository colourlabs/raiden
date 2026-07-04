#include <RaidenUI/Render/QuadBatcher.hpp>

#include <Raiden/Renderer/IBuffer.hpp>
#include <Raiden/Renderer/IPipeline.hpp>
#include <Raiden/Renderer/RenderTypes.hpp>

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>

namespace RaidenUI {

using namespace Raiden::Renderer;

// color parsing

static uint32_t packRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
  return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
         (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(r);
}

static uint32_t parseHexColor(std::string_view sv) {
  if (!sv.empty() && sv[0] == '#') {
    sv.remove_prefix(1);
  }

  auto hexVal = [](char c) -> uint8_t {
    if (c >= '0' && c <= '9') {
      return static_cast<uint8_t>(c - '0');
    }

    if (c >= 'a' && c <= 'f') {
      return static_cast<uint8_t>(c - 'a' + 10);
    }

    if (c >= 'A' && c <= 'F') {
      return static_cast<uint8_t>(c - 'A' + 10);
    }

    return 0;
  };

  if (sv.size() == 3) {
    auto r = static_cast<uint8_t>(hexVal(sv[0]) * 17);
    auto g = static_cast<uint8_t>(hexVal(sv[1]) * 17);
    auto b = static_cast<uint8_t>(hexVal(sv[2]) * 17);

    return packRGBA(r, g, b);
  }

  if (sv.size() >= 6) {
    auto r = static_cast<uint8_t>((hexVal(sv[0]) * 16) + hexVal(sv[1]));
    auto g = static_cast<uint8_t>((hexVal(sv[2]) * 16) + hexVal(sv[3]));
    auto b = static_cast<uint8_t>((hexVal(sv[4]) * 16) + hexVal(sv[5]));

    return packRGBA(r, g, b);
  }

  return 0xFFFFFFFF;
}

uint32_t parseCssColor(const std::string &str) {
  std::string_view sv(str);

  while (!sv.empty() && sv.front() == ' ') {
    sv.remove_prefix(1);
  }

  while (!sv.empty() && sv.back() == ' ') {
    sv.remove_suffix(1);
  }

  if (sv.empty()) {
    return 0;
  }

  if (sv[0] == '#') {
    return parseHexColor(sv);
  }

  if (sv == "transparent") {
    return 0;
  }

  if (sv == "red") {
    return packRGBA(255, 0, 0);
  }

  if (sv == "green") {
    return packRGBA(0, 128, 0);
  }

  if (sv == "blue") {
    return packRGBA(0, 0, 255);
  }

  if (sv == "white") {
    return packRGBA(255, 255, 255);
  }

  if (sv == "black") {
    return packRGBA(0, 0, 0);
  }

  if (sv == "gray" || sv == "grey") {
    return packRGBA(128, 128, 128);
  }

  if (sv == "yellow") {
    return packRGBA(255, 255, 0);
  }

  if (sv == "orange") {
    return packRGBA(255, 165, 0);
  }

  if (sv == "purple") {
    return packRGBA(128, 0, 128);
  }

  if (sv == "pink") {
    return packRGBA(255, 192, 203);
  }

  if (sv == "cyan") {
    return packRGBA(0, 255, 255);
  }

  if (sv == "magenta") {
    return packRGBA(255, 0, 255);
  }

  if (sv == "silver") {
    return packRGBA(192, 192, 192);
  }

  if (sv == "maroon") {
    return packRGBA(128, 0, 0);
  }

  if (sv == "olive") {
    return packRGBA(128, 128, 0);
  }

  if (sv == "navy") {
    return packRGBA(0, 0, 128);
  }

  if (sv == "teal") {
    return packRGBA(0, 128, 128);
  }

  if (sv == "aqua") {
    return packRGBA(0, 255, 255);
  }

  if (sv == "fuchsia") {
    return packRGBA(255, 0, 255);
  }

  if (sv == "lime") {
    return packRGBA(0, 255, 0);
  }

  return 0;
}

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
  quad.texture = texture;

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

  // sort quads by texture for batching (null first, then by pointer)
  std::ranges::sort(m_quads, [](const UIQuad &a, const UIQuad &b) {
    return a.texture < b.texture;
  });

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
                         QuadBatcher &batcher) {
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

  for (auto &child : node->children) {
    collectQuads(child.get(), stylesheet, batcher);
  }
}

void QuadBatcher::addElementTree(ElementNode *root,
                                 const CssStylesheet &stylesheet) {
  collectQuads(root, stylesheet, *this);
}

} // namespace RaidenUI
