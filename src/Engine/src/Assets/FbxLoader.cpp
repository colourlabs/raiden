#include "FbxLoader.hpp"

#include <Raiden/Assets/IAssetManager.hpp>
#include <Raiden/Logger.hpp>
#include <Raiden/Renderer/IMaterial.hpp>
#include <Raiden/Renderer/IRenderDevice.hpp>
#include <Raiden/Renderer/RenderTypes.hpp>

#include <ufbx.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <glm/glm.hpp>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Raiden::Assets {

using namespace ::Raiden::Core;
using namespace ::Raiden::Renderer;

static const ::Raiden::Core::Logger s_logger("Raiden::Assets::FbxLoader");

static std::string resolveTexturePath(ufbx_texture *tex, std::string_view basePath,
                                      IAssetManager &assets, size_t texId) {
  if (tex == nullptr) {
    return {};
  }

  if (tex->content.size > 0) {
    auto memPath = "mem://fbx/tex/" + std::to_string(texId);
    const auto *raw = static_cast<const std::byte *>(tex->content.data);
    std::vector<std::byte> imgData(raw, raw + tex->content.size);
    assets.registerData(memPath, std::move(imgData));
    s_logger.info("Registered embedded FBX texture at {}", memPath);
    return memPath;
  }

  std::string filename(tex->relative_filename.data, tex->relative_filename.length);
  if (!filename.empty()) {
    auto parent = std::filesystem::path(basePath).parent_path();
    auto resolved = parent / std::filesystem::path(filename);
    return resolved.generic_string();
  }

  std::string absPath(tex->absolute_filename.data, tex->absolute_filename.length);
  return absPath;
}

static std::shared_ptr<IMaterial>
loadMaterial(const ufbx_material *fbxMat, std::string_view basePath,
             IAssetManager &assets, size_t matId) {
  MaterialDesc desc;
  desc.shader = "builtin://pbr";

  if (fbxMat == nullptr) {
    return assets.loadMaterial(desc);
  }

  const auto &pbr = fbxMat->pbr;

  const auto &fbx = fbxMat->fbx;

  if (pbr.base_color.has_value) {
    desc.baseColorFactor = glm::vec4(
        static_cast<float>(pbr.base_color.value_vec3.x),
        static_cast<float>(pbr.base_color.value_vec3.y),
        static_cast<float>(pbr.base_color.value_vec3.z), 1.0F);
  } else if (fbx.diffuse_color.has_value) {
    desc.baseColorFactor = glm::vec4(
        static_cast<float>(fbx.diffuse_color.value_vec3.x),
        static_cast<float>(fbx.diffuse_color.value_vec3.y),
        static_cast<float>(fbx.diffuse_color.value_vec3.z), 1.0F);
  }
  if (pbr.base_factor.has_value) {
    desc.baseColorFactor.w = static_cast<float>(pbr.base_factor.value_real);
  } else if (fbx.diffuse_factor.has_value) {
    desc.baseColorFactor.w = static_cast<float>(fbx.diffuse_factor.value_real);
  }

  auto baseTexPath = resolveTexturePath(pbr.base_color.texture, basePath, assets, matId * 10);
  if (baseTexPath.empty()) {
    baseTexPath = resolveTexturePath(fbx.diffuse_color.texture, basePath, assets, matId * 10);
  }
  if (!baseTexPath.empty()) {
    desc.baseColorTexture = baseTexPath;
  }

  desc.metallicFactor = pbr.metalness.has_value
                            ? static_cast<float>(pbr.metalness.value_real)
                            : 0.0F;
  desc.roughnessFactor = pbr.roughness.has_value
                             ? static_cast<float>(pbr.roughness.value_real)
                             : 1.0F;

  auto mrTexPath = resolveTexturePath(pbr.metalness.texture, basePath, assets, (matId * 10) + 1);
  if (!mrTexPath.empty()) {
    desc.metallicRoughnessTexture = mrTexPath;
  }

  auto normalTexPath = resolveTexturePath(pbr.normal_map.texture, basePath, assets, (matId * 10) + 2);
  if (normalTexPath.empty()) {
    normalTexPath = resolveTexturePath(fbx.normal_map.texture, basePath, assets, (matId * 10) + 2);
  }
  if (!normalTexPath.empty()) {
    desc.normalTexture = normalTexPath;
  }

  if (pbr.emission_color.has_value) {
    desc.emissiveFactor = glm::vec3(
        static_cast<float>(pbr.emission_color.value_vec3.x),
        static_cast<float>(pbr.emission_color.value_vec3.y),
        static_cast<float>(pbr.emission_color.value_vec3.z));
  }
  if (pbr.emission_factor.has_value) {
    desc.emissiveFactor *= static_cast<float>(pbr.emission_factor.value_real);
  }

  auto emissiveTexPath = resolveTexturePath(pbr.emission_color.texture, basePath, assets, (matId * 10) + 3);
  if (emissiveTexPath.empty()) {
    emissiveTexPath = resolveTexturePath(fbx.emission_color.texture, basePath, assets, (matId * 10) + 3);
  }
  if (!emissiveTexPath.empty()) {
    desc.emissiveTexture = emissiveTexPath;
  }

  auto aoTexPath = resolveTexturePath(pbr.ambient_occlusion.texture, basePath, assets, (matId * 10) + 4);
  if (!aoTexPath.empty()) {
    desc.occlusionTexture = aoTexPath;
  }
  if (pbr.ambient_occlusion.has_value) {
    desc.occlusionStrength = static_cast<float>(pbr.ambient_occlusion.value_real);
  }

  desc.doubleSided = fbxMat->features.double_sided.enabled;

  return assets.loadMaterial(desc);
}

std::vector<Mesh> loadFbx(IRenderDevice &device, IAssetManager &assets,
                           const std::byte *data, size_t size,
                           std::string_view basePath) {
  std::vector<Mesh> result;

  ufbx_load_opts opts = {};
  opts.target_unit_meters = 1.0F;
  opts.ignore_animation = true;
  opts.generate_missing_normals = true;
  opts.use_blender_pbr_material = true;

  ufbx_error error = {};
  ufbx_scene *scene = ufbx_load_memory(data, size, &opts, &error);
  if (scene == nullptr) {
    s_logger.error("Failed to load FBX: {} (error type {})",
                   error.description.data,
                   static_cast<int>(error.type));
    return result;
  }

  s_logger.info("FBX loaded: {} nodes, {} meshes, {} materials",
                scene->nodes.count, scene->meshes.count, scene->materials.count);

  // pre-load materials
  struct MaterialSlot {
    std::shared_ptr<IMaterial> material;
    bool needsDefault = false;
  };
  std::vector<MaterialSlot> materialSlots;
  materialSlots.reserve(scene->materials.count);

  for (size_t i = 0; i < scene->materials.count; ++i) {
    auto mat = loadMaterial(scene->materials.data[i], basePath, assets, i);
    if (mat) {
      materialSlots.push_back({std::move(mat), false});
    } else {
      materialSlots.push_back({nullptr, true});
    }
  }

  // iterative DFS over all nodes
  std::vector<ufbx_node *> stack;
  for (size_t i = 0; i < scene->nodes.count; ++i) {
    ufbx_node *node = scene->nodes.data[i];
    if (!node->is_root) {
      stack.push_back(node);
    }
  }

  while (!stack.empty()) {
    ufbx_node *node = stack.back();
    stack.pop_back();

    if (node->mesh == nullptr) {
      for (size_t ci = 0; ci < node->children.count; ++ci) {
        stack.push_back(node->children.data[ci]);
      }
      continue;
    }

    const ufbx_mesh *mesh = node->mesh;
    size_t numFaces = mesh->num_faces;
    size_t indexCount = mesh->num_indices;

    if (indexCount == 0 || numFaces == 0) {
      for (size_t ci = 0; ci < node->children.count; ++ci) {
        stack.push_back(node->children.data[ci]);
      }
      continue;
    }

    // build vertices (one per index, matching glTF loader convention)
    std::vector<Vertex> vertices(indexCount);

    for (size_t i = 0; i < indexCount; ++i) {
      Vertex &v = vertices[i];

      size_t vi = mesh->vertex_position.indices.data[i];
      auto &pos = mesh->vertex_position.values.data[vi];
      v.pos = glm::vec3(static_cast<float>(pos.x), static_cast<float>(pos.y),
                        static_cast<float>(pos.z));

      if (mesh->vertex_normal.exists) {
        size_t ni = mesh->vertex_normal.indices.data[i];
        auto &nrm = mesh->vertex_normal.values.data[ni];
        v.normal = glm::vec3(static_cast<float>(nrm.x), static_cast<float>(nrm.y),
                             static_cast<float>(nrm.z));
      } else {
        v.normal = glm::vec3(0.0F, 0.0F, 1.0F);
      }

      if (mesh->vertex_color.exists) {
        size_t ci = mesh->vertex_color.indices.data[i];
        auto &col = mesh->vertex_color.values.data[ci];
        v.color = glm::vec3(static_cast<float>(col.x), static_cast<float>(col.y),
                            static_cast<float>(col.z));
      } else {
        v.color = glm::vec3(1.0F);
      }

      if (mesh->vertex_uv.exists) {
        size_t ui = mesh->vertex_uv.indices.data[i];
        auto &uv = mesh->vertex_uv.values.data[ui];
        v.uv = glm::vec2(static_cast<float>(uv.x), static_cast<float>(uv.y));
      } else {
        // procedural UV from position (matching glTF loader)
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

    auto vtxBuf =
        device.createBuffer({.size = vertices.size() * sizeof(Vertex),
                             .usage = BufferUsage::Vertex, .access = MemoryAccess::CpuToGpu});
    if (!vtxBuf) {
      s_logger.error("Failed to create vertex buffer for FBX mesh");
      for (size_t ci = 0; ci < node->children.count; ++ci) {
        stack.push_back(node->children.data[ci]);
      }
      continue;
    }
    vtxBuf->upload(vertices.data(), vertices.size() * sizeof(Vertex));

    // share the vertex buffer across all material sub-meshes
    std::shared_ptr<IBuffer> sharedVtxBuf(std::move(vtxBuf));

    // group faces by material
    std::unordered_map<uint32_t, std::vector<uint32_t>> matFaces;
    for (size_t f = 0; f < numFaces; ++f) {
      uint32_t matIdx = (f < mesh->face_material.count) ? mesh->face_material.data[f] : 0;
      matFaces[matIdx].push_back(static_cast<uint32_t>(f));
    }

    bool use32Bit = indexCount > 65535;
    size_t idxSize = use32Bit ? sizeof(uint32_t) : sizeof(uint16_t);
    IndexType idxType = use32Bit ? IndexType::Uint32 : IndexType::Uint16;

    for (auto &[matIdx, faces] : matFaces) {
      std::vector<uint32_t> matIndices;
      matIndices.reserve(faces.size() * 6);

      for (uint32_t fi : faces) {
        ufbx_face face = mesh->faces.data[fi];
        if (face.num_indices < 3) {
          continue;
        }
        for (size_t t = 2; t < face.num_indices; ++t) {
          matIndices.push_back(face.index_begin);
          matIndices.push_back(face.index_begin + t - 1);
          matIndices.push_back(face.index_begin + t);
        }
      }

      if (matIndices.empty()) {
        continue;
      }

      std::vector<std::byte> flat(matIndices.size() * idxSize);
      for (size_t i = 0; i < matIndices.size(); ++i) {
        if (use32Bit) {
          auto val = static_cast<uint32_t>(matIndices[i]);
          std::memcpy(&flat[i * sizeof(uint32_t)], &val, sizeof(uint32_t));
        } else {
          auto val = static_cast<uint16_t>(matIndices[i]);
          std::memcpy(&flat[i * sizeof(uint16_t)], &val, sizeof(uint16_t));
        }
      }

      auto idxBuf = device.createBuffer(
          {.size = flat.size(), .usage = BufferUsage::Index, .access = MemoryAccess::CpuToGpu,
           .indexType = idxType});
      if (!idxBuf) {
        continue;
      }
      idxBuf->upload(flat.data(), flat.size());

      // resolve material
      std::shared_ptr<IMaterial> material;
      if (matIdx < node->materials.count) {
        ufbx_material *fbxMat = node->materials.data[matIdx];
        for (size_t si = 0; si < scene->materials.count; ++si) {
          if (scene->materials.data[si] == fbxMat && si < materialSlots.size()) {
            auto &slot = materialSlots[si];
            if (!slot.needsDefault) {
              material = slot.material;
            }
            break;
          }
        }
      }

      if (!material) {
        material = assets.loadMaterial(MaterialDesc{});
      }

      result.push_back(Mesh{
          .vertexBuffer = sharedVtxBuf,
          .indexBuffer = std::move(idxBuf),
          .indexCount = static_cast<uint32_t>(matIndices.size()),
          .indexOffset = 0,
          .material = std::move(material),
      });
    }

    for (size_t ci = 0; ci < node->children.count; ++ci) {
      stack.push_back(node->children.data[ci]);
    }
  }

  s_logger.info("FBX loaded: {} engine meshes produced", result.size());
  ufbx_free_scene(scene);
  return result;
}

} // namespace Raiden::Assets
