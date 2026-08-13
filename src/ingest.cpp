// ─────────────────────────────────────────────────────────────────────────────
// ingest.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "ingest.hpp"
#include "components.hpp"
#include <chrono>
#include <random>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>

namespace aarf {

void IngestSystem::init(CartographerWorld& world, Queue* queue, IngestConfig cfg) {
    queue_ = queue;
    cfg_   = cfg;
    stop_.store(false);

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
        metrics_.record_pop();
        const auto it = std::find(names_.begin(), names_.end(), opt->feature_name);
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
    if (cfg_.warn_on_drop && metrics_.total_dropped.load() > 0)
        metrics_.print();
}

void IngestSystem::ingest_loop(CartographerWorld& /*world*/) {
    std::mt19937                          rng{std::random_device{}()};
    std::uniform_int_distribution<int>    pick(0, (int)names_.size() - 1);
    std::uniform_real_distribution<float> frand(0.f, 1.f);
    std::normal_distribution<float>       walk(0.f, cfg_.walk_sigma);

    std::vector<float> values(names_.size(), 0.f);
    for (auto& v : values) v = frand(rng);

    using clock = std::chrono::steady_clock;
    using ms    = std::chrono::milliseconds;
    const auto tick_period = ms(cfg_.tick_ms);
    auto next_tick = clock::now() + tick_period;

    std::uniform_int_distribution<int> n_active_dist(
        cfg_.nodes_per_tick_min, cfg_.nodes_per_tick_max);
    std::uniform_int_distribution<int> burst_size_dist(
        cfg_.burst_size_min, cfg_.burst_size_max);
    std::uniform_int_distribution<int> burst_interval_dist(
        cfg_.burst_interval_min, cfg_.burst_interval_max);

    int frames_until_burst = burst_interval_dist(rng);
    uint64_t drop_warn_threshold = 1000;

    while (!stop_.load(std::memory_order_relaxed)) {
        const int n_active = n_active_dist(rng);
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

            int retries = cfg_.push_max_retries;
            while (!queue_->push(d)) {
                metrics_.record_drop();
                if (--retries <= 0) break;
                metrics_.record_retry();
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            if (retries > 0) metrics_.record_push();

            // Warn on excessive drops
            if (cfg_.warn_on_drop) {
                uint64_t drops = metrics_.total_dropped.load(std::memory_order_relaxed);
                if (drops >= drop_warn_threshold) {
                    std::fprintf(stderr, "[ingest] %" PRIu64 " events dropped (queue full)\n",
                                 drops);
                    drop_warn_threshold *= 2;
                }
            }
        }

        // Cascade burst
        if (--frames_until_burst <= 0) {
            frames_until_burst = burst_interval_dist(rng);
            const int center = pick(rng);
            const int radius = burst_size_dist(rng);
            const int end    = std::min(center + radius, (int)names_.size());
            for (int i = center; i < end; ++i) {
                float spike    = cfg_.burst_spike_floor + frand(rng) * (1.f - cfg_.burst_spike_floor);
                float prev     = values[i];
                values[i]      = spike;
                NodeDelta d;
                d.feature_name = names_[i];
                d.value        = spike;
                d.prev_value   = prev;
                d.intent       = "cascade";
                if (queue_->push(d)) metrics_.record_push();
                else                 metrics_.record_drop();
            }
        }

        std::this_thread::sleep_until(next_tick);
        next_tick += tick_period;
    }
}

} // namespace aarf
