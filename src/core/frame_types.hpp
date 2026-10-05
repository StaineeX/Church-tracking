// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <vector>

namespace church {

enum class PixelFormat { rgba8 };

struct VideoFrame {
    std::vector<std::uint8_t> pixels;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t stride = 0;
    PixelFormat format = PixelFormat::rgba8;
    std::int64_t timestamp_ns = 0;
};

} // namespace church
