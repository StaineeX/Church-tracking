// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "core/frame_types.hpp"
#include "core/motion_engine.hpp"

#include <vector>

namespace church {

class Detector {
public:
    virtual ~Detector() = default;
    virtual std::vector<TargetObservation> detect(const VideoFrame &frame) = 0;
};

} // namespace church
