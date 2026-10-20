// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/person_tracker.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace church {
namespace {

bool valid_target(const TargetObservation &target)
{
    const auto &bounds = target.bounds;
    return std::isfinite(bounds.left) && std::isfinite(bounds.top) &&
           std::isfinite(bounds.right) && std::isfinite(bounds.bottom) &&
           std::isfinite(target.confidence) && bounds.left >= 0.0f && bounds.top >= 0.0f &&
           bounds.right <= 1.0f && bounds.bottom <= 1.0f && bounds.left < bounds.right &&
           bounds.top < bounds.bottom;
}

std::uint64_t elapsed_ns(std::int64_t from, std::int64_t to)
{
    return static_cast<std::uint64_t>(to) - static_cast<std::uint64_t>(from);
}

float center_x(const NormalizedRect &bounds)
{
    return (bounds.left + bounds.right) * 0.5f;
}

float center_y(const NormalizedRect &bounds)
{
    return (bounds.top + bounds.bottom) * 0.5f;
}

} // namespace

PersonTracker::PersonTracker(std::int64_t max_prediction_ns)
    : max_prediction_ns_(std::max<std::int64_t>(0, max_prediction_ns))
{
}

void PersonTracker::reset()
{
    last_observation_.reset();
    velocity_x_ = 0.0f;
    velocity_y_ = 0.0f;
}

void PersonTracker::set_target(TargetObservation target)
{
    if (!valid_target(target)) {
        reset();
        return;
    }

    if (!last_observation_) {
        last_observation_ = target;
        velocity_x_ = 0.0f;
        velocity_y_ = 0.0f;
        return;
    }

    if (target.timestamp_ns < last_observation_->timestamp_ns)
        return;

    const auto delta_ns = elapsed_ns(last_observation_->timestamp_ns, target.timestamp_ns);
    if (delta_ns == 0) {
        last_observation_ = target;
        velocity_x_ = 0.0f;
        velocity_y_ = 0.0f;
        return;
    }

    const float seconds = static_cast<float>(static_cast<double>(delta_ns) / 1e9);
    velocity_x_ = (center_x(target.bounds) - center_x(last_observation_->bounds)) / seconds;
    velocity_y_ = (center_y(target.bounds) - center_y(last_observation_->bounds)) / seconds;
    last_observation_ = target;
}

std::optional<TargetObservation> PersonTracker::update(const VideoFrame &frame)
{
    if (!last_observation_)
        return std::nullopt;
    if (frame.timestamp_ns <= last_observation_->timestamp_ns)
        return last_observation_;

    const auto delta_ns = elapsed_ns(last_observation_->timestamp_ns, frame.timestamp_ns);
    if (delta_ns > static_cast<std::uint64_t>(max_prediction_ns_)) {
        reset();
        return std::nullopt;
    }

    const float seconds = static_cast<float>(static_cast<double>(delta_ns) / 1e9);
    const float width = last_observation_->bounds.right - last_observation_->bounds.left;
    const float height = last_observation_->bounds.bottom - last_observation_->bounds.top;
    const float half_width = width * 0.5f;
    const float half_height = height * 0.5f;
    const float predicted_x = center_x(last_observation_->bounds) + velocity_x_ * seconds;
    const float predicted_y = center_y(last_observation_->bounds) + velocity_y_ * seconds;
    const float clamped_x = std::clamp(predicted_x, half_width, 1.0f - half_width);
    const float clamped_y = std::clamp(predicted_y, half_height, 1.0f - half_height);

    auto prediction = *last_observation_;
    prediction.bounds = {clamped_x - half_width, clamped_y - half_height,
                         clamped_x + half_width, clamped_y + half_height};
    prediction.timestamp_ns = frame.timestamp_ns;
    return prediction;
}

} // namespace church
