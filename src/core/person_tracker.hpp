// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "core/tracker.hpp"

#include <cstdint>
#include <optional>

namespace church {

class PersonTracker final : public Tracker {
public:
    explicit PersonTracker(std::int64_t max_prediction_ns = 500'000'000);

    void reset() override;
    void set_target(TargetObservation target) override;
    std::optional<TargetObservation> current_target() const override { return last_observation_; }
    std::optional<TargetObservation> predict(std::int64_t timestamp_ns) const override;
    std::optional<TargetObservation> update(const VideoFrame &frame) override;

private:
    std::optional<TargetObservation> last_observation_;
    float velocity_x_ = 0.0f;
    float velocity_y_ = 0.0f;
    std::int64_t max_prediction_ns_;
};

} // namespace church
