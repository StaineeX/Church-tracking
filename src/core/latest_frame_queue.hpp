// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "core/frame_types.hpp"

#include <condition_variable>
#include <mutex>
#include <optional>
#include <utility>

namespace church {

class LatestFrameQueue {
public:
    // A busy queue lock drops this analysis frame; it never stalls a producer.
    bool submit(VideoFrame frame)
    {
        std::unique_lock lock(mutex_, std::try_to_lock);
        if (!lock || closed_)
            return false;
        pending_ = std::move(frame);
        lock.unlock();
        ready_.notify_one();
        return true;
    }

    bool try_take(VideoFrame &out)
    {
        std::lock_guard lock(mutex_);
        return take_locked(out);
    }

    bool wait_take(VideoFrame &out)
    {
        std::unique_lock lock(mutex_);
        ready_.wait(lock, [&] { return closed_ || pending_.has_value(); });
        return take_locked(out);
    }

    void close()
    {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
            pending_.reset();
        }
        ready_.notify_all();
    }

private:
    bool take_locked(VideoFrame &out)
    {
        if (!pending_)
            return false;
        out = std::move(*pending_);
        pending_.reset();
        return true;
    }

    std::mutex mutex_;
    std::condition_variable ready_;
    std::optional<VideoFrame> pending_;
    bool closed_ = false;
};

} // namespace church
