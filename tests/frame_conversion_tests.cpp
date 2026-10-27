// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/frame_conversion.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace {
int failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

church::SourceFrameView base_view(church::SourcePixelFormat format, std::uint32_t width,
                                  std::uint32_t height)
{
    church::SourceFrameView view;
    view.format = format;
    view.width = width;
    view.height = height;
    view.timestamp_ns = 1'000;
    return view;
}

void check_pixel(const church::VideoFrame &frame, std::size_t offset, std::uint8_t r,
                 std::uint8_t g, std::uint8_t b, std::uint8_t a, const char *message)
{
    check(frame.pixels.size() >= offset + 4 && frame.pixels[offset] == r &&
              frame.pixels[offset + 1] == g && frame.pixels[offset + 2] == b &&
              frame.pixels[offset + 3] == a,
          message);
}

void packed_formats_copy_channels_and_padding()
{
    const std::array formats{church::SourcePixelFormat::rgba, church::SourcePixelFormat::bgra,
                             church::SourcePixelFormat::bgrx, church::SourcePixelFormat::bgr3};
    for (const auto format : formats) {
        const std::uint32_t bytes_per_pixel = format == church::SourcePixelFormat::bgr3 ? 3 : 4;
        const std::uint32_t stride = 2 * bytes_per_pixel + 3;
        std::vector<std::uint8_t> storage(stride * 2, 0xee);
        auto *row = storage.data();
        if (format == church::SourcePixelFormat::rgba) {
            row[0] = 1; row[1] = 2; row[2] = 3; row[3] = 4;
            row[4] = 5; row[5] = 6; row[6] = 7; row[7] = 8;
        } else {
            row[0] = 3; row[1] = 2; row[2] = 1; row[3] = format == church::SourcePixelFormat::bgra ? 4 : 0;
            row[4] = 7; row[5] = 6; row[6] = 5; row[7] = format == church::SourcePixelFormat::bgra ? 8 : 0;
            if (format == church::SourcePixelFormat::bgr3) {
                row[3] = 7; row[4] = 6; row[5] = 5;
            }
        }
        auto *second = storage.data() + stride;
        if (format == church::SourcePixelFormat::rgba) {
            second[0] = 9; second[1] = 10; second[2] = 11; second[3] = 12;
        } else if (format == church::SourcePixelFormat::bgr3) {
            second[0] = 11; second[1] = 10; second[2] = 9;
        } else {
            second[0] = 11; second[1] = 10; second[2] = 9; second[3] = 12;
        }

        auto view = base_view(format, 2, 2);
        view.planes[0] = {storage.data(), stride};
        const auto converted = church::convert_source_frame(view);
        check(converted.has_value(), "padded packed frame converts");
        if (!converted)
            continue;
        const auto alpha = format == church::SourcePixelFormat::rgba ||
                                   format == church::SourcePixelFormat::bgra
                               ? 4
                               : 255;
        check_pixel(*converted, 0, 1, 2, 3, alpha,
                    "first packed pixel preserves channel order");
        check_pixel(*converted, 8, 9, 10, 11,
                    format == church::SourcePixelFormat::rgba ||
                            format == church::SourcePixelFormat::bgra
                        ? 12
                        : 255,
                    "padded row does not leak into packed pixels");
    }
}

void yuv_formats_use_padded_planes()
{
    std::array<float, 16> matrix{};
    matrix[0] = matrix[4] = matrix[8] = 1.0f;
    for (const auto format : {church::SourcePixelFormat::nv12, church::SourcePixelFormat::i420}) {
        auto view = base_view(format, 3, 3);
        view.full_range = true;
        view.color_matrix = matrix;
        std::vector<std::uint8_t> y(5 * 3, 0);
        y[0] = 10; y[1] = 20; y[2] = 30;
        y[5] = 40; y[6] = 50; y[7] = 60;
        y[10] = 70; y[11] = 80; y[12] = 90;
        view.planes[0] = {y.data(), 5};
        std::vector<std::uint8_t> u(3 * 2, 128);
        std::vector<std::uint8_t> v(3 * 2, 128);
        if (format == church::SourcePixelFormat::nv12) {
            std::vector<std::uint8_t> uv(6 * 2, 128);
            view.planes[1] = {uv.data(), 6};
            const auto converted = church::convert_source_frame(view);
            check(converted.has_value(), "padded NV12 planes convert");
            if (converted)
                check_pixel(*converted, 8, 30, 30, 30, 255, "NV12 samples map to owned RGBA");
        } else {
            view.planes[1] = {u.data(), 3};
            view.planes[2] = {v.data(), 3};
            const auto converted = church::convert_source_frame(view);
            check(converted.has_value(), "padded I420 planes convert");
            if (converted)
                check_pixel(*converted, 8, 30, 30, 30, 255, "I420 samples map to owned RGBA");
        }
    }
}

void malformed_frames_fail_closed()
{
    auto view = base_view(church::SourcePixelFormat::rgba, 2, 1);
    std::array<std::uint8_t, 8> pixels{};
    view.planes[0] = {pixels.data(), 7};
    check(!church::convert_source_frame(view), "undersized packed stride is rejected");
    view.planes[0] = {nullptr, 8};
    check(!church::convert_source_frame(view), "null plane is rejected");
    view.planes[0] = {pixels.data(), 8};
    view.timestamp_ns = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1;
    check(!church::convert_source_frame(view), "signed timestamp overflow is rejected");
    view.timestamp_ns = 1;
    view.width = 0;
    check(!church::convert_source_frame(view), "zero width is rejected");
    view.width = 2;
    view.format = church::SourcePixelFormat::unsupported;
    check(!church::convert_source_frame(view), "unsupported format is rejected");
}

void flipped_frames_reverse_source_rows()
{
    auto view = base_view(church::SourcePixelFormat::rgba, 1, 2);
    view.flip = true;
    std::array<std::uint8_t, 16> pixels{1, 2, 3, 255, 0xee, 0xee, 0xee, 0xee,
                                        4, 5, 6, 255, 0xee, 0xee, 0xee, 0xee};
    view.planes[0] = {pixels.data(), 8};
    const auto converted = church::convert_source_frame(view);
    check(converted.has_value(), "flipped packed frame converts");
    if (converted) {
        check_pixel(*converted, 0, 4, 5, 6, 255, "flip puts the last source row first");
        check_pixel(*converted, 4, 1, 2, 3, 255, "flip puts the first source row last");
    }
}
} // namespace

int main()
{
    packed_formats_copy_channels_and_padding();
    yuv_formats_use_padded_planes();
    malformed_frames_fail_closed();
    flipped_frames_reverse_source_rows();
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
