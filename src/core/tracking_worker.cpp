// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/tracking_worker.hpp"

#include "core/synthetic_detector.hpp"
#include "core/synthetic_tracker.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

namespace church {

TrackingWorker::TrackingWorker(std::unique_ptr<Detector> detector,
                               std::unique_ptr<Tracker> tracker, MotionSettings settings)
    : detector_(detector ? std::move(detector) : std::make_unique<SyntheticDetector>()),
      tracker_(tracker ? std::move(tracker) : std::make_unique<SyntheticTracker>()),
      motion_(settings)
{
    snapshots_.publish(motion_.state());
}

TrackingWorker::~TrackingWorker() { stop(); }

void TrackingWorker::start()
{
    if (!thread_.joinable())
        thread_ = std::thread(&TrackingWorker::run, this);
}

bool TrackingWorker::submit(VideoFrame frame) { return queue_.submit(std::move(frame)); }

std::shared_ptr<const CropState> TrackingWorker::snapshot() const { return snapshots_.read(); }

void TrackingWorker::stop()
{
    queue_.close();
    if (thread_.joinable())
        thread_.join();
}

void TrackingWorker::run()
{
    std::optional<std::int64_t> origin;
    std::int64_t previous = 0;
    unsigned processed = 0;
    VideoFrame frame;
    while (queue_.wait_take(frame)) {
        if (!origin)
            origin = frame.timestamp_ns;
        // A late, out-of-order frame cannot turn the motion clock backward.
        const auto elapsed = static_cast<std::uint64_t>(frame.timestamp_ns) -
                             static_cast<std::uint64_t>(*origin);
        const std::int64_t relative = frame.timestamp_ns < *origin ? 0 :
            elapsed > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
                ? std::numeric_limits<std::int64_t>::max()
                : static_cast<std::int64_t>(elapsed);
        if (relative < previous)
            continue;
        frame.timestamp_ns = relative;
        std::optional<TargetObservation> target;
        if (processed++ % 3 == 0) {
            auto detections = detector_->detect(frame);
            if (detections.empty()) {
                tracker_->reset();
            } else if (motion_.accepts_observation(detections.front(), relative)) {
                target = detections.front();
                tracker_->set_target(*target);
            } else {
                tracker_->reset();
            }
        } else {
            target = tracker_->update(frame);
        }
        const float motion_delta_seconds = static_cast<float>(std::min(
            static_cast<double>(relative - previous) / 1e9, 1.0));
        previous = relative;
        snapshots_.publish(motion_.update_at(target, motion_delta_seconds, relative));
    }
}

} // namespace church
