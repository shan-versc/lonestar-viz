#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// octree_bench.hpp — Simple wall-clock benchmark for physics step comparison.
//
// Usage:
//   OctreeBench bench;
//   bench.run_naive(world, n_steps);
//   bench.run_octree(world, n_steps);
//   bench.print_report();
// ─────────────────────────────────────────────────────────────────────────────
#include <chrono>
#include <cstdio>
#include "registry.hpp"
#include "physics.hpp"
#include "octree_physics.hpp"

namespace aarf {

struct BenchResult {
    double total_ms{0};
    double per_step_us{0};
    int    steps{0};
};

class OctreeBench {
public:
    BenchResult run_naive(CartographerWorld& world, int steps = 200) {
        PhysicsSystem sys;
        return time_steps(world, sys, steps);
    }

    BenchResult run_octree(CartographerWorld& world, int steps = 200) {
        OctreePhysicsSystem sys;
        return time_steps(world, sys, steps);
    }

    static void print_report(const BenchResult& naive,
                              const BenchResult& octree,
                              std::size_t node_count) {
        std::printf("\n── Physics Benchmark ─────────────────────────────\n");
        std::printf("  Nodes        : %zu\n", node_count);
        std::printf("  Naive   O(n²): %.2f ms/step  (%.0f µs)\n",
                    naive.per_step_us / 1000.0, naive.per_step_us);
        std::printf("  Octree O(n lg n): %.2f ms/step  (%.0f µs)\n",
                    octree.per_step_us / 1000.0, octree.per_step_us);
        std::printf("  Speedup      : %.2fx\n",
                    naive.per_step_us / (octree.per_step_us + 1e-9));
        std::printf("──────────────────────────────────────────────────\n\n");
    }

private:
    template<typename Sys>
    BenchResult time_steps(CartographerWorld& world, Sys& sys, int steps) {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();
        for (int i = 0; i < steps; ++i)
            sys.step(world, 0.016f);
        auto t1 = clock::now();
        double total = std::chrono::duration<double, std::milli>(t1 - t0).count();
        return { total, total * 1000.0 / steps, steps };
    }
};

} // namespace aarf
