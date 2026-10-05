// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <atomic>
#include <memory>
#include <utility>

namespace church {

template <typename T> class SnapshotStore {
public:
    void publish(T value)
    {
        std::shared_ptr<const T> complete = std::make_shared<const T>(std::move(value));
        snapshot_.store(std::move(complete), std::memory_order_release);
    }

    std::shared_ptr<const T> read() const
    {
        return snapshot_.load(std::memory_order_acquire);
    }

private:
    std::atomic<std::shared_ptr<const T>> snapshot_;
};

} // namespace church
