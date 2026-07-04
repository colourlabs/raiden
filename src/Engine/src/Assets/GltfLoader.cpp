#include "GltfLoader.hpp"

#include <Raiden/Assets/IAssetManager.hpp>
#include <Raiden/Logger.hpp>
#include <Raiden/Renderer/IMaterial.hpp>
#include <Raiden/Renderer/IRenderDevice.hpp>
#include <Raiden/Renderer/RenderTypes.hpp>

#include <fastgltf/base64.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string_view>
#include <vector>

namespace Raiden::Assets {

using namespace ::Raiden::Core;
using namespace ::Raiden::Renderer;

static const ::Raiden::Core::Logger s_logger("Raiden::Assets::GltfLoader");

// helpers to extract raw bytes from a fastgltf DataSource
static const std::byte *getBufferData(const fastgltf::Buffer &buffer) {
  if (const auto *arr = std::get_if<fastgltf::sources::Array>(&buffer.data)) {
    return arr->bytes.data();
  }
  if (const auto *bv = std::get_if<fastgltf::sources::ByteView>(&buffer.data)) {
    return bv->bytes.data();
  }
  {
    return nullptr;
  }
}

static const std::byte *getAccessorData(const fastgltf::Asset &asset,
                                        const fastgltf::Accessor &accessor) {
  if (!accessor.bufferViewIndex.has_value()) {
    return nullptr;
  }
  const auto &bv = asset.bufferViews[*accessor.bufferViewIndex];
  if (bv.bufferIndex >= asset.buffers.size()) {
    return nullptr;
  }
  const auto &buf = asset.buffers[bv.bufferIndex];
  const auto *base = getBufferData(buf);
  if (base == nullptr) {
    return nullptr;
  }
  return base + bv.byteOffset + accessor.byteOffset;
}

static size_t getByteStride(const fastgltf::Asset &asset,
                            const fastgltf::Accessor &acc) {
  if (acc.bufferViewIndex.has_value()) {
    const auto &bv = asset.bufferViews[*acc.bufferViewIndex];
    if (bv.byteStride.has_value()) {
      return *bv.byteStride;
}
  }
  return fastgltf::getElementByteSize(acc.type, acc.componentType);
}

static void readVertexPos(const fastgltf::Asset &asset, const std::byte *src,
                          size_t index, const fastgltf::Accessor &acc,
                          glm::vec3 &dst) {
  if (acc.componentType == fastgltf::ComponentType::Float) {
    size_t srcStride = getByteStride(asset, acc);
    std::memcpy(&dst, src + (index * srcStride), sizeof(float) * 3);
  } else {
    dst = glm::vec3(0.0F);
  }
}

static void readVertexNormal(const fastgltf::Asset &asset, const std::byte *src,
                             size_t index, const fastgltf::Accessor &acc,
                             glm::vec3 &dst) {
  if (acc.componentType == fastgltf::ComponentType::Float) {
    size_t srcStride = getByteStride(asset, acc);
    std::memcpy(&dst, src + (index * srcStride), sizeof(float) * 3);
  } else {
    dst = glm::vec3(0, 0, 1);
  }
}

static void readVertexUV(const fastgltf::Asset &asset, const std::byte *src,
                         size_t index, const fastgltf::Accessor &acc,
                         glm::vec2 &dst) {
  if (acc.componentType == fastgltf::ComponentType::Float) {
    size_t srcStride = getByteStride(asset, acc);
    std::memcpy(&dst, src + (index * srcStride), sizeof(float) * 2);
  } else {
    dst = glm::vec2(0.0F);
  }
}

static void readVertexColor(const fastgltf::Asset &asset, const std::byte *src,
                            size_t index, const fastgltf::Accessor &acc,
                            glm::vec3 &dst) {
  if (acc.componentType == fastgltf::ComponentType::Float) {
    size_t srcStride = getByteStride(asset, acc);
    size_t compCount = fastgltf::getNumComponents(acc.type);
    std::array<float, 4> tmp = {1, 1, 1, 1};
    std::memcpy(tmp.data(), src + (index * srcStride), sizeof(float) * compCount);
    dst = glm::vec3(tmp[0], tmp[1], tmp[2]);
  } else {
    dst = glm::vec3(1.0F);
  }
}

// extract embedded image bytes (sources::Array or ByteView)
// returns empty vector when the image uses an external URI
static std::vector<std::byte>
getEmbeddedImageData(const fastgltf::Image &image) {
  if (const auto *arr = std::get_if<fastgltf::sources::Array>(&image.data)) {
    return {arr->bytes.begin(), arr->bytes.end()};
  }
  if (const auto *bv = std::get_if<fastgltf::sources::ByteView>(&image.data)) {
    return {bv->bytes.begin(), bv->bytes.end()};
  }
  return {};
}

// resolve a relative texture URI against the glTF's base VFS path
static std::string resolveTextureUri(std::string_view basePath,
                                     std::string_view uri) {
  auto parent = std::filesystem::path(basePath).parent_path();
  auto resolved = parent / std::filesystem::path(uri);
  return resolved.generic_string();
}

// resolve a glTF texture index to a loadable VFS path
// returns the VFS path (external URI or mem:// for embedded)
static std::string resolveGltfTexture(const fastgltf::Asset &asset,
                                      size_t texIdx,
                                      std::string_view basePath) {
  if (texIdx >= asset.textures.size()) {
    return {};
  }
  const auto &tex = asset.textures[texIdx];
  if (!tex.imageIndex.has_value()) {
    return {};
  }

  const auto &image = asset.images[*tex.imageIndex];

  // external URI — resolve relative to glTF path
  if (const auto *uri = std::get_if<fastgltf::sources::URI>(&image.data)) {
    if (!uri->uri.isDataUri()) {
      auto vfsPath = resolveTextureUri(basePath, uri->uri.string());

      if (vfsPath.size() < 5 || vfsPath.substr(vfsPath.size() - 5) != ".ktx2") {
        s_logger.warn("glTF texture '{}' is not KTX2 - consider converting for "
                      "optimal performance (smaller size, faster load and it's "
                      "a GPU-ready format)!",
                      vfsPath);
      }

      return vfsPath;
    }
  }

  // embedded (data URI, Array, or ByteView), served through mem:// VFS
  // registered earlier in loadGltf()
  return "mem://gltf/tex/" + std::to_string(*tex.imageIndex);
}

// load a glTF PBR material into an engine material
static std::shared_ptr<IMaterial>
loadMaterial(const fastgltf::Asset &asset, const fastgltf::Material &gltfMat,
             std::string_view basePath, IRenderDevice &device,
             IAssetManager &assets) {
  MaterialDesc desc;
  desc.shader = "builtin://pbr";

  // base color
  desc.baseColorFactor = glm::make_vec4(gltfMat.pbrData.baseColorFactor.data());

  if (gltfMat.pbrData.baseColorTexture.has_value()) {
    auto path = resolveGltfTexture(
        asset, gltfMat.pbrData.baseColorTexture->textureIndex, basePath);
    if (!path.empty()) {
      desc.baseColorTexture = path;
    }
  }

  // metallic-roughness
  desc.metallicFactor = gltfMat.pbrData.metallicFactor;
  desc.roughnessFactor = gltfMat.pbrData.roughnessFactor;

  if (gltfMat.pbrData.metallicRoughnessTexture.has_value()) {
    auto path = resolveGltfTexture(
        asset, gltfMat.pbrData.metallicRoughnessTexture->textureIndex,
        basePath);
    if (!path.empty()) {
      desc.metallicRoughnessTexture = path;
    }
  }

  // normal map
  if (gltfMat.normalTexture.has_value()) {
    auto path = resolveGltfTexture(asset, gltfMat.normalTexture->textureIndex,
                                   basePath);
    if (!path.empty()) {
      desc.normalTexture = path;
    }
  }

  // emissive
  desc.emissiveFactor = glm::make_vec3(gltfMat.emissiveFactor.data());

  if (gltfMat.emissiveTexture.has_value()) {
    auto path = resolveGltfTexture(asset, gltfMat.emissiveTexture->textureIndex,
                                   basePath);
    if (!path.empty()) {
      desc.emissiveTexture = path;
    }
  }

  // occlusion
  if (gltfMat.occlusionTexture.has_value()) {
    auto path = resolveGltfTexture(
        asset, gltfMat.occlusionTexture->textureIndex, basePath);
    if (!path.empty()) {
      desc.occlusionTexture = path;
    }
    desc.occlusionStrength = gltfMat.occlusionTexture->strength;
  }

  // alpha
  switch (gltfMat.alphaMode) {
  case fastgltf::AlphaMode::Mask:
    desc.alphaMode = MaterialDesc::AlphaMode::Mask;
    break;
  case fastgltf::AlphaMode::Blend:
    desc.alphaMode = MaterialDesc::AlphaMode::Blend;
    break;
  default:
    break;
  }

  desc.alphaCutoff = gltfMat.alphaCutoff;
  desc.doubleSided = gltfMat.doubleSided;

  return assets.loadMaterial(desc);
}

// main loader
std::vector<Mesh> loadGltf(IRenderDevice &device, IAssetManager &assets,
                           const std::byte *data, size_t size,
                           std::string_view basePath) {
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
  s_logger.info("glTF loaded: {} meshes, {} materials, {} images",
                gltfAsset.meshes.size(), gltfAsset.materials.size(),
                gltfAsset.images.size());

  // register embedded images in the VFS under mem:// paths
  // so loadMaterial() -> loadTexture() -> vfs.readBytes() can find them
  for (size_t i = 0; i < gltfAsset.images.size(); ++i) {
    const auto &image = gltfAsset.images[i];
    std::vector<std::byte> imgData = getEmbeddedImageData(image);

    // also decode data URIs (e.g. data:image/png;base64,...)
    if (imgData.empty()) {
      if (const auto *uri = std::get_if<fastgltf::sources::URI>(&image.data)) {
        if (uri->uri.isDataUri()) {
          auto uriStr = uri->uri.string();
          // format: "data:[<mime>][;base64],<base64data>"
          if (auto comma = uriStr.find(','); comma != std::string_view::npos) {
            auto b64 = uriStr.substr(comma + 1);
            auto decoded = fastgltf::base64::decode(b64);
            if (!decoded.empty()) {
              auto *raw = reinterpret_cast<std::byte *>(decoded.data());
              imgData.assign(raw, raw + decoded.size());
            }
          }
        }
      }
    }

    if (!imgData.empty()) {
      auto memPath = "mem://gltf/tex/" + std::to_string(i);
      assets.registerData(memPath, std::move(imgData));
      s_logger.info("Registered embedded texture at {}", memPath);
    }
  }

  // pre-load materials (indexed by position in glTF materials array)
  // fastgltf uses std::nullopt to mean "use the default glTF material"
  // glm::vec4(0.5, 0.5, 0.5, 1.0) baseColor, 0.0 metallic, 1.0 roughness
  struct MaterialSlot {
    std::shared_ptr<IMaterial> material;
    bool needsDefault = false;
  };
  std::vector<MaterialSlot> materialSlots;
  materialSlots.reserve(gltfAsset.materials.size());

  for (size_t i = 0; i < gltfAsset.materials.size(); ++i) {
    auto mat = loadMaterial(gltfAsset, gltfAsset.materials[i], basePath, device,
                            assets);
    if (mat) {
      materialSlots.push_back({std::move(mat), false});
    } else {
      materialSlots.push_back({nullptr, true});
    }
  }

  // walk all nodes in the default scene (or all nodes if no scene)
  struct NodeStack {
    size_t nodeIndex;
    glm::mat4 parentTransform;
  };

  auto pushNode = [](std::vector<NodeStack> &s, size_t idx, glm::mat4 xf) {
    s.emplace_back(NodeStack{.nodeIndex = idx, .parentTransform = xf});
  };

  std::vector<NodeStack> stack;

  if (gltfAsset.defaultScene.has_value()) {
    for (auto nodeIdx : gltfAsset.scenes[*gltfAsset.defaultScene].nodeIndices) {
      pushNode(stack, nodeIdx, glm::mat4(1.0F));
    }
  } else {
    for (size_t i = 0; i < gltfAsset.nodes.size(); ++i) {
      pushNode(stack, i, glm::mat4(1.0F));
    }
  }

  while (!stack.empty()) {
    auto [nodeIdx, parentXform] = stack.back();
    stack.pop_back();

    const auto &node = gltfAsset.nodes[nodeIdx];

    // compute local transform
    glm::mat4 localXform(1.0F);
    if (const auto *mat = std::get_if<fastgltf::math::fmat4x4>(&node.transform)) {
      std::memcpy(&localXform, mat->data(), sizeof(float) * 16);
      localXform = glm::transpose(localXform);
    }

    glm::mat4 worldXform = parentXform * localXform;

    // handle mesh
    if (node.meshIndex.has_value()) {
      const auto &mesh = gltfAsset.meshes[*node.meshIndex];

      for (const auto &primitive : mesh.primitives) {
        if (primitive.type != fastgltf::PrimitiveType::Triangles) {
          continue;
        }

        // accessors
        const auto *attrEnd = primitive.attributes.cend();
        const auto *posAttr = primitive.findAttribute("POSITION");
        const auto *normAttr = primitive.findAttribute("NORMAL");
        const auto *uvAttr = primitive.findAttribute("TEXCOORD_0");
        const auto *colAttr = primitive.findAttribute("COLOR_0");
        bool hasPos = posAttr != attrEnd;
        bool hasNorm = normAttr != attrEnd;
        bool hasUv = uvAttr != attrEnd;
        bool hasCol = colAttr != attrEnd;

        if (!hasPos) {
          continue;
        }

        const auto &posAcc = gltfAsset.accessors[posAttr->accessorIndex];
        size_t vertexCount = posAcc.count;

        // vertex buffer
        std::vector<Vertex> vertices(vertexCount);

        {
          const auto *posData = getAccessorData(gltfAsset, posAcc);
          for (size_t i = 0; i < vertexCount; ++i) {
            readVertexPos(gltfAsset, posData, i, posAcc, vertices[i].pos);
          }
        }

        if (hasNorm) {
          const auto &normAcc = gltfAsset.accessors[normAttr->accessorIndex];
          const auto *normData = getAccessorData(gltfAsset, normAcc);
          size_t n = std::min(vertexCount, (size_t)normAcc.count);
          for (size_t i = 0; i < n; ++i) {
            readVertexNormal(gltfAsset, normData, i, normAcc,
                             vertices[i].normal);
          }
          for (size_t i = n; i < vertexCount; ++i) {
            vertices[i].normal = glm::vec3(0, 0, 1);
          }
        } else {
          for (size_t i = 0; i < vertexCount; ++i) {
            vertices[i].normal = glm::vec3(0, 0, 1);
          }
        }

        if (hasUv) {
          const auto &uvAcc = gltfAsset.accessors[uvAttr->accessorIndex];
          const auto *uvData = getAccessorData(gltfAsset, uvAcc);
          size_t n = std::min(vertexCount, (size_t)uvAcc.count);
          for (size_t i = 0; i < n; ++i) {
            readVertexUV(gltfAsset, uvData, i, uvAcc, vertices[i].uv);
          }
          for (size_t i = n; i < vertexCount; ++i) {
            vertices[i].uv = glm::vec2(0.0F);
          }
        } else {
          for (size_t i = 0; i < vertexCount; ++i) {
            auto &v = vertices[i];
            glm::vec3 n = glm::abs(v.normal);
            float sx = v.pos.x + 0.5F;
            float sy = 0.5F - v.pos.y;
            float sz = v.pos.z + 0.5F;
            if (n.x > n.y && n.x > n.z) {
              v.uv = glm::vec2(sz, sy);
            } else if (n.y > n.x && n.y > n.z) {
              v.uv = glm::vec2(sx, sz);
            } else {
              v.uv = glm::vec2(sx, sy);
            }
          }
        }

        if (hasCol) {
          const auto &colAcc = gltfAsset.accessors[colAttr->accessorIndex];
          const auto *colData = getAccessorData(gltfAsset, colAcc);
          size_t n = std::min(vertexCount, (size_t)colAcc.count);
          for (size_t i = 0; i < n; ++i) {
            readVertexColor(gltfAsset, colData, i, colAcc, vertices[i].color);
          }
          for (size_t i = n; i < vertexCount; ++i) {
            vertices[i].color = glm::vec3(1.0F);
          }
        } else {
          for (size_t i = 0; i < vertexCount; ++i) {
            vertices[i].color = glm::vec3(1.0F);
          }
        }

        auto vtxBuf =
            device.createBuffer({.size = vertices.size() * sizeof(Vertex),
                                 .usage = BufferUsage::Vertex, .access = MemoryAccess::CpuToGpu});
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
          const auto *idxData = getAccessorData(gltfAsset, idxAcc);
          indexCount = static_cast<uint32_t>(idxAcc.count);

          if (idxData != nullptr && indexCount > 0) {
            bool is32Bit =
                idxAcc.componentType == fastgltf::ComponentType::UnsignedInt;
            size_t idxSize = is32Bit ? sizeof(uint32_t) : sizeof(uint16_t);

            std::vector<std::byte> flat(idxAcc.count * idxSize);
            size_t srcStride = getByteStride(gltfAsset, idxAcc);
            for (size_t i = 0; i < idxAcc.count; ++i) {
              std::memcpy(&flat[i * idxSize], idxData + (i * srcStride), idxSize);
            }

            idxBuf = device.createBuffer(
                {.size = flat.size(), .usage = BufferUsage::Index, .access = MemoryAccess::CpuToGpu,
                 .indexType = is32Bit ? IndexType::Uint32 : IndexType::Uint16});
            if (idxBuf) {
              idxBuf->upload(flat.data(), flat.size());
            }
          }
        }

        if (!idxBuf || indexCount == 0) {
          s_logger.warn("glTF primitive has no index buffer, skipping");
          continue;
        }

        // resolve material for this primitive
        std::shared_ptr<IMaterial> material;

        if (primitive.materialIndex.has_value() &&
            *primitive.materialIndex < materialSlots.size()) {
          auto &slot = materialSlots[*primitive.materialIndex];
          if (!slot.needsDefault) {
            material = slot.material;
          }
        }

        if (!material) {
          // create a default PBR material
          material = assets.loadMaterial(MaterialDesc{});
        }

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

} // namespace Raiden::Assets
