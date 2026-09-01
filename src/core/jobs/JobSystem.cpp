#include "core/jobs/JobSystem.h"

#include <algorithm>

namespace rf::core::jobs {

namespace {
std::size_t defaultWorkerCount() {
    const auto hardware = static_cast<std::size_t>(std::thread::hardware_concurrency());
    if (hardware <= 2) return 1;
    return std::clamp<std::size_t>(hardware - 2, 1, 8);
}
} // namespace

JobSystem::JobSystem(std::size_t workerCount) {
    if (workerCount == 0) workerCount = defaultWorkerCount();
    workers_.reserve(workerCount);
    for (std::size_t i = 0; i < workerCount; ++i) workers_.emplace_back([this]() { workerLoop(); });
}

JobSystem::~JobSystem() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
}

void JobSystem::waitIdle() {
    std::unique_lock lock(mutex_);
    idle_.wait(lock, [this]() { return queue_.empty() && activeJobs_ == 0; });
}

void JobSystem::workerLoop() {
    for (;;) {
        std::function<void()> job;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this]() { return stopping_ || !queue_.empty(); });
            if (stopping_ && queue_.empty()) return;
            job = std::move(queue_.front());
            queue_.pop();
            ++activeJobs_;
        }

        job();

        {
            std::lock_guard lock(mutex_);
            --activeJobs_;
            if (queue_.empty() && activeJobs_ == 0) idle_.notify_all();
        }
    }
}

} // namespace rf::core::jobs
