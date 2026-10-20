// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "core/detector.hpp"
#include "core/latest_frame_queue.hpp"
#include "core/snapshot_store.hpp"
#include "core/tracker.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace church {

class TrackingWorker {
public:
    explicit TrackingWorker(std::unique_ptr<Detector> detector = nullptr,
                            std::unique_ptr<Tracker> tracker = nullptr,
                            MotionSettings settings = {});
    ~TrackingWorker();
    TrackingWorker(const TrackingWorker &) = delete;
    TrackingWorker &operator=(const TrackingWorker &) = delete;

    void start();
    bool submit(VideoFrame frame);
    void invalidate();
    std::shared_ptr<const CropState> snapshot() const;
    void stop();

private:
    void run();

    LatestFrameQueue queue_;
    SnapshotStore<CropState> snapshots_;
    std::unique_ptr<Detector> detector_;
    std::unique_ptr<Tracker> tracker_;
    MotionEngine motion_;
    std::thread thread_;
    mutable std::mutex snapshot_mutex_;
    std::atomic<std::uint64_t> invalidation_epoch_{0};
};

} // namespace church
