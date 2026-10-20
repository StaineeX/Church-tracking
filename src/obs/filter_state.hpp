// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "core/tracking_worker.hpp"

#include <cstdint>
#include <memory>

namespace church {

// OBS-independent seam. The render callback may submit metadata and read a
// snapshot, but starting and stopping the worker belongs to OBS lifecycle.
class FilterState {
public:
    FilterState(bool enabled, bool synthetic_mode, MotionSettings settings = {},
                std::unique_ptr<Detector> detector = nullptr,
                std::unique_ptr<Tracker> tracker = nullptr);
    ~FilterState();
    FilterState(const FilterState &) = delete;
    FilterState &operator=(const FilterState &) = delete;

    bool submit(VideoFrame frame);
    bool submit_synthetic(std::int64_t timestamp_ns);
    bool synthetic_mode() const { return synthetic_mode_; }
    bool active() const { return worker_ != nullptr; }
    CropState crop() const;
    void stop();

private:
    std::unique_ptr<TrackingWorker> worker_;
    bool synthetic_mode_ = false;
};

} // namespace church
