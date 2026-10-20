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
namespace {

float intersection_over_union(const NormalizedRect &left, const NormalizedRect &right)
{
    const float overlap_left = std::max(left.left, right.left);
    const float overlap_top = std::max(left.top, right.top);
    const float overlap_right = std::min(left.right, right.right);
    const float overlap_bottom = std::min(left.bottom, right.bottom);
    const float overlap_width = std::max(0.0f, overlap_right - overlap_left);
    const float overlap_height = std::max(0.0f, overlap_bottom - overlap_top);
    const float overlap = overlap_width * overlap_height;
    const float left_area = std::max(0.0f, left.right - left.left) *
                            std::max(0.0f, left.bottom - left.top);
    const float right_area = std::max(0.0f, right.right - right.left) *
                             std::max(0.0f, right.bottom - right.top);
    const float union_area = left_area + right_area - overlap;
    return union_area > 0.0f ? overlap / union_area : 0.0f;
}

float center_distance_squared(const NormalizedRect &left, const NormalizedRect &right)
{
    const float dx = (left.left + left.right) - (right.left + right.right);
    const float dy = (left.top + left.bottom) - (right.top + right.bottom);
    return dx * dx + dy * dy;
}

std::optional<TargetObservation> select_detection(
    const std::vector<TargetObservation> &detections,
    const std::optional<TargetObservation> &current_target, const MotionEngine &motion,
    std::int64_t timestamp_ns)
{
    std::optional<TargetObservation> selected;
    if (!current_target) {
        for (const auto &detection : detections) {
            if (!motion.accepts_observation(detection, timestamp_ns))
                continue;
            if (!selected || detection.confidence > selected->confidence)
                selected = detection;
        }
        return selected;
    }

    float selected_iou = -1.0f;
    float selected_distance = std::numeric_limits<float>::max();
    for (const auto &detection : detections) {
        if (!motion.accepts_observation(detection, timestamp_ns))
            continue;
        const float iou = intersection_over_union(current_target->bounds, detection.bounds);
        const float distance = center_distance_squared(current_target->bounds, detection.bounds);
        if (iou > selected_iou + 1e-6f ||
            (std::fabs(iou - selected_iou) <= 1e-6f && distance < selected_distance)) {
            selected = detection;
            selected_iou = iou;
            selected_distance = distance;
        }
    }
    return selected;
}

} // namespace

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

bool TrackingWorker::submit(VideoFrame frame)
{
    frame.processing_epoch = invalidation_epoch_.load(std::memory_order_acquire);
    return queue_.submit(std::move(frame));
}

void TrackingWorker::invalidate()
{
    invalidation_epoch_.fetch_add(1, std::memory_order_acq_rel);
    snapshots_.publish(CropState{});
}

std::shared_ptr<const CropState> TrackingWorker::snapshot() const { return snapshots_.read(); }

bool TrackingWorker::snapshot_current() const
{
    return processed_epoch_.load(std::memory_order_acquire) ==
           invalidation_epoch_.load(std::memory_order_acquire);
}

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
        const auto frame_epoch = frame.processing_epoch;
        if (frame_epoch != invalidation_epoch_.load(std::memory_order_acquire))
            continue;
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
            target = select_detection(detections, tracker_->current_target(), motion_, relative);
            if (target) {
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
        const auto crop = motion_.update_at(target, motion_delta_seconds, relative);
        if (frame_epoch != invalidation_epoch_.load(std::memory_order_acquire))
            continue;
        snapshots_.publish(crop);
        processed_epoch_.store(frame_epoch, std::memory_order_release);
    }
}

} // namespace church
