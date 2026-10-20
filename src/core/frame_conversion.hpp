// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "core/frame_types.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace church {

enum class SourcePixelFormat { unsupported, rgba, bgra, bgrx, bgr3, nv12, i420 };

struct SourcePlaneView {
    const std::uint8_t *data = nullptr;
    std::uint32_t stride = 0;
};

struct SourceFrameView {
    SourcePixelFormat format = SourcePixelFormat::unsupported;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::array<SourcePlaneView, 3> planes{};
    std::uint64_t timestamp_ns = 0;
    std::array<float, 16> color_matrix{};
    bool full_range = false;
    bool flip = false;
};

std::optional<VideoFrame> convert_source_frame(const SourceFrameView &source);

} // namespace church
