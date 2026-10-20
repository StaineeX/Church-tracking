// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "core/frame_types.hpp"
#include "core/motion_engine.hpp"

#include <cstdint>
#include <optional>

namespace church {

class Tracker {
public:
    virtual ~Tracker() = default;
    virtual void reset() = 0;
    virtual void set_target(TargetObservation target) = 0;
    virtual std::optional<TargetObservation> current_target() const { return std::nullopt; }
    // Compatibility default: trackers without timestamp prediction keep their
    // current target available for association.
    virtual std::optional<TargetObservation> predict(std::int64_t timestamp_ns) const
    {
        (void)timestamp_ns;
        return current_target();
    }
    virtual std::optional<TargetObservation> update(const VideoFrame &frame) = 0;
};

} // namespace church
