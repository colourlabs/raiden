#include <Raiden/Jobs/JobSystem.hpp>

#include <algorithm>
#include <cstdlib>

namespace Raiden::Jobs {

namespace {
thread_local int32_t tls_workerIndex = -1; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}

struct JobCounter::Impl {
  std::atomic<int32_t> counter{0};
};

bool JobCounter::isComplete() const {
  return !impl_ || impl_->counter.load(std::memory_order_acquire) == 0;
}

int32_t JobCounter::value() const {
  return impl_ ? impl_->counter.load(std::memory_order_relaxed) : 0;
}

struct JobSystem::InternalJob {
  std::function<void()> task;
  JobPriority priority = JobPriority::Normal;
  std::string label;
  std::shared_ptr<JobCounter::Impl> counterToDecrement;
  JobCounter dependency;
};

struct JobSystem::WorkerThread {
  std::thread thread;
  std::mutex mutex;
  std::vector<std::shared_ptr<InternalJob>> queue;
  uint32_t index = 0;
};

JobSystem::JobSystem() = default;

JobSystem::~JobSystem() { shutdown(); }

void JobSystem::init(const JobSystemConfig &config) {
  if (initialized_) {
    return;
  }

  uint32_t numWorkers = config.numWorkers;
  if (numWorkers == 0) {
    auto hw = std::thread::hardware_concurrency();
    numWorkers = hw > 1 ? hw - 1 : 1;
  }

  workers_.reserve(numWorkers);
  for (uint32_t i = 0; i < numWorkers; ++i) {
    auto worker = std::make_unique<WorkerThread>();
    worker->index = i;
    worker->thread = std::thread(&JobSystem::workerMain, this, i);
    workers_.push_back(std::move(worker));
  }

  initialized_ = true;
}

void JobSystem::shutdown() {
  if (!initialized_) {
    return;
}

  shutdownRequested_.store(true, std::memory_order_release);
  wakeAll();

  for (auto &worker : workers_) {
    if (worker->thread.joinable()) {
      worker->thread.join();
}
  }

  workers_.clear();

  {
    std::scoped_lock lock(globalMutex_);
    for (auto &q : globalQueues_) {
      q.clear();
}
  }

  {
    std::scoped_lock lock(pendingMutex_);
    pendingJobs_.clear();
  }

  initialized_ = false;
}

bool JobSystem::dependencyUnmet(const InternalJob &job) {
  return job.dependency.impl_ && !job.dependency.isComplete();
}

void JobSystem::wakeOne() { wakeCV_.notify_one(); }

void JobSystem::wakeAll() { wakeCV_.notify_all(); }

void JobSystem::enqueue(std::shared_ptr<InternalJob> job) {
  if (!job) {
    return;
}
  std::scoped_lock lock(globalMutex_);
  globalQueues_[static_cast<size_t>(job->priority)].push_back(std::move(job));
}

JobCounter JobSystem::submit(JobDesc desc) {
  auto counter = std::make_shared<JobCounter::Impl>();
  counter->counter.store(1, std::memory_order_relaxed);

  auto job = jobPool_.acquire();
  job->task = std::move(desc.task);
  job->priority = desc.priority;
  job->label = desc.label;
  job->counterToDecrement = counter;
  job->dependency = std::move(desc.dependency);

  if (dependencyUnmet(*job)) {
  std::scoped_lock lock(pendingMutex_);
    pendingJobs_.push_back(std::move(job));
  } else {
    enqueue(std::move(job));
  }

  wakeOne();

  JobCounter result;
  result.impl_ = std::move(counter);
  return result;
}

JobCounter JobSystem::submitBatch(std::span<JobDesc> descs) {
  if (descs.empty()) {
    JobCounter result;
    result.impl_ = std::make_shared<JobCounter::Impl>();
    result.impl_->counter.store(0, std::memory_order_relaxed);
    return result;
  }

  auto counter = std::make_shared<JobCounter::Impl>();
  counter->counter.store(static_cast<int32_t>(descs.size()),
                         std::memory_order_relaxed);

  bool enqueuedAny = false;
  auto numWorkers = static_cast<uint32_t>(workers_.size());
  uint32_t w = 0;

  for (auto &desc : descs) {
    auto job = jobPool_.acquire();
    job->task = desc.task;
    job->priority = desc.priority;
    job->label = desc.label;
    job->counterToDecrement = counter;
    job->dependency = desc.dependency;

    if (dependencyUnmet(*job)) {
      std::scoped_lock lock(pendingMutex_);
      pendingJobs_.push_back(std::move(job));
      continue;
    }

    if (numWorkers > 0) {
      auto &worker = workers_[w % numWorkers];
      std::scoped_lock lock(worker->mutex);
      worker->queue.push_back(std::move(job));
      ++w;
    } else {
      enqueue(std::move(job)); // fallback: no workers, use global queue
    }
    enqueuedAny = true;
  }

  if (enqueuedAny) {
    wakeAll();
}

  JobCounter result;
  result.impl_ = std::move(counter);
  return result;
}

void JobSystem::parallelFor(uint32_t begin, uint32_t end, uint32_t grainSize,
                            const std::function<void(uint32_t, uint32_t)> &fn) {
  if (begin >= end) {
    return;
  }

  if (grainSize == 0) {
    grainSize = 1;
  }

  std::vector<JobDesc> descs;
  uint32_t chunkStart = begin;

  while (chunkStart < end) {
    uint32_t chunkEnd = std::min(chunkStart + grainSize, end);

    descs.push_back(
        {.task = [fn, chunkStart, chunkEnd]() { fn(chunkStart, chunkEnd); }});

    chunkStart = chunkEnd;
  }

  if (descs.size() == 1) {
    fn(begin, end);
    return;
  }

  auto counter = submitBatch(descs);
  waitAndAssist(counter);
}

void JobSystem::waitAndAssist(const JobCounter &counter) {
  while (!counter.isComplete()) {
    if (!assistOnce()) {
      std::this_thread::yield();
    }
  }
}

bool JobSystem::assistOnce() {
  int32_t idx = tls_workerIndex;

  if (idx >= 0) {
    return tryExecuteOne(static_cast<uint32_t>(idx));
  }

  return tryExecuteOne(UINT32_MAX);
}

int32_t JobSystem::currentWorkerIndex() { return tls_workerIndex; }

// job retrieval and execution

std::shared_ptr<JobSystem::InternalJob>
JobSystem::tryGetJob(uint32_t workerIndex) {
  // own queue (LIFO from back for cache locality)
  if (workerIndex < workers_.size()) {
    auto &worker = workers_[workerIndex];
    std::scoped_lock lock(worker->mutex);
    if (!worker->queue.empty()) {
      auto job = std::move(worker->queue.back());
      worker->queue.pop_back();
      return job;
    }
  }

  // steal from a random victim (FIFO from front to minimise contention)
  auto numWorkers = static_cast<uint32_t>(workers_.size());

  if (numWorkers > 0) {
    uint32_t start = static_cast<uint32_t>(std::rand()) % numWorkers;
    for (uint32_t i = 0; i < numWorkers; ++i) {
      uint32_t victimIndex = (start + i) % numWorkers;
      
      if (victimIndex == workerIndex) {
        continue;
      }

      auto &victim = workers_[victimIndex];
      std::scoped_lock lock(victim->mutex);
      
      if (!victim->queue.empty()) {
        auto job = std::move(victim->queue.front());
        victim->queue.erase(victim->queue.begin());
        return job;
      }
    }
  }

  // global queue (pick highest priority)
  {
    std::scoped_lock lock(globalMutex_);
    for (int p = static_cast<int>(JobPriority::Count) - 1; p >= 0; --p) {
      auto &q = globalQueues_[p];
      if (!q.empty()) {
        auto job = std::move(q.back());
        q.pop_back();
        return job;
      }
    }
  }

  return nullptr;
}

void JobSystem::executeJob(std::shared_ptr<InternalJob> job) {
  if (!job) {
    return;
}

  if (dependencyUnmet(*job)) {
    std::scoped_lock lock(pendingMutex_);
    pendingJobs_.push_back(std::move(job));
    return;
  }

  if (job->task) {
    job->task();
}

  auto counter = job->counterToDecrement;
  job->counterToDecrement.reset();
  job->task = nullptr; // release captured state before pooling
  job->dependency = JobCounter{};

  jobPool_.release(std::move(job));

  if (counter) {
    int32_t prev = counter->counter.fetch_sub(1, std::memory_order_acq_rel);
    if (prev == 1) {
      counterReachedZero();
    }
  }
}

void JobSystem::counterReachedZero() {
  promotePendingJobs();
  wakeAll();
}

void JobSystem::promotePendingJobs() {
  std::scoped_lock lock(pendingMutex_);

  auto it = pendingJobs_.begin();
  while (it != pendingJobs_.end()) {
    if (!dependencyUnmet(**it)) {
      enqueue(std::move(*it));
      it = pendingJobs_.erase(it);
    } else {
      ++it;
    }
  }
}

bool JobSystem::tryExecuteOne(uint32_t workerIndex) {
  auto job = tryGetJob(workerIndex);
  if (!job) {
    return false;
}

  executeJob(std::move(job));

  return true;
}

// worker main

void JobSystem::workerMain(uint32_t workerIndex) {
  tls_workerIndex = static_cast<int32_t>(workerIndex);

  while (true) {
    if (tryExecuteOne(workerIndex)) {
      continue;
}

    promotePendingJobs();

    if (shutdownRequested_.load(std::memory_order_acquire)) {
      while (tryExecuteOne(workerIndex)) {
      }
      break;
    }

    {
      std::unique_lock<std::mutex> lock(wakeMutex_);
      wakeCV_.wait_for(lock, std::chrono::milliseconds(1));
    }
  }

  tls_workerIndex = -1;
}

} // namespace Raiden::Jobs