// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

namespace church {

struct NormalizedRect {
    float left;
    float top;
    float right;
    float bottom;
};

bool is_valid(const NormalizedRect &rect);

} // namespace church
