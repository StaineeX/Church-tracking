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
    // Internal worker generation. A frame submitted before real-frame
    // invalidation must not republish its stale crop afterward.
    std::uint64_t processing_epoch = 0;
};

} // namespace church
