// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "obs/filter_state.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

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
} // namespace

int main()
{
    neutral_state_is_available_without_graphics();
    disabled_filter_skips_motion_work();
    non_synthetic_filter_skips_motion_work();
    synthetic_timestamps_drive_the_worker();
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
