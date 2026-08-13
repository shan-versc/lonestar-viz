#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// drain_profiler.hpp — Measures time spent draining the SPSC queue each frame.
//
// Wraps IngestSystem::drain() with a high-resolution timer and accumulates
// rolling stats (min/max/avg drain time in µs) for display in the stats panel.
// ─────────────────────────────────────────────────────────────────────────────
#include <chrono>
#include <cstdint>
#include <algorithm>
#include <limits>
#include "ingest.hpp"
#include "registry.hpp"

namespace aarf {

struct DrainStats {
    double last_us{0};
    double min_us{std::numeric_limits<double>::max()};
    double max_us{0};
    double avg_us{0};   ///< exponential moving average
    uint64_t frame_count{0};
};

class DrainProfiler {
public:
    DrainStats stats;

    void drain(IngestSystem& sys, CartographerWorld& world) {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();
        sys.drain(world);
        auto t1 = clock::now();

        double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
        stats.last_us = us;
        stats.min_us  = std::min(stats.min_us, us);
        stats.max_us  = std::max(stats.max_us, us);
        // EMA with alpha=0.05
        stats.avg_us  = stats.avg_us * 0.95 + us * 0.05;
        ++stats.frame_count;
    }
};

} // namespace aarf
