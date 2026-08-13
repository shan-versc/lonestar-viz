#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// physics.hpp — Force-directed layout: Coulomb repulsion + Hooke attraction
// ─────────────────────────────────────────────────────────────────────────────
#include "registry.hpp"

namespace aarf {

struct PhysicsConfig {
    float k_repulse   = 80.f;   ///< Coulomb constant
    float k_spring    = 0.08f;  ///< Hooke spring constant
    float rest_len    = 8.f;    ///< Spring rest length (units)
    float damping     = 0.85f;  ///< Per-frame velocity damping
    float max_force   = 40.f;   ///< Force clamp to prevent explosion
    float active_dist = 25.f;   ///< Radius around active nodes to simulate
    float impulse_k   = 6.f;    ///< Value-delta → impulse magnitude

    /// Convergence hint: layout is considered stable when max node speed
    /// drops below this threshold for 30 consecutive frames.
    float convergence_speed = 0.05f;
};

class PhysicsSystem {
public:
    explicit PhysicsSystem(PhysicsConfig cfg = {}) : cfg_(cfg) {}

    /// Single simulation step. Accumulates forces, then calls world.tick(dt).
    void step(CartographerWorld& world, float dt);

    /// Reset all velocities (e.g. after camera reset / R key).
    void reset_velocities(CartographerWorld& world);

    PhysicsConfig& config() noexcept { return cfg_; }

private:
    void accumulate_repulsion(CartographerWorld& world);

protected:
    void accumulate_springs(CartographerWorld& world);
    void apply_value_impulses(CartographerWorld& world);

private:
    PhysicsConfig cfg_;
    bool          paused_{false};

public:
    void toggle_pause() noexcept { paused_ = !paused_; }
    [[nodiscard]] bool paused() const noexcept { return paused_; }
};

} // namespace aarf
