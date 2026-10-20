#include "core/tracking_worker.hpp"

#include <atomic>
#include <barrier>
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

bool wait_for_exact_timestamp(church::TrackingWorker &worker, std::int64_t timestamp_ns)
{
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < deadline) {
        auto state = worker.snapshot();
        if (state && state->timestamp_ns >= timestamp_ns)
            return state->timestamp_ns == timestamp_ns;
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

class InFlightEpochDetector final : public church::Detector {
public:
    explicit InFlightEpochDetector(std::shared_ptr<Gate> gate) : gate_(std::move(gate)) {}

    std::vector<church::TargetObservation> detect(const church::VideoFrame &frame) override
    {
        const int call = ++calls;
        if (call == 2) {
            std::unique_lock lock(gate_->mutex);
            gate_->entered = true;
            gate_->cv.notify_all();
            gate_->cv.wait(lock, [&] { return gate_->release; });
            return {{{0.80f, 0.40f, 0.90f, 0.60f}, 1.0f, frame.timestamp_ns}};
        }
        if (call == 1)
            return {{{0.80f, 0.40f, 0.90f, 0.60f}, 1.0f, frame.timestamp_ns}};
        return {};
    }

    std::atomic<int> calls{0};

private:
    std::shared_ptr<Gate> gate_;
};

class CarryingTracker final : public church::Tracker {
public:
    void reset() override { target_.reset(); }
    void set_target(church::TargetObservation target) override { target_ = target; }
    std::optional<church::TargetObservation> current_target() const override { return target_; }
    std::optional<church::TargetObservation> update(const church::VideoFrame &frame) override
    {
        if (target_)
            target_->timestamp_ns = frame.timestamp_ns;
        return target_;
    }

private:
    std::optional<church::TargetObservation> target_;
};

void worker_resets_state_before_accepting_frame_after_inflight_invalidation()
{
    auto gate = std::make_shared<Gate>();
    auto detector = std::make_unique<InFlightEpochDetector>(gate);
    auto *detector_ptr = detector.get();
    church::TrackingWorker worker(std::move(detector), std::make_unique<CarryingTracker>());
    worker.start();
    worker.submit(frame(0));
    check(wait_for_tracking(worker), "old epoch starts tracking");
    worker.submit(frame(1000000000));
    check(wait_for_timestamp(worker, 1000000000), "old epoch advances the motion clock");
    worker.submit(frame(2000000000));
    check(wait_for_timestamp(worker, 2000000000), "old epoch builds a tracking crop");
    const auto old_crop = worker.snapshot();
    check(old_crop && old_crop->tracking && old_crop->zoom > 1.0f,
          "old epoch leaves non-neutral motion state");

    worker.submit(frame(3000000000));
    {
        std::unique_lock lock(gate->mutex);
        check(gate->cv.wait_for(lock, 2s, [&] { return gate->entered; }),
              "detector blocks inside the old epoch");
    }
    std::atomic<bool> invalidated{false};
    std::atomic<bool> stale_after_invalidation{false};
    std::atomic<bool> stop_reader{false};
    std::thread reader([&] {
        while (!stop_reader.load(std::memory_order_acquire)) {
            if (!invalidated.load(std::memory_order_acquire))
                continue;
            const auto state = worker.snapshot();
            if (state && state->tracking)
                stale_after_invalidation.store(true, std::memory_order_release);
        }
    });
    std::barrier start_race(3);
    std::thread release([&] {
        start_race.arrive_and_wait();
        {
            std::lock_guard lock(gate->mutex);
            gate->release = true;
        }
        gate->cv.notify_all();
    });
    std::thread invalidate([&] {
        start_race.arrive_and_wait();
        worker.invalidate();
        invalidated.store(true, std::memory_order_release);
    });
    start_race.arrive_and_wait();
    release.join();
    invalidate.join();

    const auto invalidated_crop = worker.snapshot();
    check(invalidated_crop && !invalidated_crop->tracking && invalidated_crop->zoom == 1.0f &&
              invalidated_crop->visible_region.left == 0.0f &&
              invalidated_crop->visible_region.top == 0.0f &&
              invalidated_crop->visible_region.right == 1.0f &&
              invalidated_crop->visible_region.bottom == 1.0f,
          "blocking invalidation publishes a neutral snapshot immediately");
    worker.submit(frame(4000000000));

    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (detector_ptr->calls.load() < 3 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
    stop_reader.store(true, std::memory_order_release);
    reader.join();
    check(!stale_after_invalidation.load(std::memory_order_acquire),
          "stale work cannot be returned after blocking invalidation");
    const auto fresh_crop = worker.snapshot();
    check(detector_ptr->calls.load() == 3, "new epoch starts with a fresh detector cadence");
    check(fresh_crop && !fresh_crop->tracking && fresh_crop->zoom == 1.0f &&
              fresh_crop->visible_region.left == 0.0f &&
              fresh_crop->visible_region.top == 0.0f &&
              fresh_crop->visible_region.right == 1.0f &&
              fresh_crop->visible_region.bottom == 1.0f,
          "frame after invalidation starts from neutral state");
    worker.stop();
}

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

void worker_preserves_exact_ordinary_frame_timestamps()
{
    church::TrackingWorker worker;
    worker.start();
    constexpr std::int64_t frame_period_ns = 33333333;
    worker.submit(frame(0));
    check(wait_for_tracking(worker), "first ordinary cadence frame processed");
    for (int frame_number = 1; frame_number < 5; ++frame_number) {
        worker.submit(frame(frame_number * frame_period_ns));
        check(wait_for_exact_timestamp(worker, frame_number * frame_period_ns),
              "ordinary frame cadence keeps exact nanosecond timestamp");
    }
    worker.stop();
}

class FutureDetector final : public church::Detector {
public:
    std::vector<church::TargetObservation> detect(const church::VideoFrame &frame) override
    {
        ++calls;
        return {{{0.45f, 0.45f, 0.55f, 0.55f}, 1.0f, frame.timestamp_ns + 1}};
    }

    std::atomic<int> calls{0};
};

class AssociationDetector final : public church::Detector {
public:
    std::vector<church::TargetObservation> detect(const church::VideoFrame &frame) override
    {
        if (calls_++ == 0)
            return {{{0.20f, 0.20f, 0.40f, 0.40f}, 0.80f, frame.timestamp_ns},
                    {{0.70f, 0.20f, 0.90f, 0.40f}, 0.60f, frame.timestamp_ns}};
        return {{{0.22f, 0.20f, 0.42f, 0.40f}, 0.70f, frame.timestamp_ns},
                {{0.70f, 0.20f, 0.90f, 0.40f}, 0.99f, frame.timestamp_ns}};
    }

private:
    int calls_ = 0;
};

class RecordingTracker final : public church::Tracker {
public:
    void reset() override { target_.reset(); }
    void set_target(church::TargetObservation target) override
    {
        target_ = target;
        selected.push_back(target);
    }
    std::optional<church::TargetObservation> current_target() const override { return target_; }
    std::optional<church::TargetObservation> update(const church::VideoFrame &frame) override
    {
        if (target_)
            target_->timestamp_ns = frame.timestamp_ns;
        return target_;
    }

    std::vector<church::TargetObservation> selected;

private:
    std::optional<church::TargetObservation> target_;
};

void worker_associates_detection_with_existing_target()
{
    auto tracker = std::make_unique<RecordingTracker>();
    auto *tracker_ptr = tracker.get();
    church::TrackingWorker worker(std::make_unique<AssociationDetector>(), std::move(tracker));
    worker.start();
    worker.submit(frame(0));
    check(wait_for_tracking(worker), "initial association target published");
    for (int frame_number = 1; frame_number <= 3; ++frame_number) {
        worker.submit(frame(frame_number));
        check(wait_for_timestamp(worker, frame_number), "association cadence frame processed");
    }
    check(tracker_ptr->selected.size() == 2, "both detection results reached the tracker");
    check(tracker_ptr->selected.size() == 2 && tracker_ptr->selected[1].bounds.left < 0.3f,
          "existing target remains selected when a distractor becomes higher confidence");
    worker.stop();
}

void worker_does_not_seed_tracker_from_future_detection()
{
    auto detector = std::make_unique<FutureDetector>();
    auto *detector_ptr = detector.get();
    church::TrackingWorker worker(std::move(detector));
    worker.start();
    worker.submit(frame(1000000000));
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (detector_ptr->calls.load() == 0 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
    check(detector_ptr->calls.load() == 1, "future detection frame reached detector");
    std::this_thread::sleep_for(1ms);
    worker.submit(frame(1033333333));
    check(wait_for_exact_timestamp(worker, 33333333), "following frame processed");
    auto state = worker.snapshot();
    check(state && !state->tracking,
          "future detection is filtered before tracker seeding");
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
    worker_resets_state_before_accepting_frame_after_inflight_invalidation();
    worker_tracker_holds_target_between_detections_then_resets_on_loss();
    worker_preserves_exact_ordinary_frame_timestamps();
    worker_does_not_seed_tracker_from_future_detection();
    worker_associates_detection_with_existing_target();
    worker_uses_full_elapsed_time_for_stale_observations();
    worker_saturates_extreme_relative_timestamps();
    worker_drops_old_frames_when_processing_is_delayed();
    worker_stop_unblocks_and_joins_cleanly();
    worker_never_calls_graphics_or_requires_obs();
    return failures.load() ? EXIT_FAILURE : EXIT_SUCCESS;
}
