#include <benchmark/benchmark.h>

#include <Raiden/ECS/Camera.hpp>
#include <Raiden/ECS/MeshRenderer.hpp>
#include <Raiden/ECS/Name.hpp>
#include <Raiden/ECS/Transform.hpp>
#include <Raiden/ECS/World.hpp>

using namespace Raiden::ECS;

static void BM_ECS_CreateEntities(benchmark::State &state) {
  for (auto _ : state) {
    World w;
    for (int64_t i = 0; i < state.range(0); i++) {
      w.create();
    }
  }
}

BENCHMARK(BM_ECS_CreateEntities)->Arg(64)->Arg(65)->Arg(128)->Arg(1000)->Arg(10000); // NOLINT

static void BM_ECS_AssignComponent(benchmark::State &state) {
  for (auto _ : state) {
    World w;
    std::vector<Entity> ents;
    ents.reserve(state.range(0));

    for (int64_t i = 0; i < state.range(0); i++) {
      ents.push_back(w.create());
    }
    
    for (auto e : ents) {
      w.assign<Transform>(e);
    }
  }
}

BENCHMARK(BM_ECS_AssignComponent)->Arg(64)->Arg(65)->Arg(128)->Arg(1000)->Arg(10000); // NOLINT

static void BM_ECS_ViewSingle(benchmark::State &state) {
  World w;
  for (int64_t i = 0; i < state.range(0); i++) {
    Entity e = w.create();
    w.assign<Transform>(e);
  }

  for (auto _ : state) {
    int count = 0;
    w.view<Transform>().each([&](Entity, Transform &) {
      count++;
      benchmark::DoNotOptimize(count);
    });
  }
}

BENCHMARK(BM_ECS_ViewSingle)->Arg(64)->Arg(65)->Arg(128)->Arg(1000)->Arg(10000); // NOLINT

static void BM_ECS_ViewMultiComponent(benchmark::State &state) {
  World w;
  for (int64_t i = 0; i < state.range(0); i++) {
    Entity e = w.create();
    w.assign<Transform>(e);
    w.assign<Name>(e, std::string("entity"));
  }

  for (auto _ : state) {
    int count = 0;
    w.view<Transform, Name>().each([&](Entity, Transform &, Name &) {
      count++;
      benchmark::DoNotOptimize(count);
    });
  }
}

BENCHMARK(BM_ECS_ViewMultiComponent)->Arg(64)->Arg(65)->Arg(128)->Arg(1000)->Arg(10000); // NOLINT

static void BM_ECS_DestroyAll(benchmark::State &state) {
  for (auto _ : state) {
    {
      World w;
      std::vector<Entity> ents;
      for (int64_t i = 0; i < state.range(0); i++) {
        Entity e = w.create();
        w.assign<Transform>(e);
        w.assign<Name>(e, std::string("test"));
        ents.push_back(e);
      }
      for (auto e : ents) {
        w.destroy(e);
      }
    }
  }
}

BENCHMARK(BM_ECS_DestroyAll)->Arg(64)->Arg(65)->Arg(128)->Arg(1000)->Arg(10000); // NOLINT

static void BM_ECS_TransformHierarchy(benchmark::State &state) {
  World w;
  std::vector<Entity> parents;
  for (int64_t i = 0; i < state.range(0); i++) {
    Entity p = w.create();
    w.assign<Transform>(p);
    parents.push_back(p);
    Entity c = w.create();
    auto &t = w.assign<Transform>(c);
    t.parent = p;
  }

  for (auto _ : state) {
    updateTransforms(w);
  }
}

BENCHMARK(BM_ECS_TransformHierarchy)->Arg(64)->Arg(65)->Arg(128)->Arg(1000)->Arg(10000); // NOLINT

BENCHMARK_MAIN(); // NOLINT
