// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/motion_engine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

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

} // namespace

MotionEngine::MotionEngine(MotionSettings settings)
    : settings_(valid_settings(settings) ? settings : MotionSettings{})
{
}

CropState MotionEngine::update(std::optional<TargetObservation> target, float delta_seconds)
{
    // Bound pauses so a delayed callback cannot teleport the crop.
    const float dt = std::isfinite(delta_seconds) && delta_seconds > 0.0f
                         ? std::min(delta_seconds, 1.0f) : 0.0f;
    const auto elapsed_ns = static_cast<std::int64_t>(std::llround(static_cast<double>(dt) * 1e9));
    constexpr auto max_clock = std::numeric_limits<std::int64_t>::max() - 1000000000;
    state_.timestamp_ns += std::min(elapsed_ns, max_clock - state_.timestamp_ns);

    const bool accepted = target && is_valid(target->bounds) &&
                          std::isfinite(target->confidence) &&
                          target->confidence >= settings_.confidence_threshold &&
                          target->timestamp_ns >= 0 && target->timestamp_ns <= state_.timestamp_ns &&
                          static_cast<double>(state_.timestamp_ns - target->timestamp_ns) <=
                              static_cast<double>(settings_.stale_timeout) * 1e9;

    const float desired_zoom = accepted
        ? std::clamp(std::min((settings_.dead_zone.right - settings_.dead_zone.left) /
                                   (target->bounds.right - target->bounds.left),
                               (settings_.dead_zone.bottom - settings_.dead_zone.top) /
                                   (target->bounds.bottom - target->bounds.top)),
                     settings_.min_zoom, settings_.max_zoom)
        : settings_.min_zoom;
    state_.zoom = move_toward(state_.zoom, desired_zoom, settings_.zoom_speed * dt);

    const float half_width = 0.5f / state_.zoom;
    const float half_height = 0.5f / state_.zoom;
    float desired_x = 0.5f;
    float desired_y = 0.5f;
    if (accepted) {
        const float target_x = center_x(target->bounds);
        const float target_y = center_y(target->bounds);
        const float width = 2.0f * half_width;
        const float height = 2.0f * half_height;
        const float left = center_x(state_.visible_region) - half_width;
        const float top = center_y(state_.visible_region) - half_height;
        const float zone_left = left + settings_.dead_zone.left * width;
        const float zone_right = left + settings_.dead_zone.right * width;
        const float zone_top = top + settings_.dead_zone.top * height;
        const float zone_bottom = top + settings_.dead_zone.bottom * height;
        desired_x = center_x(state_.visible_region) +
                    (target_x < zone_left ? target_x - zone_left :
                     target_x > zone_right ? target_x - zone_right : 0.0f);
        desired_y = center_y(state_.visible_region) +
                    (target_y < zone_top ? target_y - zone_top :
                     target_y > zone_bottom ? target_y - zone_bottom : 0.0f);
    }

    const float pan_limit = settings_.max_pan_speed * dt;
    const float x = std::clamp(move_toward(center_x(state_.visible_region), desired_x, pan_limit),
                               half_width, 1.0f - half_width);
    const float y = std::clamp(move_toward(center_y(state_.visible_region), desired_y, pan_limit),
                               half_height, 1.0f - half_height);
    state_.visible_region = {x - half_width, y - half_height, x + half_width, y + half_height};
    state_.tracking = accepted;
    return state_;
}

} // namespace church
