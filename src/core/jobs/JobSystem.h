#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace rf::core::jobs {

class JobSystem {
public:
    explicit JobSystem(std::size_t workerCount = 0);
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    void submit(std::function<void()> job);

    template <typename Function>
    auto submitResult(Function&& function) -> std::future<std::invoke_result_t<Function>> {
        using Result = std::invoke_result_t<Function>;
        auto task = std::make_shared<std::packaged_task<Result()>>(std::forward<Function>(function));
        auto future = task->get_future();
        {
            std::scoped_lock lock(mutex_);
            if (stopping_) throw std::runtime_error("RuneForge JobSystem is stopping");
            queue_.push([task]() { (*task)(); });
        }
        workAvailable_.notify_one();
        return future;
    }

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
