#include <Raiden/Scene/SceneSerializer.hpp>

#include <Raiden/ECS/Camera.hpp>
#include <Raiden/ECS/Collider.hpp>
#include <Raiden/ECS/MeshRenderer.hpp>
#include <Raiden/ECS/Name.hpp>
#include <Raiden/ECS/Rigidbody.hpp>
#include <Raiden/ECS/Transform.hpp>
#include <Raiden/ECS/World.hpp>
#include <Raiden/Logger.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <nlohmann/json.hpp>

#include <cstring>
#include <unordered_map>

static const Raiden::Core::Logger s_logger("SceneSerializer");

using json = nlohmann::json;

namespace {

glm::vec3 vec3FromJson(const json &j) {
  return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

glm::quat quatFromJson(const json &j) {
  return {j[3].get<float>(), j[0].get<float>(), j[1].get<float>(),
          j[2].get<float>()};
}

json vec3ToJson(const glm::vec3 &v) { return {v.x, v.y, v.z}; }

json quatToJson(const glm::quat &q) { return {q.x, q.y, q.z, q.w}; }

} // namespace

namespace Raiden::Scene {

// JSON

bool saveJson(const SerializedScene &scene, Core::IVirtualFileSystem &vfs,
              std::string_view path) {
  json j;
  j["version"] = scene.version;

  json entities = json::array();
  for (const auto &e : scene.entities) {
    json ej;
    ej["name"] = e.name;

    if (e.hasTransform) {
      ej["transform"]["translation"] = vec3ToJson(e.translation);
      ej["transform"]["rotation"] = quatToJson(e.rotation);
      ej["transform"]["scale"] = vec3ToJson(e.scale);
      ej["transform"]["parent"] = e.parentIndex;
    }

    if (e.hasCamera) {
      ej["camera"]["active"] = e.cameraActive;
      ej["camera"]["fov"] = e.fov;
      ej["camera"]["zNear"] = e.zNear;
      ej["camera"]["zFar"] = e.zFar;
    }

    if (e.hasMeshRenderer) {
      ej["meshRenderer"]["mesh"] = e.meshPath;
      ej["meshRenderer"]["texture"] = e.texturePath;
      ej["meshRenderer"]["shader"] = e.shader;
      ej["meshRenderer"]["metallic"] = e.metallic;
      ej["meshRenderer"]["roughness"] = e.roughness;
    }

    if (e.hasRigidbody) {
      ej["rigidbody"]["type"] = e.rigidbodyType;
      ej["rigidbody"]["mass"] = e.rigidbodyMass;
      ej["rigidbody"]["friction"] = e.rigidbodyFriction;
      ej["rigidbody"]["restitution"] = e.rigidbodyRestitution;
    }

    if (e.hasCollider) {
      ej["collider"]["shape"] = e.colliderShape;
      ej["collider"]["halfExtents"] = vec3ToJson(e.colliderHalfExtents);
      ej["collider"]["radius"] = e.colliderRadius;
      ej["collider"]["height"] = e.colliderHeight;
    }

    entities.push_back(std::move(ej));
  }

  j["entities"] = std::move(entities);

  std::string contents = j.dump(2) + "\n";
  if (!vfs.write(path, contents)) {
    s_logger.error("Failed to write scene JSON to '{}'", path);
    return false;
  }

  s_logger.info("Saved scene ({} entities) to '{}'", scene.entities.size(),
                path);
  return true;
}

bool loadJson(SerializedScene &scene, Core::IVirtualFileSystem &vfs,
              std::string_view path) {
  if (!vfs.exists(path)) {
    s_logger.error("Scene file not found: '{}'", path);
    return false;
  }

  std::string contents = vfs.readAll(path);
  if (contents.empty()) {
    s_logger.error("Scene file is empty: '{}'", path);
    return false;
  }

  json j;
  try {
    j = json::parse(contents);
  } catch (const json::parse_error &err) {
    s_logger.error("Failed to parse scene JSON: {}", err.what());
    return false;
  }

  scene.version = j.value("version", 1);
  scene.entities.clear();

  if (j.contains("entities") && j["entities"].is_array()) {
    for (const auto &ej : j["entities"]) {
      EntityData e;
      e.name = ej.value("name", "");

      if (ej.contains("transform")) {
        const auto &t = ej["transform"];
        e.hasTransform = true;
        e.translation = vec3FromJson(t["translation"]);
        e.rotation = quatFromJson(t["rotation"]);
        e.scale = vec3FromJson(t["scale"]);
        e.parentIndex = t.value("parent", -1);
      }

      if (ej.contains("camera")) {
        const auto &c = ej["camera"];
        e.hasCamera = true;
        e.cameraActive = c.value("active", true);
        e.fov = c.value("fov", 45.0F);
        e.zNear = c.value("zNear", 0.1F);
        e.zFar = c.value("zFar", 100.0F);
      }

      if (ej.contains("meshRenderer")) {
        const auto &mr = ej["meshRenderer"];
        e.hasMeshRenderer = true;
        e.meshPath = mr.value("mesh", "");
        e.texturePath = mr.value("texture", "");
        e.shader = mr.value("shader", "builtin://pbr");
        e.metallic = mr.value("metallic", 0.0F);
        e.roughness = mr.value("roughness", 0.8F);
      }

      if (ej.contains("rigidbody")) {
        const auto &rb = ej["rigidbody"];
        e.hasRigidbody = true;
        e.rigidbodyType = rb.value("type", static_cast<uint8_t>(0));
        e.rigidbodyMass = rb.value("mass", 1.0F);
        e.rigidbodyFriction = rb.value("friction", 0.5F);
        e.rigidbodyRestitution = rb.value("restitution", 0.1F);
      }

      if (ej.contains("collider")) {
        const auto &col = ej["collider"];
        e.hasCollider = true;
        e.colliderShape = col.value("shape", static_cast<uint8_t>(0));
        e.colliderHalfExtents = vec3FromJson(col["halfExtents"]);
        e.colliderRadius = col.value("radius", 0.5F);
        e.colliderHeight = col.value("height", 1.0F);
      }

      scene.entities.push_back(std::move(e));
    }
  }

  s_logger.info("Loaded scene ({} entities) from '{}'", scene.entities.size(),
                path);
  return true;
}

// binary
static constexpr uint32_t kBinaryMagic = 0x44494152; // "RAID"
static constexpr uint32_t kComponentTransform = 1 << 0;
static constexpr uint32_t kComponentCamera = 1 << 1;
static constexpr uint32_t kComponentMeshRenderer = 1 << 2;
static constexpr uint32_t kComponentRigidbody = 1 << 3;
static constexpr uint32_t kComponentCollider = 1 << 4;

bool saveBinary(const SerializedScene &scene, Core::IVirtualFileSystem &vfs,
                std::string_view path) {
  std::vector<std::byte> buf;

  auto writeRaw = [&buf](const void *data, size_t size) {
    const auto *src = static_cast<const std::byte *>(data);
    buf.insert(buf.end(), src, src + size);
  };

  auto writeU32 = [&writeRaw](uint32_t v) { writeRaw(&v, sizeof(v)); };
  auto writeI32 = [&writeRaw](int32_t v) { writeRaw(&v, sizeof(v)); };
  auto writeF32 = [&writeRaw](float v) { writeRaw(&v, sizeof(v)); };
  auto writeU8 = [&writeRaw](uint8_t v) { writeRaw(&v, sizeof(v)); };

  // header
  writeU32(kBinaryMagic);
  writeU32(static_cast<uint32_t>(scene.version));
  writeU32(static_cast<uint32_t>(scene.entities.size()));

  // entity table: name + component mask
  for (const auto &e : scene.entities) {
    uint32_t mask = 0;
    if (e.hasTransform) {
      mask |= kComponentTransform;
    }
    if (e.hasCamera) {
      mask |= kComponentCamera;
    }
    if (e.hasMeshRenderer) {
      mask |= kComponentMeshRenderer;
    }
    if (e.hasRigidbody) {
      mask |= kComponentRigidbody;
    }
    if (e.hasCollider) {
      mask |= kComponentCollider;
    }

    auto nameLen = static_cast<uint32_t>(e.name.size());
    writeU32(nameLen);
    if (nameLen > 0) {
      writeRaw(e.name.data(), nameLen);
    }
    writeU32(mask);
  }

  // transform data
  for (const auto &e : scene.entities) {
    if (!e.hasTransform) {
      continue;
    }

    writeF32(e.translation.x);
    writeF32(e.translation.y);
    writeF32(e.translation.z);
    writeF32(e.rotation.x);
    writeF32(e.rotation.y);
    writeF32(e.rotation.z);
    writeF32(e.rotation.w);
    writeF32(e.scale.x);
    writeF32(e.scale.y);
    writeF32(e.scale.z);
    writeI32(e.parentIndex);
  }

  // camera data
  for (const auto &e : scene.entities) {
    if (!e.hasCamera) {
      continue;
    }
    writeU8(e.cameraActive ? 1 : 0);
    writeF32(e.fov);
    writeF32(e.zNear);
    writeF32(e.zFar);
  }

  // mesh renderer data
  for (const auto &e : scene.entities) {
    if (!e.hasMeshRenderer) {
      continue;
    }
    auto meshLen = static_cast<uint32_t>(e.meshPath.size());
    writeU32(meshLen);
    if (meshLen > 0) {
      writeRaw(e.meshPath.data(), meshLen);
    }
    auto texLen = static_cast<uint32_t>(e.texturePath.size());
    writeU32(texLen);
    if (texLen > 0) {
      writeRaw(e.texturePath.data(), texLen);
    }
    auto shaderLen = static_cast<uint32_t>(e.shader.size());
    writeU32(shaderLen);
    if (shaderLen > 0) {
      writeRaw(e.shader.data(), shaderLen);
    }
    writeF32(e.metallic);
    writeF32(e.roughness);
  }

  // rigidbody data
  for (const auto &e : scene.entities) {
    if (!e.hasRigidbody) {
      continue;
    }
    writeU8(e.rigidbodyType);
    writeF32(e.rigidbodyMass);
    writeF32(e.rigidbodyFriction);
    writeF32(e.rigidbodyRestitution);
  }

  // collider data
  for (const auto &e : scene.entities) {
    if (!e.hasCollider) {
      continue;
    }
    writeU8(e.colliderShape);
    writeF32(e.colliderHalfExtents.x);
    writeF32(e.colliderHalfExtents.y);
    writeF32(e.colliderHalfExtents.z);
    writeF32(e.colliderRadius);
    writeF32(e.colliderHeight);
  }

  if (!vfs.writeBytes(path, buf)) {
    s_logger.error("Failed to write scene binary to '{}'", path);
    return false;
  }

  s_logger.info("Saved binary scene ({} entities, {} bytes) to '{}'",
                scene.entities.size(), buf.size(), path);
  return true;
}

bool loadBinary(SerializedScene &scene, Core::IVirtualFileSystem &vfs,
                std::string_view path) {
  if (!vfs.exists(path)) {
    s_logger.error("Scene file not found: '{}'", path);
    return false;
  }

  auto buf = vfs.readBytes(path);
  if (buf.empty()) {
    s_logger.error("Scene file is empty: '{}'", path);
    return false;
  }

  const auto *data = buf.data();
  size_t remaining = buf.size();
  size_t pos = 0;

  auto readRaw = [&](void *dst, size_t size) -> bool {
    if (pos + size > remaining) {
      return false;
    }
    std::memcpy(dst, data + pos, size);
    pos += size;
    return true;
  };

  auto readU32 = [&readRaw](uint32_t &v) { return readRaw(&v, sizeof(v)); };
  auto readI32 = [&readRaw](int32_t &v) { return readRaw(&v, sizeof(v)); };
  auto readF32 = [&readRaw](float &v) { return readRaw(&v, sizeof(v)); };
  auto readU8 = [&readRaw](uint8_t &v) { return readRaw(&v, sizeof(v)); };

  // header
  uint32_t magic = 0;
  if (!readU32(magic) || magic != kBinaryMagic) {
    s_logger.error("Invalid binary scene magic in '{}'", path);
    return false;
  }

  uint32_t version = 0;
  if (!readU32(version)) {
    s_logger.error("Failed to read scene version");
    return false;
  }
  scene.version = static_cast<int>(version);

  uint32_t entityCount = 0;
  if (!readU32(entityCount)) {
    s_logger.error("Failed to read entity count");
    return false;
  }

  scene.entities.resize(entityCount);

  // entity table
  std::vector<uint32_t> masks(entityCount);
  for (uint32_t i = 0; i < entityCount; ++i) {
    uint32_t nameLen = 0;
    if (!readU32(nameLen)) {
      s_logger.error("Failed to read name length for entity {}", i);
      return false;
    }

    if (nameLen > 0) {
      scene.entities[i].name.resize(nameLen);
      if (!readRaw(scene.entities[i].name.data(), nameLen)) {
        s_logger.error("Failed to read name for entity {}", i);
        return false;
      }
    }

    if (!readU32(masks[i])) {
      s_logger.error("Failed to read component mask for entity {}", i);
      return false;
    }
  }

  // transform data
  for (uint32_t i = 0; i < entityCount; ++i) {
    if ((masks[i] & kComponentTransform) == 0U) {
      continue;
    }

    auto &e = scene.entities[i];
    e.hasTransform = true;
    if (!readF32(e.translation.x) || !readF32(e.translation.y) ||
        !readF32(e.translation.z) || !readF32(e.rotation.x) ||
        !readF32(e.rotation.y) || !readF32(e.rotation.z) ||
        !readF32(e.rotation.w) || !readF32(e.scale.x) || !readF32(e.scale.y) ||
        !readF32(e.scale.z) || !readI32(e.parentIndex)) {
      s_logger.error("Failed to read transform for entity {}", i);
      return false;
    }
  }

  // camera data
  for (uint32_t i = 0; i < entityCount; ++i) {
    if ((masks[i] & kComponentCamera) == 0U) {
      continue;
    }

    auto &e = scene.entities[i];
    e.hasCamera = true;
    uint8_t active = 0;
    if (!readU8(active) || !readF32(e.fov) || !readF32(e.zNear) ||
        !readF32(e.zFar)) {
      s_logger.error("Failed to read camera for entity {}", i);
      return false;
    }
    e.cameraActive = (active != 0);
  }

  // mesh renderer data
  for (uint32_t i = 0; i < entityCount; ++i) {
    if ((masks[i] & kComponentMeshRenderer) == 0U) {
      continue;
    }

    auto &e = scene.entities[i];
    e.hasMeshRenderer = true;

    uint32_t meshLen = 0;
    if (!readU32(meshLen)) {
      return false;
    }
    if (meshLen > 0) {
      e.meshPath.resize(meshLen);
      if (!readRaw(e.meshPath.data(), meshLen)) {
        return false;
      }
    }

    uint32_t texLen = 0;
    if (!readU32(texLen)) {
      return false;
    }
    if (texLen > 0) {
      e.texturePath.resize(texLen);
      if (!readRaw(e.texturePath.data(), texLen)) {
        return false;
      }
    }

    uint32_t shaderLen = 0;
    if (!readU32(shaderLen)) {
      return false;
    }
    if (shaderLen > 0) {
      e.shader.resize(shaderLen);
      if (!readRaw(e.shader.data(), shaderLen)) {
        return false;
      }
    }

    if (!readF32(e.metallic) || !readF32(e.roughness)) {
      return false;
    }
  }

  // rigidbody data
  for (uint32_t i = 0; i < entityCount; ++i) {
    if ((masks[i] & kComponentRigidbody) == 0U) {
      continue;
    }

    auto &e = scene.entities[i];
    e.hasRigidbody = true;
    uint8_t rbType = 0;
    if (!readU8(rbType) || !readF32(e.rigidbodyMass) ||
        !readF32(e.rigidbodyFriction) || !readF32(e.rigidbodyRestitution)) {
      s_logger.error("Failed to read rigidbody for entity {}", i);
      return false;
    }
    e.rigidbodyType = rbType;
  }

  // collider data
  for (uint32_t i = 0; i < entityCount; ++i) {
    if ((masks[i] & kComponentCollider) == 0U) {
      continue;
    }

    auto &e = scene.entities[i];
    e.hasCollider = true;
    uint8_t colShape = 0;
    if (!readU8(colShape) || !readF32(e.colliderHalfExtents.x) ||
        !readF32(e.colliderHalfExtents.y) || !readF32(e.colliderHalfExtents.z) ||
        !readF32(e.colliderRadius) || !readF32(e.colliderHeight)) {
      s_logger.error("Failed to read collider for entity {}", i);
      return false;
    }
    e.colliderShape = colShape;
  }

  s_logger.info("Loaded binary scene ({} entities) from '{}'",
                scene.entities.size(), path);
  return true;
}

// World <-> SerializedScen

SerializedScene serializeWorld(ECS::World &world) {
  SerializedScene scene;
  scene.version = SerializedScene::kCurrentVersion;

  // build entity-to-index map (stable sequential IDs)
  std::unordered_map<uint32_t, uint32_t> entityToIndex;

  // first pass: assign indices to all entities with Transform
  world.view<ECS::Transform>().each([&](ECS::Entity e, ECS::Transform &) {
    auto idx = static_cast<uint32_t>(scene.entities.size());
    entityToIndex[e.index] = idx;
    scene.entities.emplace_back();
  });

  // second pass: fill data
  world.view<ECS::Transform>().each([&](ECS::Entity e, ECS::Transform &t) {
    auto &ed = scene.entities[entityToIndex[e.index]];

    // name
    world.view<ECS::Name>().each([&](ECS::Entity ne, ECS::Name &n) {
      if (ne == e) {
        ed.name = n.value;
      }
    });

    // transform
    ed.hasTransform = true;
    ed.translation = t.translation;
    ed.rotation = t.rotation;
    ed.scale = t.scale;

    if (t.parent != ECS::NullEntity) {
      auto it = entityToIndex.find(t.parent.index);
      if (it != entityToIndex.end()) {
        ed.parentIndex = static_cast<int>(it->second);
      }
    }
  });

  // cameras
  world.view<ECS::Camera>().each([&](ECS::Entity e, ECS::Camera &c) {
    auto it = entityToIndex.find(e.index);
    if (it == entityToIndex.end()) {
      return;
    }

    auto &ed = scene.entities[it->second];
    ed.hasCamera = true;
    ed.cameraActive = c.active;
    ed.fov = c.fov;
    ed.zNear = c.zNear;
    ed.zFar = c.zFar;
  });

  // mesh renderers
  world.view<ECS::MeshRenderer>().each([&](ECS::Entity e, ECS::MeshRenderer &mr) {
    auto it = entityToIndex.find(e.index);
    if (it == entityToIndex.end()) {
      return;
    }

    auto &ed = scene.entities[it->second];
    ed.hasMeshRenderer = true;
    ed.meshPath = mr.meshPath;
    ed.texturePath = mr.texturePath;
    ed.shader = mr.shader;
    ed.metallic = mr.metallic;
    ed.roughness = mr.roughness;
  });

  // rigidbodies
  world.view<ECS::Rigidbody>().each([&](ECS::Entity e, ECS::Rigidbody &rb) {
    auto it = entityToIndex.find(e.index);
    if (it == entityToIndex.end()) {
      return;
    }

    auto &ed = scene.entities[it->second];
    ed.hasRigidbody = true;
    ed.rigidbodyType = static_cast<uint8_t>(rb.type);
    ed.rigidbodyMass = rb.mass;
    ed.rigidbodyFriction = rb.friction;
    ed.rigidbodyRestitution = rb.restitution;
  });

  // colliders
  world.view<ECS::Collider>().each([&](ECS::Entity e, ECS::Collider &col) {
    auto it = entityToIndex.find(e.index);
    if (it == entityToIndex.end()) {
      return;
    }

    auto &ed = scene.entities[it->second];
    ed.hasCollider = true;
    ed.colliderShape = static_cast<uint8_t>(col.shape);
    ed.colliderHalfExtents = col.halfExtents;
    ed.colliderRadius = col.radius;
    ed.colliderHeight = col.height;
  });

  return scene;
}

void deserializeWorld(const SerializedScene &scene, ECS::World &world) {
  // create entities and build index-to-entity map
  std::vector<ECS::Entity> entityMap(scene.entities.size());

  for (size_t i = 0; i < scene.entities.size(); ++i) {
    entityMap[i] = world.create();
  }

  // assign components
  for (size_t i = 0; i < scene.entities.size(); ++i) {
    const auto &ed = scene.entities[i];
    auto entity = entityMap[i];

    if (!ed.name.empty()) {
      world.assign<ECS::Name>(entity, ed.name);
    }

    if (ed.hasTransform) {
      ECS::Entity parent = ECS::NullEntity;
      if (ed.parentIndex >= 0 &&
          static_cast<size_t>(ed.parentIndex) < entityMap.size()) {
        parent = entityMap[ed.parentIndex];
      }

      ECS::Transform t;

      t.translation = ed.translation;
      t.rotation = ed.rotation;
      t.scale = ed.scale;
      t.parent = parent;

      world.assign<ECS::Transform>(entity, t);
    }

    if (ed.hasCamera) {
      world.assign<ECS::Camera>(entity);
      auto &cam = world.get<ECS::Camera>(entity);
      cam.active = ed.cameraActive;
      cam.fov = ed.fov;
      cam.zNear = ed.zNear;
      cam.zFar = ed.zFar;
    }

    if (ed.hasMeshRenderer) {
      ECS::MeshRenderer mr;
      mr.meshPath = ed.meshPath;
      mr.texturePath = ed.texturePath;
      mr.shader = ed.shader;
      mr.metallic = ed.metallic;
      mr.roughness = ed.roughness;
      world.assign<ECS::MeshRenderer>(entity, mr);
    }

    if (ed.hasRigidbody) {
      ECS::Rigidbody rb;
      rb.type = static_cast<ECS::Rigidbody::Type>(ed.rigidbodyType);
      rb.mass = ed.rigidbodyMass;
      rb.friction = ed.rigidbodyFriction;
      rb.restitution = ed.rigidbodyRestitution;
      world.assign<ECS::Rigidbody>(entity, rb);
    }

    if (ed.hasCollider) {
      ECS::Collider col;
      col.shape = static_cast<ECS::Collider::Shape>(ed.colliderShape);
      col.halfExtents = ed.colliderHalfExtents;
      col.radius = ed.colliderRadius;
      col.height = ed.colliderHeight;
      world.assign<ECS::Collider>(entity, col);
    }
  }

  // rebuild world matrices
  ECS::updateTransforms(world);
}

} // namespace Raiden::Scene
