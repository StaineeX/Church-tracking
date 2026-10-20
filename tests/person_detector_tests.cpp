// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/opencv_person_detector.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {
int failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

church::VideoFrame person_fixture()
{
    constexpr std::uint32_t width = 320;
    constexpr std::uint32_t height = 240;
    constexpr std::uint32_t stride = width * 4 + 16;
    church::VideoFrame frame;
    frame.width = width;
    frame.height = height;
    frame.stride = stride;
    frame.pixels.assign(static_cast<std::size_t>(stride) * height, 230);
    frame.timestamp_ns = 123456789;

    cv::Mat image(static_cast<int>(height), static_cast<int>(width), CV_8UC4,
                  frame.pixels.data(), stride);
    image.forEach<cv::Vec4b>([](cv::Vec4b &pixel, const int *) {
        pixel[3] = 255;
    });

    const cv::Scalar person(20, 20, 20, 255);
    cv::circle(image, {160, 57}, 22, person, cv::FILLED);
    cv::rectangle(image, {132, 82, 188, 161}, person, cv::FILLED);
    cv::line(image, {135, 91}, {105, 145}, person, 13);
    cv::line(image, {185, 91}, {215, 145}, person, 13);
    cv::line(image, {145, 156}, {126, 221}, person, 17);
    cv::line(image, {175, 156}, {194, 221}, person, 17);
    return frame;
}

void malformed_frames_are_rejected_without_detections()
{
    church::OpenCvPersonDetector detector;
    check(detector.detect({}).empty(), "empty frame is rejected");

    church::VideoFrame undersized_stride{{1, 2, 3, 4}, 2, 1, 4,
                                         church::PixelFormat::rgba8, 0};
    check(detector.detect(undersized_stride).empty(), "undersized stride is rejected");

    church::VideoFrame undersized_storage{{1, 2, 3, 4}, 2, 2, 8,
                                          church::PixelFormat::rgba8, 0};
    check(detector.detect(undersized_storage).empty(), "undersized storage is rejected");

    church::VideoFrame too_small{{}, 64, 64, 64 * 4, church::PixelFormat::rgba8, 0};
    too_small.pixels.resize(static_cast<std::size_t>(too_small.stride) * too_small.height);
    check(detector.detect(too_small).empty(), "frames smaller than the HOG window are rejected");
}

void person_fixture_produces_normalized_confident_bounds()
{
    church::OpenCvPersonDetector detector(0.0f, 240);
    const auto detections = detector.detect(person_fixture());
    check(!detections.empty(), "person-shaped fixture is accepted by the HOG detector");
    for (std::size_t i = 0; i < detections.size(); ++i) {
        const auto &detection = detections[i];
        const auto &bounds = detection.bounds;
        check(std::isfinite(bounds.left) && std::isfinite(bounds.top) &&
                  std::isfinite(bounds.right) && std::isfinite(bounds.bottom),
              "detection bounds are finite");
        check(bounds.left >= 0.0f && bounds.top >= 0.0f && bounds.right <= 1.0f &&
                  bounds.bottom <= 1.0f && bounds.left < bounds.right &&
                  bounds.top < bounds.bottom,
              "detection bounds are normalized and non-empty");
        check(std::isfinite(detection.confidence), "detection confidence is finite");
        check(detection.timestamp_ns == 123456789, "detection preserves frame timestamp");
        if (i > 0)
            check(detections[i - 1].confidence >= detection.confidence,
                  "detections are ordered by descending confidence");
    }
    if (!detections.empty()) {
        const auto &bounds = detections.front().bounds;
        const float center_x = (bounds.left + bounds.right) * 0.5f;
        const float center_y = (bounds.top + bounds.bottom) * 0.5f;
        check(center_x > 0.35f && center_x < 0.65f,
              "resized detection maps back near the fixture center horizontally");
        check(center_y > 0.25f && center_y < 0.75f,
              "resized detection maps back near the fixture center vertically");
        check(bounds.left < 0.5f && bounds.right > 0.5f && bounds.top < 0.5f &&
                  bounds.bottom > 0.5f,
              "normalized detection surrounds the fixture center");
    }
}
} // namespace

int main()
{
    malformed_frames_are_rejected_without_detections();
    person_fixture_produces_normalized_confident_bounds();
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
