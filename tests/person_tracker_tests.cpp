// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/person_tracker.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {
int failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

church::VideoFrame frame(std::int64_t timestamp_ns)
{
    return {{}, 1, 1, 0, church::PixelFormat::rgba8, timestamp_ns};
}

float center_x(const church::TargetObservation &target)
{
    return (target.bounds.left + target.bounds.right) * 0.5f;
}

void tracker_predicts_constant_velocity_from_timestamps()
{
    church::PersonTracker tracker;
    tracker.set_target({{0.2f, 0.3f, 0.4f, 0.7f}, 0.9f, 1'000'000'000});
    tracker.set_target({{0.3f, 0.3f, 0.5f, 0.7f}, 0.9f, 1'100'000'000});

    const auto predicted = tracker.update(frame(1'200'000'000));
    check(predicted.has_value(), "tracker predicts during a short detection gap");
    check(predicted && std::fabs(center_x(*predicted) - 0.5f) < 0.0001f,
          "prediction uses timestamp-based center velocity");
    check(predicted && predicted->timestamp_ns == 1'200'000'000,
          "prediction carries the current frame timestamp");
}

void tracker_clamps_predicted_bounds_to_normalized_source()
{
    church::PersonTracker tracker;
    tracker.set_target({{0.8f, 0.2f, 0.9f, 0.4f}, 1.0f, 1'000'000'000});
    tracker.set_target({{0.9f, 0.2f, 1.0f, 0.4f}, 1.0f, 1'100'000'000});

    const auto predicted = tracker.update(frame(1'200'000'000));
    check(predicted && predicted->bounds.left == 0.9f && predicted->bounds.right == 1.0f,
          "prediction clamps without changing target size");
    check(predicted && predicted->bounds.top >= 0.0f && predicted->bounds.bottom <= 1.0f,
          "vertical prediction remains inside the source");
}

void tracker_stops_prediction_after_a_cumulative_gap()
{
    church::PersonTracker tracker;
    tracker.set_target({{0.2f, 0.2f, 0.4f, 0.4f}, 1.0f, 1'000'000'000});
    tracker.set_target({{0.3f, 0.2f, 0.5f, 0.4f}, 1.0f, 1'100'000'000});

    for (int step = 1; step <= 5; ++step) {
        const auto predicted = tracker.update(frame(1'100'000'000 + step * 100'000'000));
        check(predicted.has_value(), "tracker predicts each frame within the horizon");
    }
    check(!tracker.update(frame(1'700'000'000)),
          "tracker returns no target after cumulative predictions exceed the horizon");
}

void tracker_reset_clears_target_and_velocity()
{
    church::PersonTracker tracker;
    tracker.set_target({{0.2f, 0.2f, 0.4f, 0.4f}, 1.0f, 1'000'000'000});
    tracker.set_target({{0.4f, 0.2f, 0.6f, 0.4f}, 1.0f, 1'100'000'000});
    tracker.reset();
    check(!tracker.update(frame(1'200'000'000)), "reset removes the previous target");

    tracker.set_target({{0.2f, 0.2f, 0.4f, 0.4f}, 1.0f, 2'000'000'000});
    const auto after_reset = tracker.update(frame(2'100'000'000));
    check(after_reset && std::fabs(center_x(*after_reset) - 0.3f) < 0.0001f,
          "reset also clears velocity before the next target");
}
} // namespace

int main()
{
    tracker_predicts_constant_velocity_from_timestamps();
    tracker_clamps_predicted_bounds_to_normalized_source();
    tracker_stops_prediction_after_a_cumulative_gap();
    tracker_reset_clears_target_and_velocity();
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
