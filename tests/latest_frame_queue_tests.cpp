#include "core/latest_frame_queue.hpp"
#include "core/snapshot_store.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

namespace {
int failures = 0;
void check(bool ok, const char *message)
{
    if (!ok) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void latest_frame_queue_keeps_only_newest_owned_frame()
{
    church::LatestFrameQueue queue;
    std::vector<std::uint8_t> source{1, 2, 3, 4};
    church::VideoFrame first{source, 1, 1, 4, church::PixelFormat::rgba8, 10};
    check(queue.submit(first), "first frame accepted");
    source[0] = 99;
    check(queue.submit({{5, 6, 7, 8}, 1, 1, 4, church::PixelFormat::rgba8, 20}),
          "second frame accepted");
    church::VideoFrame taken;
    check(queue.try_take(taken), "pending frame available");
    check(taken.timestamp_ns == 20 && taken.pixels == std::vector<std::uint8_t>({5, 6, 7, 8}),
          "newest frame owns its pixels");
    check(!queue.try_take(taken), "only one frame remained");
    check(queue.submit(first), "lvalue frame accepted");
    first.pixels[0] = 77;
    check(queue.try_take(taken) && taken.pixels[0] == 1,
          "queue does not borrow a caller buffer");
    queue.close();
    check(!queue.submit(first), "closed queue rejects intake");
}

struct Versioned {
    int version;
    int check;
};

void snapshot_store_returns_last_complete_value_without_partial_updates()
{
    church::SnapshotStore<Versioned> store;
    check(!store.read(), "empty store has no snapshot");
    store.publish({1, 10});
    auto old = store.read();
    store.publish({2, 20});
    auto current = store.read();
    check(old && old->version == 1 && old->check == 10,
          "old reader keeps complete immutable value");
    check(current && current->version == 2 && current->check == 20,
          "new reader sees complete replacement");
    std::thread publisher([&] {
        for (int version = 3; version <= 10000; ++version)
            store.publish({version, version * 10});
    });
    for (int i = 0; i < 10000; ++i) {
        auto value = store.read();
        check(value && value->check == value->version * 10,
              "concurrent reader never sees partial update");
    }
    publisher.join();
}
} // namespace

int main()
{
    latest_frame_queue_keeps_only_newest_owned_frame();
    snapshot_store_returns_last_complete_value_without_partial_updates();
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
