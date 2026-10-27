#include "core/tracking_worker.hpp"
#include "core/person_tracker.hpp"

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
#include <stdexcept>
#include <thread>
#include <utility>
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

bool submit_until_accepted(church::TrackingWorker &worker, const church::VideoFrame &source)
{
    for (int attempt = 0; attempt < 1000; ++attempt) {
        auto frame_attempt = source;
        if (worker.submit(std::move(frame_attempt)))
            return true;
        std::this_thread::yield();
    }
    return false;
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
    check(submit_until_accepted(worker, frame(900000000000LL)), "first synthetic frame submitted");
    check(wait_for_tracking(worker), "first synthetic state published");
    auto first = worker.snapshot();
    check(first && first->tracking && first->timestamp_ns == 0,
          "arbitrary input origin becomes relative zero");
    check(submit_until_accepted(worker, frame(905000000000LL)), "later synthetic frame submitted");
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

class ResetProbeDetector final : public church::Detector {
public:
    std::vector<church::TargetObservation> detect(const church::VideoFrame &frame) override
    {
        ++calls;
        return {{{0.80f, 0.40f, 0.90f, 0.60f}, 1.0f, frame.timestamp_ns}};
    }

    std::atomic<int> calls{0};
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
    auto detector = std::make_unique<ResetProbeDetector>();
    auto *detector_ptr = detector.get();
    church::TrackingWorker worker(std::move(detector), std::make_unique<CarryingTracker>());
    std::atomic<bool> arm_before_publish{false};
    std::atomic<int> hook_calls{0};
    std::atomic<int> hook_invalidations{0};
    std::mutex hook_mutex;
    std::condition_variable hook_cv;
    bool first_hook_entered = false;
    bool second_hook_entered = false;
    bool release_first_hook = false;
    bool release_second_hook = false;
    worker.set_before_publish_callback_for_testing([&] {
        if (!arm_before_publish.load(std::memory_order_acquire))
            return;
        const int hook_call = hook_calls.fetch_add(1, std::memory_order_acq_rel) + 1;
        std::unique_lock lock(hook_mutex);
        if (hook_call == 1) {
            lock.unlock();
            worker.invalidate();
            hook_invalidations.fetch_add(1, std::memory_order_release);
            lock.lock();
            first_hook_entered = true;
            hook_cv.notify_all();
            hook_cv.wait(lock, [&] { return release_first_hook; });
        } else if (hook_call == 2) {
            second_hook_entered = true;
            hook_cv.notify_all();
            hook_cv.wait(lock, [&] { return release_second_hook; });
        }
    });
    worker.start();
    check(submit_until_accepted(worker, frame(0)), "old epoch frame submitted");
    check(wait_for_tracking(worker), "old epoch starts tracking");
    check(submit_until_accepted(worker, frame(1000000000)), "old epoch clock frame submitted");
    check(wait_for_timestamp(worker, 1000000000), "old epoch advances the motion clock");
    check(submit_until_accepted(worker, frame(2000000000)), "old epoch crop frame submitted");
    check(wait_for_timestamp(worker, 2000000000), "old epoch builds a tracking crop");
    const auto old_crop = worker.snapshot();
    check(old_crop && old_crop->tracking && old_crop->zoom > 1.0f,
          "old epoch leaves non-neutral motion state");

    arm_before_publish.store(true, std::memory_order_release);
    check(submit_until_accepted(worker, frame(3000000000)),
          "in-flight invalidation frame submitted");
    {
        std::unique_lock lock(hook_mutex);
        check(hook_cv.wait_for(lock, 2s, [&] { return first_hook_entered; }),
              "before-publish hook runs after the ordinary epoch check");
    }

    const auto invalidated_crop = worker.snapshot();
    check(invalidated_crop && !invalidated_crop->tracking && invalidated_crop->zoom == 1.0f &&
              invalidated_crop->visible_region.left == 0.0f &&
              invalidated_crop->visible_region.top == 0.0f &&
              invalidated_crop->visible_region.right == 1.0f &&
              invalidated_crop->visible_region.bottom == 1.0f,
          "before-publish invalidation leaves a neutral snapshot");
    check(hook_invalidations.load(std::memory_order_acquire) == 1,
          "before-publish hook invalidates exactly once");

    check(submit_until_accepted(worker, frame(4000000000)),
          "new epoch frame queues while publication is paused");
    {
        std::lock_guard lock(hook_mutex);
        release_first_hook = true;
    }
    hook_cv.notify_all();

    {
        std::unique_lock lock(hook_mutex);
        check(hook_cv.wait_for(lock, 2s, [&] { return second_hook_entered; }),
              "worker reaches the next publication hook");
    }
    const auto post_hook_crop = worker.snapshot();
    check(post_hook_crop && !post_hook_crop->tracking && post_hook_crop->zoom == 1.0f &&
              post_hook_crop->visible_region.left == 0.0f &&
              post_hook_crop->visible_region.top == 0.0f &&
              post_hook_crop->visible_region.right == 1.0f &&
              post_hook_crop->visible_region.bottom == 1.0f,
          "snapshot remains neutral after invalidation and before the next publish");
    check(hook_invalidations.load(std::memory_order_acquire) == 1,
          "before-publish hook invalidates exactly once");
    {
        std::lock_guard lock(hook_mutex);
        release_second_hook = true;
    }
    hook_cv.notify_all();

    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (detector_ptr->calls.load() < 3 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
    check(detector_ptr->calls.load() == 3, "new epoch starts with a fresh detector cadence");
    bool fresh_epoch_published = false;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto fresh_crop = worker.snapshot();
        if (fresh_crop && fresh_crop->tracking && fresh_crop->timestamp_ns == 0) {
            fresh_epoch_published = true;
            break;
        }
        std::this_thread::sleep_for(1ms);
    }
    check(fresh_epoch_published,
          "subsequent valid work starts from reset state instead of stale crop state");
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
    church::TrackingWorker worker(std::make_unique<OneTargetDetector>(),
                                  std::make_unique<church::PersonTracker>());
    worker.start();
    check(submit_until_accepted(worker, frame(10)), "prediction seed frame submitted");
    check(wait_for_tracking(worker), "detector supplied first target");
    for (int i = 1; i <= 6; ++i) {
        check(submit_until_accepted(worker, frame(10 + i * 100000000LL)),
              "prediction frame submitted");
        check(wait_for_timestamp(worker, i * 100000000LL),
              "tracker processed next frame");
        auto state = worker.snapshot();
        check(state && state->tracking == (i < 6),
              "tracker predicts through a missed pass, then resets after its horizon");
    }
    worker.stop();
}

void worker_preserves_exact_ordinary_frame_timestamps()
{
    church::TrackingWorker worker;
    worker.start();
    constexpr std::int64_t frame_period_ns = 33333333;
    check(submit_until_accepted(worker, frame(0)), "ordinary origin frame submitted");
    check(wait_for_tracking(worker), "first ordinary cadence frame processed");
    for (int frame_number = 1; frame_number < 5; ++frame_number) {
        check(submit_until_accepted(worker, frame(frame_number * frame_period_ns)),
              "ordinary cadence frame submitted");
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

class CrossingDetector final : public church::Detector {
public:
    std::vector<church::TargetObservation> detect(const church::VideoFrame &frame) override
    {
        switch (calls_++) {
        case 0:
            return {{{0.15f, 0.2f, 0.25f, 0.8f}, 0.80f, frame.timestamp_ns},
                    {{0.75f, 0.2f, 0.85f, 0.8f}, 0.60f, frame.timestamp_ns}};
        case 1:
            return {{{0.35f, 0.2f, 0.45f, 0.8f}, 0.70f, frame.timestamp_ns},
                    {{0.55f, 0.2f, 0.65f, 0.8f}, 0.99f, frame.timestamp_ns}};
        default:
            return {{{0.55f, 0.2f, 0.65f, 0.8f}, 0.70f, frame.timestamp_ns},
                    {{0.35f, 0.2f, 0.45f, 0.8f}, 0.99f, frame.timestamp_ns}};
        }
    }

private:
    int calls_ = 0;
};

class RecordingPersonTracker final : public church::Tracker {
public:
    void reset() override { tracker_.reset(); }
    void set_target(church::TargetObservation target) override
    {
        selected.push_back(target);
        tracker_.set_target(target);
    }
    std::optional<church::TargetObservation> current_target() const override
    {
        return tracker_.current_target();
    }
    std::optional<church::TargetObservation> predict(std::int64_t timestamp_ns) const override
    {
        return tracker_.predict(timestamp_ns);
    }
    std::optional<church::TargetObservation> update(const church::VideoFrame &frame) override
    {
        return tracker_.update(frame);
    }

    std::vector<church::TargetObservation> selected;

private:
    church::PersonTracker tracker_;
};

void worker_uses_predicted_target_when_people_cross()
{
    auto tracker = std::make_unique<RecordingPersonTracker>();
    auto *tracker_ptr = tracker.get();
    church::TrackingWorker worker(std::make_unique<CrossingDetector>(), std::move(tracker));
    worker.start();
    check(submit_until_accepted(worker, frame(0)), "crossing origin frame submitted");
    check(wait_for_tracking(worker), "crossing detector supplies the initial target");
    for (int frame_number = 1; frame_number <= 6; ++frame_number) {
        check(submit_until_accepted(worker, frame(frame_number * 100'000'000LL)),
              "crossing frame submitted");
        check(wait_for_timestamp(worker, frame_number * 100'000'000LL),
              "crossing sequence advances to the next frame");
    }
    check(tracker_ptr->selected.size() == 3, "crossing sequence reaches all detector passes");
    check(tracker_ptr->selected.size() == 3 && tracker_ptr->selected[2].bounds.left > 0.5f,
          "predicted current bounds keep the original person after the crossing");
    worker.stop();
}

class ThrowingDetector final : public church::Detector {
public:
    std::vector<church::TargetObservation> detect(const church::VideoFrame &frame) override
    {
        const int call = calls.fetch_add(1);
        if (call == 1)
            throw std::runtime_error("detector failure");
        if (call == 2)
            throw 7;
        return {{{0.45f, 0.45f, 0.55f, 0.55f}, 1.0f, frame.timestamp_ns}};
    }

    std::atomic<int> calls{0};
};

void worker_recovers_from_detector_exceptions()
{
    auto detector = std::make_unique<ThrowingDetector>();
    auto *detector_ptr = detector.get();
    church::TrackingWorker worker(std::move(detector),
                                  std::make_unique<church::PersonTracker>());
    worker.start();
    check(submit_until_accepted(worker, frame(0)), "detector seed frame submitted");
    check(wait_for_tracking(worker), "detector establishes tracking before failure");
    for (int frame_number = 1; frame_number <= 3; ++frame_number) {
        check(submit_until_accepted(worker, frame(frame_number * 100'000'000LL)),
              "detector failure frame submitted");
        check(wait_for_timestamp(worker, frame_number * 100'000'000LL),
              "worker advances through detector exception");
    }
    check(detector_ptr->calls.load() == 2, "standard detector exception was invoked");
    const auto after_standard_failure = worker.snapshot();
    check(after_standard_failure && !after_standard_failure->tracking &&
              after_standard_failure->zoom == 1.0f &&
              after_standard_failure->visible_region.left == 0.0f &&
              after_standard_failure->visible_region.top == 0.0f &&
              after_standard_failure->visible_region.right == 1.0f &&
              after_standard_failure->visible_region.bottom == 1.0f,
          "detector failure clears the predicted crop to neutral state");
    for (int frame_number = 4; frame_number <= 6; ++frame_number) {
        check(submit_until_accepted(worker, frame(frame_number * 100'000'000LL)),
              "unknown detector failure frame submitted");
        check(wait_for_timestamp(worker, frame_number * 100'000'000LL),
              "worker continues after unknown detector exception");
    }
    check(detector_ptr->calls.load() == 3, "unknown detector exception was invoked");
    for (int frame_number = 7; frame_number <= 9; ++frame_number) {
        check(submit_until_accepted(worker, frame(frame_number * 100'000'000LL)),
              "detector recovery frame submitted");
        check(wait_for_timestamp(worker, frame_number * 100'000'000LL),
              "worker continues toward a valid detector frame");
    }
    check(detector_ptr->calls.load() == 4, "worker reaches a later valid detector pass");
    check(worker.snapshot()->tracking, "worker resumes tracking after detector exceptions");
    worker.stop();
}

class ThrowingTracker final : public church::Tracker {
public:
    explicit ThrowingTracker(bool unknown_exception) : unknown_exception_(unknown_exception) {}

    void reset() override
    {
        ++reset_calls;
        target_.reset();
    }

    void set_target(church::TargetObservation target) override { target_ = target; }
    std::optional<church::TargetObservation> current_target() const override { return target_; }

    std::optional<church::TargetObservation> update(const church::VideoFrame &frame) override
    {
        if (update_calls_++ == 0) {
            if (unknown_exception_)
                throw 7;
            throw std::runtime_error("tracker failure");
        }
        if (target_)
            target_->timestamp_ns = frame.timestamp_ns;
        return target_;
    }

    int reset_calls = 0;

private:
    bool unknown_exception_;
    int update_calls_ = 0;
    std::optional<church::TargetObservation> target_;
};

void worker_recovers_from_tracker_exception(bool unknown_exception)
{
    auto tracker = std::make_unique<ThrowingTracker>(unknown_exception);
    auto *tracker_ptr = tracker.get();
    church::TrackingWorker worker(std::make_unique<StaleDetector>(), std::move(tracker));
    worker.start();
    check(submit_until_accepted(worker, frame(0)), "tracker seed frame submitted");
    check(wait_for_tracking(worker), "tracker establishes tracking before failure");
    check(submit_until_accepted(worker, frame(100'000'000)), "tracker failure frame submitted");
    check(wait_for_timestamp(worker, 100'000'000), "worker reaches throwing tracker update");
    const auto after_failure = worker.snapshot();
    check(after_failure && !after_failure->tracking && after_failure->zoom == 1.0f,
          unknown_exception ? "unknown tracker failure clears tracking"
                            : "standard tracker failure clears tracking");
    check(submit_until_accepted(worker, frame(200'000'000)), "tracker recovery frame submitted");
    check(wait_for_timestamp(worker, 200'000'000), "worker continues after tracker failure");
    check(submit_until_accepted(worker, frame(300'000'000)), "tracker re-detection frame submitted");
    check(wait_for_tracking(worker), "worker recovers on a later valid tracker frame");
    check(tracker_ptr->reset_calls > 0,
          unknown_exception ? "unknown tracker failure invokes reset"
                            : "standard tracker failure invokes reset");
    worker.stop();
}

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

class DistantDistractorDetector final : public church::Detector {
public:
    std::vector<church::TargetObservation> detect(const church::VideoFrame &frame) override
    {
        if (calls_++ == 0)
            return {{{0.20f, 0.20f, 0.40f, 0.40f}, 0.70f, frame.timestamp_ns}};
        return {{{0.75f, 0.20f, 0.95f, 0.40f}, 0.99f, frame.timestamp_ns}};
    }

private:
    int calls_ = 0;
};

void worker_rejects_a_distant_higher_confidence_distractor()
{
    auto tracker = std::make_unique<RecordingTracker>();
    auto *tracker_ptr = tracker.get();
    church::TrackingWorker worker(std::make_unique<DistantDistractorDetector>(),
                                  std::move(tracker));
    worker.start();
    check(submit_until_accepted(worker, frame(0)), "target lock origin frame submitted");
    check(wait_for_tracking(worker), "target lock acquires the first person");
    for (int frame_number = 1; frame_number <= 3; ++frame_number) {
        check(submit_until_accepted(worker, frame(frame_number)),
              "target lock follow-up frame submitted");
        check(wait_for_timestamp(worker, frame_number),
              "target lock sequence advances to the next frame");
    }
    check(tracker_ptr->selected.size() == 1,
          "distant higher-confidence person cannot replace the locked target");
    worker.stop();
}

void worker_associates_detection_with_existing_target()
{
    auto tracker = std::make_unique<RecordingTracker>();
    auto *tracker_ptr = tracker.get();
    church::TrackingWorker worker(std::make_unique<AssociationDetector>(), std::move(tracker));
    worker.start();
    check(submit_until_accepted(worker, frame(0)), "association origin frame submitted");
    check(wait_for_tracking(worker), "initial association target published");
    for (int frame_number = 1; frame_number <= 3; ++frame_number) {
        check(submit_until_accepted(worker, frame(frame_number)), "association frame submitted");
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
    check(submit_until_accepted(worker, frame(1000000000)), "future origin frame submitted");
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (detector_ptr->calls.load() == 0 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
    check(detector_ptr->calls.load() == 1, "future detection frame reached detector");
    std::this_thread::sleep_for(1ms);
    check(submit_until_accepted(worker, frame(1033333333)), "future follow-up frame submitted");
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
    check(submit_until_accepted(worker, frame(1000000000)), "stale origin frame submitted");
    check(wait_for_tracking(worker), "initial observation accepted");
    for (int i = 1; i <= 3; ++i) {
        check(submit_until_accepted(worker, frame(1000000000LL + i * 2000000000LL)),
              "stale observation frame submitted");
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
    check(submit_until_accepted(worker, frame(std::numeric_limits<std::int64_t>::min())),
          "extreme origin frame submitted");
    check(wait_for_tracking(worker), "extreme negative origin processed");
    check(submit_until_accepted(worker, frame(std::numeric_limits<std::int64_t>::max())),
          "extreme follow-up frame submitted");
    check(wait_for_timestamp(worker, 1000000000),
          "extreme relative timestamp advances without signed overflow");
    worker.stop();
}

void worker_drops_old_frames_when_processing_is_delayed()
{
    auto gate = std::make_shared<Gate>();
    church::TrackingWorker worker(std::make_unique<DelayedDetector>(gate));
    worker.start();
    check(submit_until_accepted(worker, frame(1000000000)), "delayed origin frame submitted");
    {
        std::unique_lock lock(gate->mutex);
        check(gate->cv.wait_for(lock, 2s, [&] { return gate->entered; }),
              "detector entered delayed call");
    }
    const auto before = std::chrono::steady_clock::now();
    check(submit_until_accepted(worker, frame(2000000000)), "delayed frame 2 submitted");
    check(submit_until_accepted(worker, frame(3000000000)), "delayed frame 3 submitted");
    check(submit_until_accepted(worker, frame(4000000000)), "delayed frame 4 submitted");
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
    check(submit_until_accepted(busy, frame(10)), "busy worker frame submitted");
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
    check(submit_until_accepted(worker, frame(17)), "core-only frame submitted");
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
    worker_uses_predicted_target_when_people_cross();
    worker_rejects_a_distant_higher_confidence_distractor();
    worker_recovers_from_detector_exceptions();
    worker_recovers_from_tracker_exception(false);
    worker_recovers_from_tracker_exception(true);
    worker_uses_full_elapsed_time_for_stale_observations();
    worker_saturates_extreme_relative_timestamps();
    worker_drops_old_frames_when_processing_is_delayed();
    worker_stop_unblocks_and_joins_cleanly();
    worker_never_calls_graphics_or_requires_obs();
    return failures.load() ? EXIT_FAILURE : EXIT_SUCCESS;
}
