// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "core/tracker.hpp"

namespace church {

class SyntheticTracker final : public Tracker {
public:
    void reset() override { last_.reset(); }
    void set_target(TargetObservation target) override { last_ = target; }
    std::optional<TargetObservation> update(const VideoFrame &frame) override
    {
        if (last_)
            last_->timestamp_ns = frame.timestamp_ns;
        return last_;
    }

private:
    std::optional<TargetObservation> last_;
};

} // namespace church
