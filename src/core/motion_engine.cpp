// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/motion_engine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace church {
namespace {

float move_toward(float current, float desired, float limit)
{
    return current + std::clamp(desired - current, -limit, limit);
}

float center_x(const NormalizedRect &r) { return (r.left + r.right) * 0.5f; }
float center_y(const NormalizedRect &r) { return (r.top + r.bottom) * 0.5f; }

bool valid_settings(const MotionSettings &s)
{
    return is_valid(s.dead_zone) && is_valid(s.visible_region) &&
           s.min_zoom == 1.0f && std::isfinite(s.max_zoom) && s.max_zoom >= s.min_zoom &&
           std::isfinite(s.max_pan_speed) && s.max_pan_speed >= 0.0f &&
           std::isfinite(s.zoom_speed) && s.zoom_speed >= 0.0f &&
           std::isfinite(s.confidence_threshold) && s.confidence_threshold >= 0.0f &&
           s.confidence_threshold <= 1.0f && std::isfinite(s.stale_timeout) &&
           s.stale_timeout >= 0.0f;
}

float representable_max_zoom()
{
    return 1.0f / (8.0f * std::numeric_limits<float>::epsilon());
}

MotionSettings sanitize_settings(MotionSettings settings)
{
    if (!valid_settings(settings))
        settings = MotionSettings{};
    settings.max_zoom = std::min(settings.max_zoom, representable_max_zoom());
    return settings;
}

float zoom_for_center(float x, float y)
{
    const float margin = std::min(std::min(x, 1.0f - x), std::min(y, 1.0f - y));
    return margin > 0.0f ? 0.5f / margin : std::numeric_limits<float>::infinity();
}

} // namespace

MotionEngine::MotionEngine(MotionSettings settings)
    : settings_(sanitize_settings(settings))
{
    reset();
}

void MotionEngine::reset()
{
    state_ = {};
    state_.visible_region = settings_.visible_region;
    const float width = settings_.visible_region.right - settings_.visible_region.left;
    const float height = settings_.visible_region.bottom - settings_.visible_region.top;
    const float initial_zoom = 1.0f / std::max(width, height);
    state_.zoom = std::clamp(initial_zoom, settings_.min_zoom, settings_.max_zoom);
}

CropState MotionEngine::update(std::optional<TargetObservation> target, float delta_seconds)
{
    const float raw_dt = std::isfinite(delta_seconds) && delta_seconds > 0.0f
                             ? delta_seconds : 0.0f;
    constexpr auto max_clock = std::numeric_limits<std::int64_t>::max() - 1000000000;
    constexpr long double nanoseconds_per_second = 1000000000.0L;
    const long double available_seconds =
        (static_cast<long double>(max_clock) - static_cast<long double>(state_.timestamp_ns)) /
        nanoseconds_per_second;
    if (static_cast<long double>(raw_dt) >= available_seconds) {
        return update_at(std::move(target), raw_dt, max_clock);
    } else {
        const auto elapsed_ns = static_cast<std::int64_t>(std::llround(
            static_cast<long double>(raw_dt) * nanoseconds_per_second));
        return update_at(std::move(target), raw_dt, state_.timestamp_ns + elapsed_ns);
    }
}

bool MotionEngine::accepts_observation(const TargetObservation &target,
                                       std::int64_t timestamp_ns) const
{
    return timestamp_ns >= 0 && is_valid(target.bounds) &&
           std::isfinite(target.confidence) &&
           target.confidence >= settings_.confidence_threshold && target.timestamp_ns >= 0 &&
           target.timestamp_ns <= timestamp_ns &&
           static_cast<double>(timestamp_ns - target.timestamp_ns) <=
               static_cast<double>(settings_.stale_timeout) * 1e9;
}

CropState MotionEngine::update_at(std::optional<TargetObservation> target,
                                  float motion_delta_seconds, std::int64_t timestamp_ns)
{
    const CropState previous = state_;
    if (timestamp_ns < state_.timestamp_ns)
        timestamp_ns = state_.timestamp_ns;
    state_.timestamp_ns = timestamp_ns;

    // Motion is capped so a delayed callback cannot teleport the crop.
    const float raw_dt = std::isfinite(motion_delta_seconds) && motion_delta_seconds > 0.0f
                             ? motion_delta_seconds : 0.0f;
    const float dt = std::min(raw_dt, 1.0f);

    const bool accepted = target && accepts_observation(*target, state_.timestamp_ns);

    const float desired_zoom = accepted
        ? std::clamp(std::min((settings_.dead_zone.right - settings_.dead_zone.left) /
                                   (target->bounds.right - target->bounds.left),
                               (settings_.dead_zone.bottom - settings_.dead_zone.top) /
                                   (target->bounds.bottom - target->bounds.top)),
                     settings_.min_zoom, settings_.max_zoom)
        : settings_.min_zoom;
    float zoom = move_toward(state_.zoom, desired_zoom, settings_.zoom_speed * dt);

    const float current_x = center_x(state_.visible_region);
    const float current_y = center_y(state_.visible_region);
    float half_width = 0.5f / zoom;
    float half_height = 0.5f / zoom;
    float desired_x = 0.5f;
    float desired_y = 0.5f;
    if (accepted) {
        const float target_x = center_x(target->bounds);
        const float target_y = center_y(target->bounds);
        const float width = 2.0f * half_width;
        const float height = 2.0f * half_height;
        const float left = current_x - half_width;
        const float top = current_y - half_height;
        const float zone_left = left + settings_.dead_zone.left * width;
        const float zone_right = left + settings_.dead_zone.right * width;
        const float zone_top = top + settings_.dead_zone.top * height;
        const float zone_bottom = top + settings_.dead_zone.bottom * height;
        desired_x = current_x +
                    (target_x < zone_left ? target_x - zone_left :
                     target_x > zone_right ? target_x - zone_right : 0.0f);
        desired_y = current_y +
                    (target_y < zone_top ? target_y - zone_top :
                     target_y > zone_bottom ? target_y - zone_bottom : 0.0f);
    }

    const float pan_limit = settings_.max_pan_speed * dt;
    float x = move_toward(current_x, desired_x, pan_limit);
    float y = move_toward(current_y, desired_y, pan_limit);

    // Source clamping may use the pan budget when it fits. Otherwise retain
    // the requested center and raise zoom enough for the crop to fit. This
    // preserves both limits in the same update.
    float zoom_floor = zoom;
    const auto constrain_axis = [&](float requested, float current) {
        const float bound = 0.5f / zoom;
        const float clamped = std::clamp(requested, bound, 1.0f - bound);
        if (std::fabs(clamped - current) <= pan_limit)
            return clamped;
        zoom_floor = std::max(zoom_floor, zoom_for_center(requested, 0.5f));
        return requested;
    };
    x = constrain_axis(x, current_x);
    y = constrain_axis(y, current_y);
    zoom = std::clamp(zoom_floor, settings_.min_zoom, settings_.max_zoom);
    half_width = 0.5f / zoom;
    half_height = 0.5f / zoom;
    state_.visible_region = {x - half_width, y - half_height, x + half_width, y + half_height};
    state_.zoom = zoom;
    if (!is_valid(state_.visible_region) || !std::isfinite(state_.zoom) ||
        state_.zoom < settings_.min_zoom || state_.zoom > settings_.max_zoom) {
        // When no center is reachable at the bounded maximum zoom, retaining
        // the last valid crop is the smallest safe correction.
        state_.visible_region = previous.visible_region;
        state_.zoom = previous.zoom;
    }
    state_.tracking = accepted;
    return state_;
}

} // namespace church
