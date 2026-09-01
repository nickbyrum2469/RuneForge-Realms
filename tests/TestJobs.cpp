#include "TestSuites.h"

#include "core/jobs/JobSystem.h"

#include <atomic>
#include <cassert>

void runJobTests() {
    std::atomic<int> completed{0};
    rf::core::jobs::JobSystem jobs(2);

    for (int i = 0; i < 64; ++i) {
        jobs.submit([&completed] { completed.fetch_add(1); });
    }

    auto typed = jobs.submitResult([] { return 6 * 7; });
    assert(typed.get() == 42);
    jobs.waitIdle();
    assert(jobs.pendingJobs() == 0);
    assert(completed.load() == 64);
}
