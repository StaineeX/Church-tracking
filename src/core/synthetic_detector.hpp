// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "core/detector.hpp"

#include <cmath>

namespace church {

class SyntheticDetector final : public Detector {
public:
    std::vector<TargetObservation> detect(const VideoFrame &frame) override
    {
        const double seconds = static_cast<double>(frame.timestamp_ns) / 1e9;
        const float center = 0.5f + 0.35f * static_cast<float>(std::sin(seconds));
        return {{{center - 0.05f, 0.45f, center + 0.05f, 0.55f},
                 1.0f, frame.timestamp_ns}};
    }
};

} // namespace church
