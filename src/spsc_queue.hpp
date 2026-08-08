#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// spsc_queue.hpp  —  Lock-free, wait-free Single-Producer / Single-Consumer
//                    ring-buffer queue.
//
// Usage contract:
//   • Exactly ONE thread calls push().
//   • Exactly ONE thread calls pop() / try_pop().
//   • Capacity must be a power of two (asserted at construction).
//
// The queue stores NodeDelta structs: lightweight change events emitted by the
// ingest thread and consumed by the render/physics thread.
// ─────────────────────────────────────────────────────────────────────────────
#include <atomic>
#include <array>
#include <cassert>
#include <cstddef>
#include <optional>
#include <string>

namespace aarf {

/// One delta event: a named feature value changed.
struct NodeDelta {
    std::string feature_name;   ///< e.g. "dog_present", "motion"
    float       value{0.f};     ///< new value in [0,1] (normalised by ingest)
    float       prev_value{0.f};///< previous value (for force magnitude)
    double      ts_ms{0.0};     ///< wall-clock timestamp (ms since epoch)
    /// Optional full-prediction snapshot (filled on each SSE event).
    std::string intent;
    std::string emotion;
    std::string behavior;
    float       confidence{0.f};
    std::string gate;           ///< "pass" | "review" | "reject"
};

/// Lock-free SPSC ring-buffer.
/// N must be a power of two.
template<std::size_t N = 4096>
class SpscQueue {
    static_assert((N & (N - 1)) == 0, "SpscQueue capacity must be a power of two");
public:
    SpscQueue() = default;

    // ── Producer side ────────────────────────────────────────────────────────
    /// Returns false if the queue is full (producer should back-off or drop).
    [[nodiscard]] bool push(NodeDelta delta) noexcept {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next = (head + 1) & mask_;
        if (next == tail_.load(std::memory_order_acquire))
            return false;  // full
        buf_[head] = std::move(delta);
        head_.store(next, std::memory_order_release);
        return true;
    }

    // ── Consumer side ────────────────────────────────────────────────────────
    [[nodiscard]] std::optional<NodeDelta> try_pop() noexcept {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire))
            return std::nullopt;  // empty
        NodeDelta out = std::move(buf_[tail]);
        tail_.store((tail + 1) & mask_, std::memory_order_release);
        return out;
    }

    [[nodiscard]] bool empty() const noexcept {
        return tail_.load(std::memory_order_acquire)
            == head_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::size_t size() const noexcept {
        const std::size_t h = head_.load(std::memory_order_acquire);
        const std::size_t t = tail_.load(std::memory_order_acquire);
        return (h - t) & mask_;
    }

private:
    static constexpr std::size_t mask_ = N - 1;
    std::array<NodeDelta, N> buf_{};
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
};

} // namespace aarf
