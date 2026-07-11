#include <Raiden/Jobs/JobSystem.hpp>
#include <gtest/gtest.h>

#include <atomic>

using namespace Raiden::Jobs;

// sanity

TEST(JobSystemTest, InitAndShutdown) {
  JobSystem js;
  EXPECT_FALSE(js.isInitialized());
  EXPECT_EQ(js.workerCount(), 0);
  js.init({});
  EXPECT_TRUE(js.isInitialized());
  EXPECT_GE(js.workerCount(), 1);
  js.shutdown();
  EXPECT_FALSE(js.isInitialized());
}

TEST(JobSystemTest, InitIsIdempotent) {
  JobSystem js;
  js.init({});
  js.init({}); // second call should be no-op
  EXPECT_TRUE(js.isInitialized());
}

TEST(JobSystemTest, DestructorCallsShutdown) {
  {
    JobSystem js;
    js.init({});
    EXPECT_TRUE(js.isInitialized());
  } // destructor runs; should not crash or hang
}

// submit / waitAndAssist

TEST(JobSystemTest, SubmitAndWait) {
  JobSystem js;
  js.init({.numWorkers = 2});
  std::atomic<bool> ran{false};
  auto c = js.submit({.task = [&] { ran.store(true); }});
  js.waitAndAssist(c);
  EXPECT_TRUE(ran.load());
  EXPECT_TRUE(c.isComplete());
}

TEST(JobSystemTest, SubmitMultipleAndWait) {
  JobSystem js;
  js.init({.numWorkers = 4});
  std::atomic<int32_t> count{0};
  std::vector<JobCounter> counters;

  counters.reserve(100);
  for (int i = 0; i < 100; i++) {
    counters.push_back(js.submit({.task = [&] { count.fetch_add(1); }}));
  }

  for (auto &c : counters) {
    js.waitAndAssist(c);
  }
  EXPECT_EQ(count.load(), 100);
}

TEST(JobSystemTest, SubmitOnZeroWorkers) {
  // degenerate case: main thread executes jobs via assistOnce during wait
  JobSystem js;
  js.init({.numWorkers = 0});
  std::atomic<bool> ran{false};
  auto c = js.submit({.task = [&] { ran.store(true); }});
  js.waitAndAssist(c);
  EXPECT_TRUE(ran.load());
}

// submitBatch

TEST(JobSystemTest, SubmitBatch) {
  JobSystem js;
  js.init({.numWorkers = 2});
  std::atomic<int32_t> count{0};
  std::array<JobDesc, 10> descs;
  for (auto &d : descs) {
    d.task = [&] { count.fetch_add(1); };
  }
  auto c = js.submitBatch(descs);
  js.waitAndAssist(c);
  EXPECT_EQ(count.load(), 10);
}

TEST(JobSystemTest, SubmitBatchEmpty) {
  JobSystem js;
  js.init({.numWorkers = 2});
  auto c = js.submitBatch({});
  EXPECT_TRUE(c.isComplete());
}

// parallelFor

TEST(JobSystemTest, ParallelForRange) {
  JobSystem js;
  js.init({.numWorkers = 4});
  std::atomic<int32_t> sum{0};
  js.parallelFor(0, 1000, 10, [&](uint32_t begin, uint32_t end) {
    for (uint32_t i = begin; i < end; i++) {
      sum.fetch_add(1);
    }
  });
  EXPECT_EQ(sum.load(), 1000);
}

TEST(JobSystemTest, ParallelForEmptyRange) {
  JobSystem js;
  js.init({.numWorkers = 2});
  bool ran = false;
  js.parallelFor(5, 5, 1, [&](uint32_t, uint32_t) { ran = true; });
  EXPECT_FALSE(ran);
}

TEST(JobSystemTest, ParallelForSingleChunkRunsInline) {
  JobSystem js;
  js.init({.numWorkers = 2});
  bool ran = false;
  js.parallelFor(0, 5, 100, [&](uint32_t, uint32_t) { ran = true; });
  EXPECT_TRUE(ran);
}

TEST(JobSystemTest, ParallelForZeroGrainSizeDefaultsToOne) {
  JobSystem js;
  js.init({.numWorkers = 2});
  std::atomic<int32_t> sum{0};
  js.parallelFor(0, 10, 0, [&](uint32_t b, uint32_t e) {
    sum.fetch_add(static_cast<int32_t>(e - b));
  });
  EXPECT_EQ(sum.load(), 10);
}

// dependencies

TEST(JobSystemTest, JobWithDependency) {
  JobSystem js;
  js.init({.numWorkers = 2});
  std::atomic<int32_t> order{0};
  // stage 1: one job
  auto first = js.submit({.task = [&] { order.store(1); }});
  // stage 2: depends on first
  auto second =
      js.submit({.task = [&] { order.store(2); }, .dependency = first});
  js.waitAndAssist(second);
  EXPECT_EQ(order.load(), 2);
}

TEST(JobSystemTest, ChainOfDependencies) {
  JobSystem js;
  js.init({.numWorkers = 2});
  std::atomic<int32_t> count{0};
  auto prev = js.submit({.task = [&] { count.fetch_add(1); }});
  for (int i = 0; i < 20; i++) {
    auto next =
        js.submit({.task = [&] { count.fetch_add(1); }, .dependency = prev});
    prev = std::move(next);
  }

  js.waitAndAssist(prev);
  EXPECT_EQ(count.load(), 21);
}

// assistOnce

TEST(JobSystemTest, AssistOnceExecutesJob) {
  JobSystem js;
  js.init({.numWorkers = 0});
  std::atomic<bool> ran{false};
  js.submit({.task = [&] { ran.store(true); }});
  // assistOnce may need to check the global queue (external thread path)
  bool executed = false;
  for (int i = 0; i < 100; i++) {
    if (js.assistOnce()) { executed = true; break; }
    std::this_thread::yield();
  }
  EXPECT_TRUE(executed);
  EXPECT_TRUE(ran.load());
}

TEST(JobSystemTest, AssistOnceReturnsFalseWhenIdle) {
  JobSystem js;
  js.init({.numWorkers = 0});
  EXPECT_FALSE(js.assistOnce());
}

// currentWorkerIndex

TEST(JobSystemTest, CurrentWorkerIndexOutsideWorker) {
  EXPECT_EQ(JobSystem::currentWorkerIndex(), -1);
}

// ParallelFor stress (concurrent writes are fine with atomics)

TEST(JobSystemTest, ParallelForResultsCorrect) {
  for (int iter = 0; iter < 10; iter++) {
    JobSystem js;
    js.init({.numWorkers = 4});
    int N = 10000;
    std::atomic<int64_t> sum{0};
    js.parallelFor(0, N, 64, [&](uint32_t b, uint32_t e) {
      int64_t local = 0;
    
      for (uint32_t i = b; i < e; i++) {
        local += static_cast<int64_t>(i);
      }
    
      sum.fetch_add(local);
    });
    int64_t expected = static_cast<int64_t>(N) * (N - 1) / 2;
    EXPECT_EQ(sum.load(), expected);
  }
}

// make sure WorkerThread queue stays private (no data races on per-worker
// queue push/pop). This stress test would trip TSAN if there were races.
TEST(JobSystemTest, WorkStealingNoDataRace) {
  JobSystem js;
  js.init({.numWorkers = 4});
  std::atomic<int64_t> sum{0};
  // many fine-grained jobs so workers steal from each other
  js.parallelFor(0, 50000, 1, [&](uint32_t b, uint32_t e) {
    int64_t local = 0;
    
    for (uint32_t i = b; i < e; i++) {
      local += static_cast<int64_t>(i);
    }

    sum.fetch_add(local, std::memory_order_relaxed);
  });
  int64_t expected = static_cast<int64_t>(50000) * 49999 / 2;
  EXPECT_EQ(sum.load(), expected);
}
