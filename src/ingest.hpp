#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// ingest.hpp — Background simulation data ingest into SPSC queue
// ─────────────────────────────────────────────────────────────────────────────
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <memory>
#include "spsc_queue.hpp"
#include "registry.hpp"

namespace aarf {

class IngestSystem {
public:
    using Queue = SpscQueue<4096>;

    IngestSystem()  = default;
    ~IngestSystem() { shutdown(); }

    /// Spawn ingest thread. world must outlive this object.
    void init(CartographerWorld& world, Queue* queue);

    /// Drain all pending deltas from the queue into the world.
    /// Call from render/main thread every frame.
    void drain(CartographerWorld& world);

    /// Signal stop and join ingest thread.
    void shutdown();

    [[nodiscard]] std::size_t total_events() const noexcept {
        return total_events_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::size_t queue_approx_size() const noexcept {
        return queue_ ? queue_->size() : 0;
    }

private:
    void ingest_loop(CartographerWorld& world);

    Queue*        queue_{nullptr};
    std::thread   thread_;
    std::atomic_bool stop_{false};
    std::atomic<std::size_t> total_events_{0};

    /// Lookup table: feature_name → entity (built during init)
    std::vector<std::string>   names_;
    std::vector<entt::entity>  entities_;
};

} // namespace aarf
