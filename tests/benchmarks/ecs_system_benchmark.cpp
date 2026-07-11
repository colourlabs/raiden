#include <benchmark/benchmark.h>

#include <Raiden/ECS/Name.hpp>
#include <Raiden/ECS/System.hpp>
#include <Raiden/ECS/Transform.hpp>
#include <Raiden/ECS/World.hpp>

using namespace Raiden::ECS;

class CounterSystem : public System {
public:
  void update(World &world, float /*dt*/) override {
    count_ = 0;
    world.view<Transform>().each([&](Entity, Transform &) {
      count_++;
      benchmark::DoNotOptimize(count_);
    });
  }
  [[nodiscard]] int count() const { return count_; }

private:
  int count_ = 0;
};

class MultiViewSystem : public System {
public:
  void update(World &world, float /*dt*/) override {
    count_ = 0;
    world.view<Transform, Name>().each([&](Entity, Transform &, Name &) {
      count_++;
      benchmark::DoNotOptimize(count_);
    });
  }
  [[nodiscard]] int count() const { return count_; }

private:
  int count_ = 0;
};

static void BM_System_SingleDispatch(benchmark::State &state) {
  World w;
  for (int64_t i = 0; i < state.range(0); i++) {
    Entity e = w.create();
    w.assign<Transform>(e);
  }
  w.addSystem<CounterSystem>();

  for (auto _ : state) {
    w.updateSystems(0.016F);
    benchmark::DoNotOptimize(w);
  }
}

BENCHMARK(BM_System_SingleDispatch)->Arg(64)->Arg(65)->Arg(128)->Arg(1000)->Arg(10000); // NOLINT

static void BM_System_MultiDispatch(benchmark::State &state) {
  World w;
  for (int64_t i = 0; i < state.range(0); i++) {
    Entity e = w.create();
    w.assign<Transform>(e);
    w.assign<Name>(e, std::string("entity"));
  }
  w.addSystem<CounterSystem>();
  w.addSystem<MultiViewSystem>();
  w.addSystem<CounterSystem>();

  for (auto _ : state) {
    w.updateSystems(0.016F);
    benchmark::DoNotOptimize(w);
  }
}

BENCHMARK(BM_System_MultiDispatch)->Arg(64)->Arg(65)->Arg(128)->Arg(1000)->Arg(10000); // NOLINT

static void BM_Inline_SingleView(benchmark::State &state) {
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

BENCHMARK(BM_Inline_SingleView)->Arg(64)->Arg(65)->Arg(128)->Arg(1000)->Arg(10000); // NOLINT

static void BM_Inline_MultiView(benchmark::State &state) {
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

BENCHMARK(BM_Inline_MultiView)->Arg(64)->Arg(65)->Arg(128)->Arg(1000)->Arg(10000); // NOLINT

static void BM_System_SparseView(benchmark::State &state) {
  World w;
  int64_t total = state.range(0);
  for (int64_t i = 0; i < total; i++) {
    Entity e = w.create();
    w.assign<Transform>(e);
    if (i % 10 == 0) {
      w.assign<Name>(e, std::string("named"));
    }
  }
  w.addSystem<MultiViewSystem>();

  for (auto _ : state) {
    w.updateSystems(0.016F);
    benchmark::DoNotOptimize(w);
  }
}

BENCHMARK(BM_System_SparseView)->Arg(1000)->Arg(10000); // NOLINT

static void BM_Inline_SparseView(benchmark::State &state) {
  World w;
  int64_t total = state.range(0);
  for (int64_t i = 0; i < total; i++) {
    Entity e = w.create();
    w.assign<Transform>(e);
    if (i % 10 == 0) {
      w.assign<Name>(e, std::string("named"));
    }
  }

  for (auto _ : state) {
    int count = 0;
    w.view<Transform, Name>().each([&](Entity, Transform &, Name &) {
      count++;
      benchmark::DoNotOptimize(count);
    });
  }
}

BENCHMARK(BM_Inline_SparseView)->Arg(1000)->Arg(10000); // NOLINT

static void BM_System_AddOverhead(benchmark::State &state) {
  for (auto _ : state) {
    World w;
    for (int64_t i = 0; i < state.range(0); i++) {
      Entity e = w.create();
      w.assign<Transform>(e);
    }
    for (int i = 0; i < 10; i++) {
      w.addSystem<CounterSystem>();
    }
    benchmark::DoNotOptimize(w);
  }
}

BENCHMARK(BM_System_AddOverhead)->Arg(1000)->Arg(10000); // NOLINT

BENCHMARK_MAIN(); // NOLINT
