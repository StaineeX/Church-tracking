// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "core/detector.hpp"

#include <opencv2/objdetect.hpp>

#include <cstdint>

namespace church {

class OpenCvPersonDetector final : public Detector {
public:
    explicit OpenCvPersonDetector(float hit_threshold = 0.0f,
                                  std::uint32_t max_dimension = 640);

    std::vector<TargetObservation> detect(const VideoFrame &frame) override;

private:
    cv::HOGDescriptor hog_;
    float hit_threshold_;
    std::uint32_t max_dimension_;
};

} // namespace church
