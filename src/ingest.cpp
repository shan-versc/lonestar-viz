// ─────────────────────────────────────────────────────────────────────────────
// ingest.cpp — Simulates a live data stream from a running simulation.
//
// The ingest thread produces NodeDelta events at ~60Hz. In a real deployment
// this would read from a Unix socket, shared memory, or HTTP/SSE stream.
// Here we generate synthetic data: random walks per register, with occasional
// cascading "failure" bursts that ripple through a neighbourhood.
// ─────────────────────────────────────────────────────────────────────────────
#include "ingest.hpp"
#include "components.hpp"
#include <chrono>
#include <random>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>

namespace aarf {

void IngestSystem::init(CartographerWorld& world, Queue* queue) {
    queue_ = queue;
    stop_.store(false);

    // Build name → entity lookup from all existing nodes
    auto& reg  = world.reg();
    auto  view = reg.view<NodeComponent>();
    names_.clear(); entities_.clear();
    for (auto e : view) {
        names_.push_back(reg.get<NodeComponent>(e).name);
        entities_.push_back(e);
    }

    thread_ = std::thread([this, &world]{ ingest_loop(world); });
}

void IngestSystem::drain(CartographerWorld& world) {
    while (auto opt = queue_->try_pop()) {
        // Find entity by name (linear search — acceptable for 200 nodes)
        const auto it = std::find(names_.begin(), names_.end(),
                                  opt->feature_name);
        if (it != names_.end()) {
            const std::size_t idx = static_cast<std::size_t>(
                std::distance(names_.begin(), it));
            world.update_node_value(entities_[idx], opt->value);
        }
        total_events_.fetch_add(1, std::memory_order_relaxed);
    }
}

void IngestSystem::shutdown() {
    stop_.store(true);
    if (thread_.joinable()) thread_.join();
}

void IngestSystem::ingest_loop(CartographerWorld& /*world*/) {
    std::mt19937                          rng{std::random_device{}()};
    std::uniform_int_distribution<int>    pick(0, (int)names_.size() - 1);
    std::uniform_int_distribution<int>    burst_count(8, 24);
    std::uniform_real_distribution<float> frand(0.f, 1.f);
    std::normal_distribution<float>       walk(0.f, 0.08f);

    // Per-register current values (independent of ECS — ingest owns them)
    std::vector<float> values(names_.size(), 0.f);
    for (auto& v : values) v = frand(rng);

    using clock    = std::chrono::steady_clock;
    using ms       = std::chrono::milliseconds;
    const auto tick_period = ms(16);  // ~60 Hz
    auto next_tick = clock::now() + tick_period;

    // Occasionally trigger a "cascade" burst
    int frames_until_burst = 120;

    while (!stop_.load(std::memory_order_relaxed)) {
        // Random walk a subset of registers
        const int n_active = burst_count(rng);
        for (int i = 0; i < n_active; ++i) {
            int   idx  = pick(rng);
            float prev = values[idx];
            float next = glm::clamp(prev + walk(rng), 0.f, 1.f);
            values[idx] = next;

            NodeDelta d;
            d.feature_name = names_[idx];
            d.value        = next;
            d.prev_value   = prev;

            using namespace std::chrono;
            d.ts_ms = duration_cast<duration<double, std::milli>>(
                clock::now().time_since_epoch()).count();

            // Spin-retry if queue is full (back-pressure: ingest slows down)
            int retries = 8;
            while (!queue_->push(d) && retries-- > 0)
                std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        // Cascading failure burst
        if (--frames_until_burst <= 0) {
            frames_until_burst = std::uniform_int_distribution<int>(180, 480)(rng);
            const int cascade_center = pick(rng);
            const int cascade_radius = std::uniform_int_distribution<int>(5, 20)(rng);
            for (int i = cascade_center;
                 i < std::min(cascade_center + cascade_radius,
                              (int)names_.size()); ++i)
            {
                float spike      = 0.7f + frand(rng) * 0.3f;
                float prev       = values[i];
                values[i]        = spike;
                NodeDelta d;
                d.feature_name   = names_[i];
                d.value          = spike;
                d.prev_value     = prev;
                d.intent         = "cascade";
                queue_->push(d);  // best-effort during burst
            }
        }

        std::this_thread::sleep_until(next_tick);
        next_tick += tick_period;
    }
}

} // namespace aarf
