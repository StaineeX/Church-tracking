// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "obs/filter_state.hpp"

#include <utility>

namespace church {

FilterState::FilterState(bool enabled, bool synthetic_mode, MotionSettings settings)
{
    if (enabled && synthetic_mode) {
        worker_ = std::make_unique<TrackingWorker>(nullptr, nullptr, settings);
        worker_->start();
    }
}

FilterState::~FilterState() { stop(); }

bool FilterState::submit_synthetic(std::int64_t timestamp_ns)
{
    if (!worker_ || timestamp_ns < 0)
        return false;
    // A tiny owned placeholder satisfies the worker's frame contract. Synthetic
    // detector/tracker use only the timestamp and never inspect these pixels.
    VideoFrame frame{{0, 0, 0, 0}, 1, 1, 4, PixelFormat::rgba8, timestamp_ns};
    return worker_->submit(std::move(frame));
}

CropState FilterState::crop() const
{
    if (worker_) {
        const auto latest = worker_->snapshot();
        if (latest)
            return *latest;
    }
    return {};
}

void FilterState::stop()
{
    if (worker_)
        worker_->stop();
}

} // namespace church
