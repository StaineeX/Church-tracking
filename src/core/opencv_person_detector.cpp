// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/opencv_person_detector.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace church {
namespace {

bool valid_frame(const VideoFrame &frame)
{
    const auto minimum_stride = static_cast<std::uint64_t>(frame.width) * 4u;
    if (frame.format != PixelFormat::rgba8 || frame.width == 0 || frame.height == 0 ||
        frame.width > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
        frame.height > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
        static_cast<std::uint64_t>(frame.stride) < minimum_stride)
        return false;

    const auto rows = static_cast<std::size_t>(frame.height);
    const auto stride = static_cast<std::size_t>(frame.stride);
    return rows <= std::numeric_limits<std::size_t>::max() / stride &&
           frame.pixels.size() >= rows * stride;
}

float normalized(double value, double extent)
{
    return static_cast<float>(std::clamp(value / extent, 0.0, 1.0));
}

} // namespace

OpenCvPersonDetector::OpenCvPersonDetector(float hit_threshold, std::uint32_t max_dimension)
    : hog_(),
      hit_threshold_(std::isfinite(hit_threshold) ? hit_threshold : 0.0f),
      max_dimension_(max_dimension)
{
    hog_.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());
}

std::vector<TargetObservation> OpenCvPersonDetector::detect(const VideoFrame &frame)
{
    if (!valid_frame(frame) || max_dimension_ == 0)
        return {};

    try {
        const cv::Mat rgba(static_cast<int>(frame.height), static_cast<int>(frame.width),
                           CV_8UC4, const_cast<std::uint8_t *>(frame.pixels.data()),
                           frame.stride);
        cv::Mat bgr;
        cv::cvtColor(rgba, bgr, cv::COLOR_RGBA2BGR);

        if (bgr.cols < hog_.winSize.width || bgr.rows < hog_.winSize.height)
            return {};

        const auto largest_dimension = std::max(frame.width, frame.height);
        const double scale = largest_dimension > max_dimension_
                                 ? static_cast<double>(max_dimension_) / largest_dimension
                                 : 1.0;
        cv::Mat search_image;
        if (scale < 1.0) {
            cv::resize(bgr, search_image, {}, scale, scale, cv::INTER_AREA);
        } else {
            search_image = bgr;
        }

        if (search_image.cols < hog_.winSize.width || search_image.rows < hog_.winSize.height)
            return {};

        std::vector<cv::Rect> found;
        std::vector<double> weights;
        hog_.detectMultiScale(search_image, found, weights, hit_threshold_, cv::Size(8, 8),
                              cv::Size(32, 32), 1.05, 2.0, false);

        std::vector<TargetObservation> observations;
        observations.reserve(found.size());
        for (std::size_t i = 0; i < found.size(); ++i) {
            const auto &rect = found[i];
            const double left = std::clamp(static_cast<double>(rect.x) / scale, 0.0,
                                           static_cast<double>(frame.width));
            const double top = std::clamp(static_cast<double>(rect.y) / scale, 0.0,
                                          static_cast<double>(frame.height));
            const double right = std::clamp(
                (static_cast<double>(rect.x) + rect.width) / scale, 0.0,
                static_cast<double>(frame.width));
            const double bottom = std::clamp(
                (static_cast<double>(rect.y) + rect.height) / scale, 0.0,
                static_cast<double>(frame.height));
            if (!(left < right && top < bottom))
                continue;

            const double confidence = i < weights.size() ? weights[i] : 0.0;
            if (!std::isfinite(confidence) || confidence < hit_threshold_)
                continue;

            observations.push_back(
                {{normalized(left, frame.width), normalized(top, frame.height),
                  normalized(right, frame.width), normalized(bottom, frame.height)},
                 static_cast<float>(confidence), frame.timestamp_ns});
        }
        std::stable_sort(observations.begin(), observations.end(),
                         [](const TargetObservation &left, const TargetObservation &right) {
                             return left.confidence > right.confidence;
                         });
        return observations;
    } catch (const cv::Exception &) {
        return {};
    }
}

} // namespace church
