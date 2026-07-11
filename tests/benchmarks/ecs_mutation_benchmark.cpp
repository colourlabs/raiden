#include <benchmark/benchmark.h>

#include <Raiden/ECS/Name.hpp>
#include <Raiden/ECS/Transform.hpp>
#include <Raiden/ECS/World.hpp>

using namespace Raiden::ECS;

static void BM_ECS_Assign(benchmark::State &state) {
  for (auto _ : state) {
    World w;
    Entity e = w.create();
    for (int64_t i = 0; i < state.range(0); i++) {
      benchmark::DoNotOptimize(w.assign<Transform>(e));
      w.remove<Transform>(e);
    }
    benchmark::DoNotOptimize(w);
  }
}

BENCHMARK(BM_ECS_Assign)->Arg(100)->Arg(1000); // NOLINT

static void BM_ECS_AssignMultiple(benchmark::State &state) {
  for (auto _ : state) {
    World w;
    Entity e = w.create();
    for (int64_t i = 0; i < state.range(0); i++) {
      w.assign<Transform>(e);
      w.assign<Name>(e, std::string("test"));
      w.remove<Name>(e);
      w.remove<Transform>(e);
    }
    benchmark::DoNotOptimize(w);
  }
}

BENCHMARK(BM_ECS_AssignMultiple)->Arg(100)->Arg(1000); // NOLINT

static void BM_ECS_Remove(benchmark::State &state) {
  for (auto _ : state) {
    World w;
    Entity e = w.create();
    w.assign<Transform>(e);
    for (int64_t i = 0; i < state.range(0); i++) {
      w.remove<Transform>(e);
      w.assign<Transform>(e);
    }
    benchmark::DoNotOptimize(w);
  }
}

BENCHMARK(BM_ECS_Remove)->Arg(100)->Arg(1000); // NOLINT

static void BM_ECS_DestroyCreateCycle(benchmark::State &state) {
  for (auto _ : state) {
    World w;
    std::vector<Entity> pool;
    pool.reserve(state.range(0));
    for (int64_t i = 0; i < state.range(0); i++) {
      pool.push_back(w.create());
    }
    // cycle: destroy half, recreate
    for (int64_t i = 0; i < state.range(0); i += 2) {
      w.destroy(pool[i]);
    }
    for (int64_t i = 0; i < state.range(0); i += 2) {
      pool[i] = w.create();
    }
    for (auto e : pool) {
      w.destroy(e);
    }
  }
}

BENCHMARK(BM_ECS_DestroyCreateCycle)->Arg(1000)->Arg(10000); // NOLINT

static void BM_ECS_AssignMixedArchetypes(benchmark::State &state) {
  for (auto _ : state) {
    state.PauseTiming();
    World w;
    std::vector<Entity> ents;
    ents.reserve(state.range(0));
    for (int64_t i = 0; i < state.range(0); i++) {
      ents.push_back(w.create());
    }
    // spread components across archetypes
    for (size_t i = 0; i < ents.size(); i++) {
      w.assign<Transform>(ents[i]);
      if (i % 2 == 0) {
        w.assign<Name>(ents[i], std::string("even"));
      }
    }
    state.ResumeTiming();

    // now reassign to cause archetype migrations
    for (size_t i = 0; i < ents.size(); i++) {
      if (i % 2 == 0) {
        w.remove<Name>(ents[i]);
      } else {
        w.assign<Name>(ents[i], std::string("odd"));
      }
    }
    benchmark::DoNotOptimize(w);
  }
}

BENCHMARK(BM_ECS_AssignMixedArchetypes)->Arg(1000)->Arg(10000); // NOLINT

BENCHMARK_MAIN(); // NOLINT
