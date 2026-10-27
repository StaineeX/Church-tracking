// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "obs/filter_state.hpp"

#include <chrono>
#include <cstdlib>
#include <atomic>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

namespace {
int failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void neutral_state_is_available_without_graphics()
{
    church::FilterState filter(true, true);
    const auto crop = filter.crop();
    check(crop.visible_region.left == 0.0f && crop.visible_region.top == 0.0f &&
              crop.visible_region.right == 1.0f && crop.visible_region.bottom == 1.0f &&
              crop.zoom == 1.0f && !crop.tracking && crop.timestamp_ns == 0,
          "new synthetic filter exposes a neutral crop before the worker publishes");
}

void disabled_filter_skips_motion_work()
{
    church::FilterState filter(false, true);
    check(!filter.active(), "disabled filter has no running motion worker");
    check(!filter.submit_synthetic(1'000'000'000),
          "disabled filter does not submit analysis work");
    const auto crop = filter.crop();
    check(crop.visible_region.left == 0.0f && crop.visible_region.right == 1.0f &&
              crop.zoom == 1.0f && !crop.tracking,
          "disabled filter returns a neutral crop");
}

void non_synthetic_filter_skips_motion_work()
{
    church::FilterState filter(true, false);
    check(!filter.active(), "camera mode has no synthetic worker");
    check(!filter.submit_synthetic(1'000'000'000),
          "camera mode does not invent a target in this milestone");
    check(!filter.crop().tracking, "camera mode stays neutral");
}

void synthetic_timestamps_drive_the_worker()
{
    church::FilterState filter(true, true);
    check(filter.submit_synthetic(1'000'000'000), "first metadata frame accepted");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!filter.crop().tracking && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    check(filter.crop().tracking, "synthetic worker publishes a tracking crop");
}

class InjectedDetector final : public church::Detector {
public:
    std::vector<church::TargetObservation> detect(const church::VideoFrame &frame) override
    {
        ++calls;
        return {{{0.2f, 0.2f, 0.8f, 0.8f}, 1.0f, frame.timestamp_ns}};
    }

    std::atomic<int> calls{0};
};

class InjectedTracker final : public church::Tracker {
public:
    void reset() override { target_.reset(); }
    void set_target(church::TargetObservation target) override { target_ = target; }
    std::optional<church::TargetObservation> update(const church::VideoFrame &frame) override
    {
        if (target_)
            target_->timestamp_ns = frame.timestamp_ns;
        return target_;
    }

private:
    std::optional<church::TargetObservation> target_;
};

void real_mode_accepts_owned_frames_and_publishes_injected_tracking()
{
    auto detector = std::make_unique<InjectedDetector>();
    auto *detector_ptr = detector.get();
    church::FilterState filter(true, false, {}, std::move(detector),
                               std::make_unique<InjectedTracker>());
    check(filter.active(), "real mode starts an asynchronous worker");

    church::VideoFrame frame{{10, 20, 30, 255}, 1, 1, 4, church::PixelFormat::rgba8,
                             1'000'000'000};
    check(filter.submit(std::move(frame)), "real mode accepts an owned video frame");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (detector_ptr->calls.load() == 0 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    check(detector_ptr->calls.load() == 1, "injected detector receives the submitted frame");
    while (!filter.crop().tracking && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    check(filter.crop().tracking, "real mode publishes a tracking crop from owned pixels");
}

void invalid_real_frame_clears_the_last_tracking_crop()
{
    auto detector = std::make_unique<InjectedDetector>();
    church::FilterState filter(true, false, {}, std::move(detector),
                               std::make_unique<InjectedTracker>());
    church::VideoFrame frame{{10, 20, 30, 255}, 1, 1, 4, church::PixelFormat::rgba8,
                             1'000'000'000};
    check(filter.submit(std::move(frame)), "real mode accepts a frame before invalidation");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!filter.crop().tracking && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    check(filter.crop().tracking, "tracking crop exists before invalidation");

    filter.invalidate();
    const auto crop = filter.crop();
    check(crop.visible_region.left == 0.0f && crop.visible_region.top == 0.0f &&
              crop.visible_region.right == 1.0f && crop.visible_region.bottom == 1.0f &&
              crop.zoom == 1.0f && !crop.tracking,
          "invalid real frame clears the last tracking crop to neutral");
}
} // namespace

int main()
{
    neutral_state_is_available_without_graphics();
    disabled_filter_skips_motion_work();
    non_synthetic_filter_skips_motion_work();
    synthetic_timestamps_drive_the_worker();
    real_mode_accepts_owned_frames_and_publishes_injected_tracking();
    invalid_real_frame_clears_the_last_tracking_crop();
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
