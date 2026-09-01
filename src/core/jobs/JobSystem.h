#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace rf::core::jobs {

class JobSystem {
public:
    explicit JobSystem(std::size_t workerCount = 0);
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    void submit(std::function<void()> job);
    void waitIdle();

    [[nodiscard]] std::size_t workerCount() const noexcept { return workers_.size(); }
    [[nodiscard]] std::size_t pendingJobs() const;

private:
    void workerLoop();

    mutable std::mutex mutex_;
    std::condition_variable workAvailable_;
    std::condition_variable idle_;
    std::queue<std::function<void()>> queue_;
    std::vector<std::thread> workers_;
    std::size_t activeJobs_{0};
    bool stopping_{false};
};

} // namespace rf::core::jobs
