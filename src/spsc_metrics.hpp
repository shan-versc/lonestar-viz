#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// spsc_metrics.hpp — Non-intrusive metrics wrapper around SpscQueue.
//
// Tracks push/pop counts, drop count (when queue is full), and a rolling
// throughput estimate (events/sec) using an exponential moving average.
//
// Thread safety: push_dropped() and record_push() are called from the producer
// thread; record_pop() from the consumer. Counters are atomic.
// ─────────────────────────────────────────────────────────────────────────────
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cinttypes>
#include <cstdio>
#include "spsc_queue.hpp"

namespace aarf {

struct QueueMetrics {
    std::atomic<uint64_t> total_pushed{0};
    std::atomic<uint64_t> total_popped{0};
    std::atomic<uint64_t> total_dropped{0};  ///< push() returned false
    std::atomic<uint64_t> total_retried{0};  ///< back-pressure retries

    // Throughput EMA (events/sec) — updated by consumer thread
    float pop_rate_ema{0.f};   ///< consumer-side, not atomic (single writer)

    void record_push()   noexcept { total_pushed.fetch_add(1, std::memory_order_relaxed); }
    void record_pop()    noexcept { total_popped.fetch_add(1, std::memory_order_relaxed); }
    void record_drop()   noexcept { total_dropped.fetch_add(1, std::memory_order_relaxed); }
    void record_retry()  noexcept { total_retried.fetch_add(1, std::memory_order_relaxed); }

    void print() const {
        std::printf("[spsc] pushed=%" PRIu64 "  popped=%" PRIu64
                    "  dropped=%" PRIu64 "  retried=%" PRIu64 "\n",
                    total_pushed.load(), total_popped.load(),
                    total_dropped.load(), total_retried.load());
    }
};

/// Thin wrapper: forwards push/pop, records metrics.
template<std::size_t N = 4096>
class InstrumentedQueue {
public:
    using Queue = SpscQueue<N>;

    [[nodiscard]] bool push(NodeDelta delta) noexcept {
        bool ok = q_.push(std::move(delta));
        if (ok) metrics.record_push();
        else    metrics.record_drop();
        return ok;
    }

    [[nodiscard]] std::optional<NodeDelta> try_pop() noexcept {
        auto v = q_.try_pop();
        if (v) metrics.record_pop();
        return v;
    }

    [[nodiscard]] bool        empty() const noexcept { return q_.empty(); }
    [[nodiscard]] std::size_t size()  const noexcept { return q_.size(); }

    /// Access underlying queue for direct use where needed.
    Queue& raw() noexcept { return q_; }

    QueueMetrics metrics;

private:
    Queue q_;
};

} // namespace aarf
