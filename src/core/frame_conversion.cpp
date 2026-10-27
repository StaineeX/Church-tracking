// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/frame_conversion.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <limits>

namespace church {
namespace {

struct YuvMatrix {
    std::array<float, 12> values{};
};

std::optional<std::size_t> output_size(std::uint32_t width, std::uint32_t height)
{
    const auto row_bytes = static_cast<std::uint64_t>(width) * 4u;
    if (row_bytes > std::numeric_limits<std::uint32_t>::max() ||
        row_bytes > std::numeric_limits<std::size_t>::max() ||
        (height != 0 && row_bytes > std::numeric_limits<std::uint64_t>::max() / height))
        return std::nullopt;
    const auto total_bytes = row_bytes * static_cast<std::uint64_t>(height);
    if (total_bytes > std::numeric_limits<std::size_t>::max())
        return std::nullopt;
    return static_cast<std::size_t>(total_bytes);
}

bool valid_plane(const SourcePlaneView &plane, std::uint32_t rows,
                 std::uint64_t minimum_row_bytes)
{
    return plane.data && rows != 0 && static_cast<std::uint64_t>(plane.stride) >= minimum_row_bytes &&
           static_cast<std::uint64_t>(plane.stride) <=
               std::numeric_limits<std::size_t>::max() / rows;
}

std::uint32_t source_row(std::uint32_t row, std::uint32_t height, bool flip)
{
    return flip ? height - 1u - row : row;
}

YuvMatrix yuv_matrix(const SourceFrameView &source)
{
    bool valid = true;
    bool nonzero = false;
    for (std::size_t i = 0; i < 12; ++i) {
        valid = valid && std::isfinite(source.color_matrix[i]);
        nonzero = nonzero || source.color_matrix[i] != 0.0f;
    }
    if (valid && nonzero) {
        YuvMatrix matrix;
        std::copy_n(source.color_matrix.begin(), 12, matrix.values.begin());
        return matrix;
    }
    if (source.full_range)
        return {{{1.0f, 0.0f, 1.4020f, -0.7010f,
                  1.0f, -0.3441f, -0.7141f, 0.5291f,
                  1.0f, 1.7720f, 0.0f, -0.8860f}}};
    return {{{1.1644f, 0.0f, 1.7927f, -0.9729f,
              1.1644f, -0.2132f, -0.5329f, 0.3015f,
              1.1644f, 2.1124f, 0.0f, -1.1334f}}};
}

std::uint8_t clamp_byte(float value)
{
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0f, 255.0f)));
}

void write_yuv_pixel(VideoFrame &result, std::uint32_t x, std::uint32_t y,
                     std::uint8_t y_sample, std::uint8_t u_sample, std::uint8_t v_sample,
                     const YuvMatrix &matrix)
{
    const float y_value = static_cast<float>(y_sample) / 255.0f;
    const float u_value = static_cast<float>(u_sample) / 255.0f;
    const float v_value = static_cast<float>(v_sample) / 255.0f;
    auto *pixel = result.pixels.data() + static_cast<std::size_t>(y) * result.stride + x * 4u;
    pixel[0] = clamp_byte((matrix.values[0] * y_value + matrix.values[1] * u_value +
                           matrix.values[2] * v_value + matrix.values[3]) * 255.0f);
    pixel[1] = clamp_byte((matrix.values[4] * y_value + matrix.values[5] * u_value +
                           matrix.values[6] * v_value + matrix.values[7]) * 255.0f);
    pixel[2] = clamp_byte((matrix.values[8] * y_value + matrix.values[9] * u_value +
                           matrix.values[10] * v_value + matrix.values[11]) * 255.0f);
    pixel[3] = 255;
}

} // namespace

std::optional<VideoFrame> convert_source_frame(const SourceFrameView &source)
{
    if (source.width == 0 || source.height == 0 ||
        source.timestamp_ns > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        return std::nullopt;
    if (source.format == SourcePixelFormat::unsupported)
        return std::nullopt;
    const auto bytes = output_size(source.width, source.height);
    if (!bytes)
        return std::nullopt;

    try {
        VideoFrame result;
        result.width = source.width;
        result.height = source.height;
        result.stride = source.width * 4u;
        result.format = PixelFormat::rgba8;
        result.timestamp_ns = static_cast<std::int64_t>(source.timestamp_ns);
        result.pixels.resize(*bytes);

        const auto width = source.width;
        const auto height = source.height;
        switch (source.format) {
        case SourcePixelFormat::rgba:
        case SourcePixelFormat::bgra:
        case SourcePixelFormat::bgrx:
        case SourcePixelFormat::bgr3: {
            const std::uint32_t bytes_per_pixel = source.format == SourcePixelFormat::bgr3 ? 3u : 4u;
            if (!valid_plane(source.planes[0], height,
                             static_cast<std::uint64_t>(width) * bytes_per_pixel))
                return std::nullopt;
            for (std::uint32_t y = 0; y < height; ++y) {
                const auto *input = source.planes[0].data +
                                    static_cast<std::size_t>(source_row(y, height, source.flip)) *
                                        source.planes[0].stride;
                auto *output = result.pixels.data() + static_cast<std::size_t>(y) * result.stride;
                for (std::uint32_t x = 0; x < width; ++x) {
                    if (source.format == SourcePixelFormat::rgba) {
                        output[x * 4u + 0] = input[x * 4u + 0];
                        output[x * 4u + 1] = input[x * 4u + 1];
                        output[x * 4u + 2] = input[x * 4u + 2];
                        output[x * 4u + 3] = input[x * 4u + 3];
                    } else {
                        const auto input_offset = x * bytes_per_pixel;
                        output[x * 4u + 0] = input[input_offset + 2];
                        output[x * 4u + 1] = input[input_offset + 1];
                        output[x * 4u + 2] = input[input_offset + 0];
                        output[x * 4u + 3] = source.format == SourcePixelFormat::bgra
                                                 ? input[input_offset + 3] : 255;
                    }
                }
            }
            return result;
        }
        case SourcePixelFormat::nv12: {
            const auto chroma_width = width / 2u + width % 2u;
            const auto chroma_height = height / 2u + height % 2u;
            const auto chroma_row_bytes = static_cast<std::uint64_t>(chroma_width) * 2u;
            if (!valid_plane(source.planes[0], height, width) ||
                !valid_plane(source.planes[1], chroma_height, chroma_row_bytes))
                return std::nullopt;
            const auto matrix = yuv_matrix(source);
            for (std::uint32_t y = 0; y < height; ++y) {
                const auto input_y = source_row(y, height, source.flip);
                const auto *y_row = source.planes[0].data +
                                    static_cast<std::size_t>(input_y) * source.planes[0].stride;
                const auto *uv_row = source.planes[1].data +
                                     static_cast<std::size_t>(input_y / 2u) * source.planes[1].stride;
                for (std::uint32_t x = 0; x < width; ++x) {
                    const auto uv_index = (x / 2u) * 2u;
                    write_yuv_pixel(result, x, y, y_row[x], uv_row[uv_index], uv_row[uv_index + 1],
                                    matrix);
                }
            }
            return result;
        }
        case SourcePixelFormat::i420: {
            const auto chroma_width = width / 2u + width % 2u;
            const auto chroma_height = height / 2u + height % 2u;
            if (!valid_plane(source.planes[0], height, width) ||
                !valid_plane(source.planes[1], chroma_height, chroma_width) ||
                !valid_plane(source.planes[2], chroma_height, chroma_width))
                return std::nullopt;
            const auto matrix = yuv_matrix(source);
            for (std::uint32_t y = 0; y < height; ++y) {
                const auto input_y = source_row(y, height, source.flip);
                const auto *y_row = source.planes[0].data +
                                    static_cast<std::size_t>(input_y) * source.planes[0].stride;
                const auto *u_row = source.planes[1].data +
                                    static_cast<std::size_t>(input_y / 2u) * source.planes[1].stride;
                const auto *v_row = source.planes[2].data +
                                    static_cast<std::size_t>(input_y / 2u) * source.planes[2].stride;
                for (std::uint32_t x = 0; x < width; ++x)
                    write_yuv_pixel(result, x, y, y_row[x], u_row[x / 2u], v_row[x / 2u], matrix);
            }
            return result;
        }
        case SourcePixelFormat::unsupported:
            return std::nullopt;
        }
    } catch (const std::exception &) {
        return std::nullopt;
    }
    return std::nullopt;
}

} // namespace church
