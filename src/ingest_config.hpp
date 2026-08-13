#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// ingest_config.hpp — Runtime-configurable parameters for IngestSystem.
//
// Centralises all magic numbers so they can be tweaked via ImGui sliders
// or loaded from a JSON config file at startup.
// ─────────────────────────────────────────────────────────────────────────────
#include <cstdint>

namespace aarf {

struct IngestConfig {
    /// Target ingest tick period in milliseconds (~60 Hz default).
    uint32_t tick_ms{16};

    /// Number of nodes to update per tick (random walk).
    int nodes_per_tick_min{8};
    int nodes_per_tick_max{24};

    /// Random walk step std-dev (Gaussian).
    float walk_sigma{0.08f};

    /// Cascade burst: min/max affected nodes.
    int burst_size_min{5};
    int burst_size_max{20};

    /// Frames between cascade bursts (min/max).
    int burst_interval_min{180};
    int burst_interval_max{480};

    /// Spike value floor during a cascade.
    float burst_spike_floor{0.7f};

    /// Back-pressure: max retries when queue is full before dropping.
    int push_max_retries{8};

    /// If true, the ingest thread prints a drop warning every 1000 drops.
    bool warn_on_drop{true};
};

} // namespace aarf
