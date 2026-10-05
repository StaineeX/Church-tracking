// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/geometry.hpp"

#include <cmath>

namespace church {

bool is_valid(const NormalizedRect &rect)
{
    return std::isfinite(rect.left) && std::isfinite(rect.top) &&
           std::isfinite(rect.right) && std::isfinite(rect.bottom) &&
           rect.left >= 0.0f && rect.top >= 0.0f &&
           rect.right <= 1.0f && rect.bottom <= 1.0f &&
           rect.left < rect.right && rect.top < rect.bottom;
}

} // namespace church
