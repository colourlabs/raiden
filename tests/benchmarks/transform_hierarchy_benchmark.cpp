#include <benchmark/benchmark.h>

#include <Raiden/ECS/Transform.hpp>
#include <Raiden/ECS/World.hpp>
#include <Raiden/Jobs/JobSystem.hpp>

using namespace Raiden::ECS;

// single parent with N children (flat hierarchy - depth 1)
static void BM_Transform_FlatHierarchy(benchmark::State &state) {
  World w;
  Entity root = w.create();
  w.assign<Transform>(root);

  for (int64_t i = 0; i < state.range(0); i++) {
    Entity c = w.create();
    auto &t = w.assign<Transform>(c);
    t.parent = root;
  }

  for (auto _ : state) {
    updateTransforms(w);
  }
}

BENCHMARK(BM_Transform_FlatHierarchy)->Arg(64)->Arg(65)->Arg(128)->Arg(1000)->Arg(10000); // NOLINT

// chain: 0 -> 1 -> 2 -> ... -> N (depth = N)
static void BM_Transform_ChainHierarchy(benchmark::State &state) {
  World w;
  std::vector<Entity> chain;
  for (int64_t i = 0; i < state.range(0); i++) {
    Entity e = w.create();
    auto &t = w.assign<Transform>(e);
    if (!chain.empty()) {
      t.parent = chain.back();
    }
    chain.push_back(e);
  }

  for (auto _ : state) {
    updateTransforms(w);
  }
}

BENCHMARK(BM_Transform_ChainHierarchy)->Arg(64)->Arg(65)->Arg(128)->Arg(1000); // NOLINT

// deep tree: binary tree with N total nodes
static void BM_Transform_BinaryTree(benchmark::State &state) {
  World w;
  int64_t n = state.range(0);
  std::vector<Entity> nodes;
  nodes.reserve(static_cast<size_t>(n));

  Entity root = w.create();
  w.assign<Transform>(root);
  nodes.push_back(root);

  for (int64_t i = 1; i < n; i++) {
    Entity e = w.create();
    auto &t = w.assign<Transform>(e);
    t.parent = nodes[static_cast<size_t>((i - 1) / 2)];
    nodes.push_back(e);
  }

  for (auto _ : state) {
    updateTransforms(w);
  }
}

BENCHMARK(BM_Transform_BinaryTree)->Arg(64)->Arg(65)->Arg(128)->Arg(1000)->Arg(10000); // NOLINT

static void BM_Transform_ParallelFlat(benchmark::State &state) {
  World w;
  Entity root = w.create();
  w.assign<Transform>(root);

  for (int64_t i = 0; i < state.range(0); i++) {
    Entity c = w.create();
    auto &t = w.assign<Transform>(c);
    t.parent = root;
  }

  Raiden::Jobs::JobSystem js;
  js.init();

  for (auto _ : state) {
    updateTransforms(w, js);
  }

  js.shutdown();
}

BENCHMARK(BM_Transform_ParallelFlat)->Arg(64)->Arg(65)->Arg(128)->Arg(1000)->Arg(10000); // NOLINT

static void BM_Transform_ParallelBinaryTree(benchmark::State &state) {
  World w;
  int64_t n = state.range(0);
  std::vector<Entity> nodes;
  nodes.reserve(static_cast<size_t>(n));

  Entity root = w.create();
  w.assign<Transform>(root);
  nodes.push_back(root);

  for (int64_t i = 1; i < n; i++) {
    Entity e = w.create();
    auto &t = w.assign<Transform>(e);
    t.parent = nodes[static_cast<size_t>((i - 1) / 2)];
    nodes.push_back(e);
  }

  Raiden::Jobs::JobSystem js;
  js.init();

  for (auto _ : state) {
    updateTransforms(w, js);
  }

  js.shutdown();
}

BENCHMARK(BM_Transform_ParallelBinaryTree)->Arg(64)->Arg(65)->Arg(128)->Arg(1000)->Arg(10000); // NOLINT

BENCHMARK_MAIN(); // NOLINT
