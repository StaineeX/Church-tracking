// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "core/geometry.hpp"

#include <cstdint>
#include <optional>

namespace church {

struct TargetObservation {
    NormalizedRect bounds;
    float confidence;
    std::int64_t timestamp_ns;
};

struct CropState {
    NormalizedRect visible_region{0.0f, 0.0f, 1.0f, 1.0f};
    float zoom = 1.0f;
    bool tracking = false;
    std::int64_t timestamp_ns = 0;
};

struct MotionSettings {
    NormalizedRect dead_zone{0.35f, 0.35f, 0.65f, 0.65f};
    NormalizedRect visible_region{0.0f, 0.0f, 1.0f, 1.0f};
    float min_zoom = 1.0f;
    float max_zoom = 2.0f;
    float max_pan_speed = 1.0f;
    float zoom_speed = 0.5f;
    float confidence_threshold = 0.5f;
    float stale_timeout = 0.5f;
};

class MotionEngine {
public:
    explicit MotionEngine(MotionSettings settings = {});
    void reset();
    CropState update(std::optional<TargetObservation> target, float delta_seconds);
    CropState update_at(std::optional<TargetObservation> target, float motion_delta_seconds,
                        std::int64_t timestamp_ns);
    bool accepts_observation(const TargetObservation &target, std::int64_t timestamp_ns) const;
    CropState state() const { return state_; }

private:
    MotionSettings settings_;
    CropState state_;
};

} // namespace church
