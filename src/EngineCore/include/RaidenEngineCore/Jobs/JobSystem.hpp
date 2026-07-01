#pragma once

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string_view>
#include <vector>

namespace Raiden::Core {

class JobSystem;

class JobCounter {
public:
  JobCounter() = default;

  [[nodiscard]] bool isComplete() const;
  [[nodiscard]] int32_t value() const;

private:
  friend class JobSystem;
  struct Impl;
  std::shared_ptr<Impl> impl_;
};

enum class JobPriority : uint8_t {
  Low = 0,
  Normal = 1,
  High = 2,
  Count
};

struct JobDesc {
  std::function<void()> task;
  JobCounter dependency;
  std::string_view label = "";
  JobPriority priority = JobPriority::Normal;
};

struct JobSystemConfig {
  uint32_t numWorkers = 0;
  bool setThreadAffinity = false;
};

class JobSystem {
public:
  JobSystem();
  ~JobSystem();

  JobSystem(const JobSystem &) = delete;
  JobSystem &operator=(const JobSystem &) = delete;
  JobSystem(JobSystem &&) = delete;
  JobSystem &operator=(JobSystem &&) = delete;

  void init(const JobSystemConfig &config = {});
  void shutdown();

  [[nodiscard]] bool isInitialized() const { return initialized_; }

  JobCounter submit(JobDesc desc);
  JobCounter submitBatch(std::span<JobDesc> descs);

  void parallelFor(uint32_t begin, uint32_t end, uint32_t grainSize,
                   std::function<void(uint32_t, uint32_t)> fn);

  void waitAndAssist(const JobCounter &counter);
  bool assistOnce();

  [[nodiscard]] uint32_t workerCount() const {
    return static_cast<uint32_t>(workers_.size());
  }

  [[nodiscard]] static int32_t currentWorkerIndex();

private:
  struct InternalJob;
  struct WorkerThread;

  static bool dependencyUnmet(const InternalJob &job);
  void workerMain(uint32_t workerIndex);
  bool tryExecuteOne(uint32_t workerIndex);
  std::shared_ptr<InternalJob> tryGetJob(uint32_t workerIndex);
  void executeJob(std::shared_ptr<InternalJob> job);
  void counterReachedZero();
  void promotePendingJobs();
  void enqueue(std::shared_ptr<InternalJob> job);
  void wakeOne();
  void wakeAll();

  std::vector<std::unique_ptr<WorkerThread>> workers_;
  bool initialized_ = false;

  std::mutex globalMutex_;
  std::vector<std::shared_ptr<InternalJob>> globalQueue_;

  std::mutex pendingMutex_;
  std::vector<std::shared_ptr<InternalJob>> pendingJobs_;

  std::mutex wakeMutex_;
  std::condition_variable wakeCV_;
  std::atomic<bool> shutdownRequested_{false};
  std::atomic<uint64_t> jobsInFlight_{0};
};

} // namespace Raiden::Core
