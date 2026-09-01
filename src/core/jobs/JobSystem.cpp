#include "core/jobs/JobSystem.h"

#include <algorithm>

namespace rf::core::jobs {

JobSystem::JobSystem(std::size_t workerCount) {
    if (workerCount == 0) {
        const auto detected = static_cast<std::size_t>(std::thread::hardware_concurrency());
        workerCount = detected > 2 ? detected - 1 : 1;
    }
    workerCount = std::clamp<std::size_t>(workerCount, 1, 32);
    workers_.reserve(workerCount);
    for (std::size_t i = 0; i < workerCount; ++i) workers_.emplace_back([this] { workerLoop(); });
}

JobSystem::~JobSystem() {
    {
        std::scoped_lock lock(mutex_);
        stopping_ = true;
    }
    workAvailable_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
}

void JobSystem::submit(std::function<void()> job) {
    if (!job) return;
    {
        std::scoped_lock lock(mutex_);
        if (stopping_) return;
        queue_.push(std::move(job));
    }
    workAvailable_.notify_one();
}

void JobSystem::waitIdle() {
    std::unique_lock lock(mutex_);
    idle_.wait(lock, [this] { return queue_.empty() && activeJobs_ == 0; });
}

std::size_t JobSystem::pendingJobs() const {
    std::scoped_lock lock(mutex_);
    return queue_.size() + activeJobs_;
}

void JobSystem::workerLoop() {
    for (;;) {
        std::function<void()> job;
        {
            std::unique_lock lock(mutex_);
            workAvailable_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (stopping_ && queue_.empty()) return;
            job = std::move(queue_.front());
            queue_.pop();
            ++activeJobs_;
        }

        try {
            job();
        } catch (...) {
            // Worker threads must remain alive. Game systems own error propagation for their jobs.
        }

        {
            std::scoped_lock lock(mutex_);
            --activeJobs_;
            if (queue_.empty() && activeJobs_ == 0) idle_.notify_all();
        }
    }
}

} // namespace rf::core::jobs
