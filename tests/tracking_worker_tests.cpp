#include "core/tracking_worker.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <future>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {
std::atomic<int> failures{0};
void check(bool ok, const char *message)
{
    if (!ok) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

church::VideoFrame frame(std::int64_t timestamp_ns)
{
    return {{1, 2, 3, 4}, 1, 1, 4, church::PixelFormat::rgba8, timestamp_ns};
}

bool wait_for_timestamp(church::TrackingWorker &worker, std::int64_t timestamp_ns)
{
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < deadline) {
        auto state = worker.snapshot();
        if (state && state->timestamp_ns >= timestamp_ns)
            return true;
        std::this_thread::sleep_for(1ms);
    }
    return false;
}

bool wait_for_tracking(church::TrackingWorker &worker)
{
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < deadline) {
        auto state = worker.snapshot();
        if (state && state->tracking)
            return true;
        std::this_thread::sleep_for(1ms);
    }
    return false;
}

void worker_publishes_synthetic_crop_state_from_submitted_frames()
{
    church::TrackingWorker worker;
    worker.start();
    check(worker.submit(frame(900000000000LL)), "first synthetic frame submitted");
    check(wait_for_tracking(worker), "first synthetic state published");
    auto first = worker.snapshot();
    check(first && first->tracking && first->timestamp_ns == 0,
          "arbitrary input origin becomes relative zero");
    check(worker.submit(frame(905000000000LL)), "later synthetic frame submitted");
    check(wait_for_timestamp(worker, 5000000000LL), "later state published");
    auto later = worker.snapshot();
    check(later && later->tracking && later->timestamp_ns == 5000000000LL,
          "full elapsed time reaches freshness clock");
    check(later && later->zoom <= first->zoom + 0.5001f,
          "large gap uses capped motion step");
    worker.stop();
}

struct Gate {
    std::mutex mutex;
    std::condition_variable cv;
    bool entered = false;
    bool release = false;
};

class DelayedDetector final : public church::Detector {
public:
    explicit DelayedDetector(std::shared_ptr<Gate> gate) : gate_(std::move(gate)) {}
    std::vector<church::TargetObservation> detect(const church::VideoFrame &f) override
    {
        std::unique_lock lock(gate_->mutex);
        gate_->entered = true;
        gate_->cv.notify_all();
        gate_->cv.wait(lock, [&] { return gate_->release; });
        return {{{0.45f, 0.45f, 0.55f, 0.55f}, 1.0f, f.timestamp_ns}};
    }
private:
    std::shared_ptr<Gate> gate_;
};

class StaleDetector final : public church::Detector {
public:
    std::vector<church::TargetObservation> detect(const church::VideoFrame &) override
    {
        return {{{0.45f, 0.45f, 0.55f, 0.55f}, 1.0f, 0}};
    }
};

class OneTargetDetector final : public church::Detector {
public:
    std::vector<church::TargetObservation> detect(const church::VideoFrame &frame) override
    {
        if (calls_++ == 0)
            return {{{0.45f, 0.45f, 0.55f, 0.55f}, 1.0f, frame.timestamp_ns}};
        return {};
    }
private:
    int calls_ = 0;
};

void worker_tracker_holds_target_between_detections_then_resets_on_loss()
{
    church::TrackingWorker worker(std::make_unique<OneTargetDetector>());
    worker.start();
    worker.submit(frame(10));
    check(wait_for_tracking(worker), "detector supplied first target");
    for (int i = 1; i <= 4; ++i) {
        worker.submit(frame(10 + i * 1000000000LL));
        check(wait_for_timestamp(worker, i * 1000000000LL),
              "tracker processed next frame");
        auto state = worker.snapshot();
        check(state && state->tracking == (i < 3),
              "tracker holds target until detection reports loss, then resets");
    }
    worker.stop();
}

void worker_uses_full_elapsed_time_for_stale_observations()
{
    church::TrackingWorker worker(std::make_unique<StaleDetector>());
    worker.start();
    worker.submit(frame(1000000000));
    check(wait_for_tracking(worker), "initial observation accepted");
    for (int i = 1; i <= 3; ++i) {
        worker.submit(frame(1000000000LL + i * 2000000000LL));
        check(wait_for_timestamp(worker, i * 2000000000LL),
              "worker processed successive relative timestamps");
    }
    auto state = worker.snapshot();
    check(state && !state->tracking && state->timestamp_ns == 6000000000LL,
          "old detection is stale after full elapsed gap");
    worker.stop();
}

void worker_saturates_extreme_relative_timestamps()
{
    church::TrackingWorker worker;
    worker.start();
    worker.submit(frame(std::numeric_limits<std::int64_t>::min()));
    check(wait_for_tracking(worker), "extreme negative origin processed");
    worker.submit(frame(std::numeric_limits<std::int64_t>::max()));
    check(wait_for_timestamp(worker, 1000000000),
          "extreme relative timestamp advances without signed overflow");
    worker.stop();
}

void worker_drops_old_frames_when_processing_is_delayed()
{
    auto gate = std::make_shared<Gate>();
    church::TrackingWorker worker(std::make_unique<DelayedDetector>(gate));
    worker.start();
    worker.submit(frame(1000000000));
    {
        std::unique_lock lock(gate->mutex);
        check(gate->cv.wait_for(lock, 2s, [&] { return gate->entered; }),
              "detector entered delayed call");
    }
    const auto before = std::chrono::steady_clock::now();
    worker.submit(frame(2000000000));
    worker.submit(frame(3000000000));
    worker.submit(frame(4000000000));
    check(std::chrono::steady_clock::now() - before < 100ms,
          "submissions do not wait for detector");
    auto during = worker.snapshot();
    check(std::chrono::steady_clock::now() - before < 100ms,
          "snapshot does not wait for detector");
    check(during && during->timestamp_ns == 0, "neutral state remains readable");
    {
        std::lock_guard lock(gate->mutex);
        gate->release = true;
    }
    gate->cv.notify_all();
    check(wait_for_timestamp(worker, 3000000000), "newest pending frame processed");
    auto result = worker.snapshot();
    check(result && result->timestamp_ns == 3000000000,
          "intermediate frames were discarded");
    worker.stop();
}

void worker_stop_unblocks_and_joins_cleanly()
{
    auto task = std::async(std::launch::async, [] {
        for (int i = 0; i < 20; ++i) {
            church::TrackingWorker worker;
            worker.start();
            if (i % 2 == 0) {
                worker.stop();
                check(!worker.submit(frame(i)), "stopped worker rejects frames");
            }
        }
    });
    check(task.wait_for(2s) == std::future_status::ready,
          "explicit stop and scope destruction wake idle workers and join");
    task.get();

    auto gate = std::make_shared<Gate>();
    church::TrackingWorker busy(std::make_unique<DelayedDetector>(gate));
    busy.start();
    busy.submit(frame(10));
    {
        std::unique_lock lock(gate->mutex);
        check(gate->cv.wait_for(lock, 2s, [&] { return gate->entered; }),
              "busy worker entered detector");
    }
    auto stopping = std::async(std::launch::async, [&] { busy.stop(); });
    check(stopping.wait_for(10ms) == std::future_status::timeout,
          "stop waits for active detector to finish");
    {
        std::lock_guard lock(gate->mutex);
        gate->release = true;
    }
    gate->cv.notify_all();
    check(stopping.wait_for(2s) == std::future_status::ready,
          "stop joins busy worker after detector returns");
    stopping.get();
}

void worker_never_calls_graphics_or_requires_obs()
{
    church::TrackingWorker worker;
    worker.start();
    worker.submit(frame(17));
    check(wait_for_tracking(worker), "core-only worker runs from plain pixel frame");
    worker.stop();
}
} // namespace

int main()
{
    worker_publishes_synthetic_crop_state_from_submitted_frames();
    worker_tracker_holds_target_between_detections_then_resets_on_loss();
    worker_uses_full_elapsed_time_for_stale_observations();
    worker_saturates_extreme_relative_timestamps();
    worker_drops_old_frames_when_processing_is_delayed();
    worker_stop_unblocks_and_joins_cleanly();
    worker_never_calls_graphics_or_requires_obs();
    return failures.load() ? EXIT_FAILURE : EXIT_SUCCESS;
}
