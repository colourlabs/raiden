#include "GltfLoader.hpp"

#include <RaidenEngineCore/Logger.hpp>
#include <RaidenEngineCore/Renderer/IRenderDevice.hpp>
#include <RaidenEngineCore/Renderer/RenderTypes.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <glm/glm.hpp>
#include <vector>

namespace Raiden::Core {

static const Logger s_logger("Raiden::Core::GltfLoader");

// helpers to extract raw bytes from a fastgltf DataSource
static const std::byte *getBufferData(const fastgltf::Buffer &buffer) {
  // GLB loads into sources::Array by default
  if (auto *arr = std::get_if<fastgltf::sources::Array>(&buffer.data)) {
    return arr->bytes.data();
  }
  if (auto *bv = std::get_if<fastgltf::sources::ByteView>(&buffer.data)) {
    return bv->bytes.data();
  }
  return nullptr;
}

static const std::byte *getAccessorData(const fastgltf::Asset &asset,
                                        const fastgltf::Accessor &accessor) {
  if (!accessor.bufferViewIndex.has_value())
    return nullptr;
  const auto &bv = asset.bufferViews[*accessor.bufferViewIndex];
  if (bv.bufferIndex >= asset.buffers.size())
    return nullptr;
  const auto &buf = asset.buffers[bv.bufferIndex];
  auto *base = getBufferData(buf);
  if (!base)
    return nullptr;
  return base + bv.byteOffset + accessor.byteOffset;
}

// get effective byte stride for an accessor, accounting for buffer view stride
static size_t getByteStride(const fastgltf::Asset &asset,
                            const fastgltf::Accessor &acc) {
  if (acc.bufferViewIndex.has_value()) {
    auto &bv = asset.bufferViews[*acc.bufferViewIndex];
    if (bv.byteStride.has_value())
      return *bv.byteStride;
  }
  return fastgltf::getElementByteSize(acc.type, acc.componentType);
}

// copy glTF attribute data into a specific vertex
static void readVertexPos(const fastgltf::Asset &asset, const std::byte *src,
                          size_t index, const fastgltf::Accessor &acc,
                          glm::vec3 &dst) {
  if (acc.componentType == fastgltf::ComponentType::Float) {
    size_t srcStride = getByteStride(asset, acc);
    std::memcpy(&dst, src + index * srcStride, sizeof(float) * 3);
  } else {
    dst = glm::vec3(0.0f);
  }
}

static void readVertexNormal(const fastgltf::Asset &asset, const std::byte *src,
                             size_t index, const fastgltf::Accessor &acc,
                             glm::vec3 &dst) {
  if (acc.componentType == fastgltf::ComponentType::Float) {
    size_t srcStride = getByteStride(asset, acc);
    std::memcpy(&dst, src + index * srcStride, sizeof(float) * 3);
  } else {
    dst = glm::vec3(0, 0, 1);
  }
}

static void readVertexUV(const fastgltf::Asset &asset, const std::byte *src,
                         size_t index, const fastgltf::Accessor &acc,
                         glm::vec2 &dst) {
  if (acc.componentType == fastgltf::ComponentType::Float) {
    size_t srcStride = getByteStride(asset, acc);
    std::memcpy(&dst, src + index * srcStride, sizeof(float) * 2);
  } else {
    dst = glm::vec2(0.0f);
  }
}

static void readVertexColor(const fastgltf::Asset &asset, const std::byte *src,
                            size_t index, const fastgltf::Accessor &acc,
                            glm::vec3 &dst) {
  if (acc.componentType == fastgltf::ComponentType::Float) {
    size_t srcStride = getByteStride(asset, acc);
    size_t compCount = fastgltf::getNumComponents(acc.type); // 3 or 4
    float tmp[4] = {1, 1, 1, 1};
    std::memcpy(tmp, src + index * srcStride, sizeof(float) * compCount);
    dst = glm::vec3(tmp[0], tmp[1], tmp[2]);
  } else {
    dst = glm::vec3(1.0f);
  }
}

// main loader
std::vector<Mesh> loadGltf(IRenderDevice &device, const std::byte *data,
                           size_t size) {
  std::vector<Mesh> result;

  // wrap the raw data for fastgltf
  auto expBuffer = fastgltf::GltfDataBuffer::FromBytes(data, size);
  if (!expBuffer) {
    s_logger.error("Failed to create GltfDataBuffer");
    return result;
  }
  auto &gltfBuf = expBuffer.get();

  fastgltf::Parser parser;
  auto asset = parser.loadGltfBinary(gltfBuf, std::filesystem::path(),
                                     fastgltf::Options::None,
                                     fastgltf::Category::OnlyRenderable);
  if (!asset) {
    s_logger.error("Failed to parse glTF: {}",
                   fastgltf::getErrorName(asset.error()));
    return result;
  }

  auto &gltfAsset = asset.get();
  s_logger.info("glTF loaded: {} meshes, {} materials", gltfAsset.meshes.size(),
                gltfAsset.materials.size());

  // walk all nodes in the default scene (or all nodes if no scene)
  struct NodeStack {
    size_t nodeIndex;
    glm::mat4 parentTransform;
  };

  auto pushNode = [](std::vector<NodeStack> &s, size_t idx, glm::mat4 xf) {
    s.emplace_back(NodeStack{idx, xf});
  };

  std::vector<NodeStack> stack;

  if (gltfAsset.defaultScene.has_value()) {
    for (auto nodeIdx : gltfAsset.scenes[*gltfAsset.defaultScene].nodeIndices) {
      pushNode(stack, nodeIdx, glm::mat4(1.0f));
    }
  } else {
    for (size_t i = 0; i < gltfAsset.nodes.size(); ++i) {
      pushNode(stack, i, glm::mat4(1.0f));
    }
  }

  while (!stack.empty()) {
    auto [nodeIdx, parentXform] = stack.back();
    stack.pop_back();

    const auto &node = gltfAsset.nodes[nodeIdx];

    // compute local transform
    glm::mat4 localXform(1.0f);
    if (auto *mat = std::get_if<fastgltf::math::fmat4x4>(&node.transform)) {
      std::memcpy(&localXform, mat->data(), sizeof(float) * 16);
      // glTF matrices are column-major, glm is column-major, direct copy works
      localXform = glm::transpose(localXform);
    }

    glm::mat4 worldXform = parentXform * localXform;

    // handle mesh
    if (node.meshIndex.has_value()) {
      const auto &mesh = gltfAsset.meshes[*node.meshIndex];

      for (const auto &primitive : mesh.primitives) {
        if (primitive.type != fastgltf::PrimitiveType::Triangles)
          continue;

        // accessors
        auto attrEnd = primitive.attributes.cend();
        auto *posAttr = primitive.findAttribute("POSITION");
        auto *normAttr = primitive.findAttribute("NORMAL");
        auto *uvAttr = primitive.findAttribute("TEXCOORD_0");
        auto *colAttr = primitive.findAttribute("COLOR_0");
        bool hasPos = posAttr != attrEnd;
        bool hasNorm = normAttr != attrEnd;
        bool hasUv = uvAttr != attrEnd;
        bool hasCol = colAttr != attrEnd;

        if (hasUv) {
          s_logger.info("Primitive has TEXCOORD_0, {} uvs",
                        gltfAsset.accessors[uvAttr->accessorIndex].count);
        } else {
          s_logger.warn("Primitive has no TEXCOORD_0 - using fallback UVs");
        }

        if (!hasPos)
          continue; // position is required

        const auto &posAcc = gltfAsset.accessors[posAttr->accessorIndex];
        size_t vertexCount = posAcc.count;

        // vertex buffer
        std::vector<Vertex> vertices(vertexCount);

        {
          auto *posData = getAccessorData(gltfAsset, posAcc);
          for (size_t i = 0; i < vertexCount; ++i)
            readVertexPos(gltfAsset, posData, i, posAcc, vertices[i].pos);
        }

        if (hasNorm) {
          const auto &normAcc = gltfAsset.accessors[normAttr->accessorIndex];
          auto *normData = getAccessorData(gltfAsset, normAcc);
          size_t n = std::min(vertexCount, (size_t)normAcc.count);
          for (size_t i = 0; i < n; ++i)
            readVertexNormal(gltfAsset, normData, i, normAcc,
                             vertices[i].normal);
          for (size_t i = n; i < vertexCount; ++i)
            vertices[i].normal = glm::vec3(0, 0, 1);
        } else {
          for (size_t i = 0; i < vertexCount; ++i)
            vertices[i].normal = glm::vec3(0, 0, 1);
        }

        if (hasUv) {
          const auto &uvAcc = gltfAsset.accessors[uvAttr->accessorIndex];
          auto *uvData = getAccessorData(gltfAsset, uvAcc);
          size_t n = std::min(vertexCount, (size_t)uvAcc.count);
          for (size_t i = 0; i < n; ++i)
            readVertexUV(gltfAsset, uvData, i, uvAcc, vertices[i].uv);
          for (size_t i = n; i < vertexCount; ++i)
            vertices[i].uv = glm::vec2(0.0f);
        } else {
          for (size_t i = 0; i < vertexCount; ++i) {
            auto &v = vertices[i];
            glm::vec3 n = glm::abs(v.normal);
            float sx = v.pos.x * 0.5f + 0.5f;
            float sy = v.pos.y * 0.5f + 0.5f;
            float sz = v.pos.z * 0.5f + 0.5f;
            if (n.x > n.y && n.x > n.z)
              v.uv = glm::vec2(sz, sy);
            else if (n.y > n.x && n.y > n.z)
              v.uv = glm::vec2(sx, sz);
            else
              v.uv = glm::vec2(sx, sy);
          }
        }

        if (hasCol) {
          const auto &colAcc = gltfAsset.accessors[colAttr->accessorIndex];
          auto *colData = getAccessorData(gltfAsset, colAcc);
          size_t n = std::min(vertexCount, (size_t)colAcc.count);
          for (size_t i = 0; i < n; ++i)
            readVertexColor(gltfAsset, colData, i, colAcc, vertices[i].color);
          for (size_t i = n; i < vertexCount; ++i)
            vertices[i].color = glm::vec3(1.0f);
        } else {
          for (size_t i = 0; i < vertexCount; ++i)
            vertices[i].color = glm::vec3(1.0f);
        }

        auto vtxBuf =
            device.createBuffer({vertices.size() * sizeof(Vertex),
                                 BufferUsage::Vertex, MemoryAccess::CpuToGpu});
        if (!vtxBuf) {
          s_logger.error("Failed to create vertex buffer for glTF primitive");
          continue;
        }
        vtxBuf->upload(vertices.data(), vertices.size() * sizeof(Vertex));

        // index buffer
        uint32_t indexCount = 0;
        std::shared_ptr<IBuffer> idxBuf;

        if (primitive.indicesAccessor.has_value()) {
          const auto &idxAcc = gltfAsset.accessors[*primitive.indicesAccessor];
          auto *idxData = getAccessorData(gltfAsset, idxAcc);
          indexCount = static_cast<uint32_t>(idxAcc.count);

          if (idxData && indexCount > 0) {
            bool is32Bit =
                idxAcc.componentType == fastgltf::ComponentType::UnsignedInt;
            size_t idxSize = is32Bit ? sizeof(uint32_t) : sizeof(uint16_t);

            // copy indices accounting for buffer view stride
            std::vector<std::byte> flat(idxAcc.count * idxSize);
            size_t srcStride = getByteStride(gltfAsset, idxAcc);
            for (size_t i = 0; i < idxAcc.count; ++i) {
              std::memcpy(&flat[i * idxSize], idxData + i * srcStride, idxSize);
            }

            idxBuf = device.createBuffer(
                {flat.size(), BufferUsage::Index, MemoryAccess::CpuToGpu});
            if (idxBuf) {
              idxBuf->upload(flat.data(), flat.size());
            }
          }
        }

        // if there's no index buffer, skip (we only support indexed draw)
        if (!idxBuf || indexCount == 0) {
          s_logger.warn("glTF primitive has no index buffer, skipping");
          continue;
        }

        // material
        std::shared_ptr<IMaterial> material;

        // store the world transform for the caller to use
        // (the Mesh struct doesn't carry a transform, so it's discarded here)
        result.push_back(Mesh{
            .vertexBuffer = std::move(vtxBuf),
            .indexBuffer = std::move(idxBuf),
            .indexCount = indexCount,
            .indexOffset = 0,
            .material = std::move(material),
        });
      }
    }

    // push children
    for (auto childIdx : node.children) {
      pushNode(stack, childIdx, worldXform);
    }
  }

  s_logger.info("glTF loaded: {} engine meshes produced", result.size());
  return result;
}

} // namespace Raiden::Core
