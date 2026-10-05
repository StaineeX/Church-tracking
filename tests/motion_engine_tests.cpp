// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/motion_engine.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>

using church::CropState;
using church::MotionEngine;
using church::MotionSettings;
using church::NormalizedRect;
using church::TargetObservation;

namespace {
int failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool near(float actual, float expected)
{
    return std::fabs(actual - expected) < 0.0001f;
}

void check_rect(const NormalizedRect &actual, const NormalizedRect &expected, const char *message)
{
    check(near(actual.left, expected.left) && near(actual.top, expected.top) &&
              near(actual.right, expected.right) && near(actual.bottom, expected.bottom), message);
}

void check_safe(const CropState &state)
{
    const auto &r = state.visible_region;
    check(std::isfinite(r.left) && std::isfinite(r.top) && std::isfinite(r.right) &&
              std::isfinite(r.bottom) && std::isfinite(state.zoom), "state is finite");
    check(r.left >= 0 && r.top >= 0 && r.right <= 1 && r.bottom <= 1 &&
              r.left < r.right && r.top < r.bottom, "crop stays inside source");
    check(state.zoom >= 1 && state.zoom <= 2, "zoom stays within defaults");
}

TargetObservation target(NormalizedRect bounds, std::int64_t timestamp_ns = 0,
                         float confidence = 1.0f)
{
    return {bounds, confidence, timestamp_ns};
}

void dead_zone_holds_crop_when_target_center_is_inside_zone()
{
    MotionSettings settings;
    check_rect(settings.dead_zone, {0.35f, 0.35f, 0.65f, 0.65f}, "default dead zone");
    check_rect(settings.visible_region, {0, 0, 1, 1}, "default visible region");
    check(near(settings.min_zoom, 1) && near(settings.max_zoom, 2) &&
              near(settings.max_pan_speed, 1) && near(settings.zoom_speed, 0.5f) &&
              near(settings.confidence_threshold, 0.5f) && near(settings.stale_timeout, 0.5f),
          "default motion limits");
    MotionEngine engine(settings);
    const auto initial = engine.state();
    check_rect(initial.visible_region, {0, 0, 1, 1}, "initial crop is neutral");
    check(near(initial.zoom, 1) && !initial.tracking && initial.timestamp_ns == 0,
          "initial state is neutral");
    const auto state = engine.update(target({0.3f, 0.3f, 0.7f, 0.7f}), 0.1f);
    check_rect(state.visible_region, {0, 0, 1, 1}, "inside dead zone holds crop");
    check(state.tracking, "accepted target reports tracking");
}

void dead_zone_moves_crop_toward_target_without_exceeding_pan_speed()
{
    MotionSettings settings;
    settings.max_zoom = 1;
    MotionEngine engine(settings);
    const auto first = engine.update(target({0.85f, 0.45f, 0.95f, 0.55f}), 0.1f);
    check_rect(first.visible_region, {0, 0, 1, 1}, "full source prevents out-of-bounds pan");

    MotionEngine zoomed;
    zoomed.update(target({0.45f, 0.45f, 0.55f, 0.55f}, 1000000000), 1.0f);
    const auto before = zoomed.state();
    const auto after = zoomed.update(target({0.85f, 0.45f, 0.95f, 0.55f}, 1000000000), 0.1f);
    check(after.visible_region.left > before.visible_region.left, "crop moves toward target");
    const float before_x = (before.visible_region.left + before.visible_region.right) * 0.5f;
    const float after_x = (after.visible_region.left + after.visible_region.right) * 0.5f;
    check(after_x - before_x <= 0.1001f,
          "pan translation respects elapsed time");
    check_safe(after);
}

void crop_is_clamped_to_source_bounds_at_max_zoom()
{
    MotionSettings settings;
    settings.zoom_speed = 10;
    settings.max_pan_speed = 10;
    MotionEngine engine(settings);
    const auto state = engine.update(target({0.95f, 0.95f, 1, 1}, 1000000000), 1.0f);
    check(near(state.zoom, 2), "zoom reaches exact maximum");
    check_rect(state.visible_region, {0.5f, 0.5f, 1, 1}, "crop clamps at bottom right");
    check_safe(state);
}

void zoom_is_limited_and_uses_elapsed_time()
{
    MotionEngine engine;
    const auto observation = target({0.45f, 0.45f, 0.55f, 0.55f});
    const auto first = engine.update(observation, 0.1f);
    check(near(first.zoom, 1.05f), "zoom change uses 0.1 seconds");
    const auto second = engine.update(observation, 0.1f);
    check(near(second.zoom, 1.1f), "zoom advances by fixed elapsed time");
    check_safe(second);
}

void low_confidence_and_stale_targets_hold_safe_state()
{
    MotionEngine engine;
    const auto observation = target({0.45f, 0.45f, 0.55f, 0.55f});
    const auto accepted = engine.update(observation, 0.1f);
    const auto low = engine.update(target({0.8f, 0.8f, 0.9f, 0.9f}, 100000000, 0.49f), 0);
    check_rect(low.visible_region, accepted.visible_region, "low confidence holds at zero delta");
    check(!low.tracking, "low confidence is not tracking");
    const auto stale = engine.update(observation, 1.0f);
    check(!stale.tracking, "old timestamp is stale");
    check(stale.zoom < accepted.zoom, "stale target eases toward neutral");
    check_safe(stale);
}

void invalid_rectangles_and_time_values_never_publish_non_finite_state()
{
    MotionEngine engine;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const auto a = engine.update(target({nan, 0, 1, 1}), nan);
    const auto b = engine.update(target({0.8f, 0.2f, 0.2f, 0.8f}), inf);
    const auto c = engine.update(target({-0.1f, 0, 0.2f, 0.2f}), -1);
    const auto d = engine.update(target({0, 0, 1, 1}, 0, inf), 1e30f);
    for (const auto &state : {a, b, c, d}) {
        check_safe(state);
        check(!state.tracking, "invalid input is never tracked");
    }
    check(d.timestamp_ns <= 1000000000, "huge delta is bounded");

}

void target_loss_eases_to_neutral_without_a_jump()
{
    MotionEngine engine;
    engine.update(target({0.85f, 0.45f, 0.95f, 0.55f}, 1000000000), 1.0f);
    const auto before = engine.state();
    const auto after = engine.update(std::nullopt, 0.1f);
    check(!after.tracking, "missing target reports fallback");
    check(after.zoom < before.zoom && after.zoom >= before.zoom - 0.0501f,
          "zoom eases toward neutral at bounded speed");
    const float before_x = (before.visible_region.left + before.visible_region.right) * 0.5f;
    const float after_x = (after.visible_region.left + after.visible_region.right) * 0.5f;
    check(std::fabs(after_x - before_x) <= 0.1001f,
          "fallback pan does not jump");
    check_safe(after);
}
} // namespace

int main()
{
    dead_zone_holds_crop_when_target_center_is_inside_zone();
    dead_zone_moves_crop_toward_target_without_exceeding_pan_speed();
    crop_is_clamped_to_source_bounds_at_max_zoom();
    zoom_is_limited_and_uses_elapsed_time();
    low_confidence_and_stale_targets_hold_safe_state();
    invalid_rectangles_and_time_values_never_publish_non_finite_state();
    target_loss_eases_to_neutral_without_a_jump();
    if (failures)
        return EXIT_FAILURE;
    std::cout << "PASS: seven motion engine behaviors\n";
    return EXIT_SUCCESS;
}
