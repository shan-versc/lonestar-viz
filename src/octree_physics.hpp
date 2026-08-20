#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// octree_physics.hpp — O(n log n) force accumulation using spatial octree.
//
// The naive O(n²) repulsion in physics.cpp is replaced here with a Barnes-Hut
// style approximation: for each node, query the octree for nearby neighbours
// within a threshold radius; only those receive exact Coulomb forces.  Nodes
// in distant octree cells contribute an approximate aggregate force computed
// from the cell's centre-of-mass and total weight.
// ─────────────────────────────────────────────────────────────────────────────
#include "physics.hpp"
#include "octree.hpp"
#include "registry.hpp"

namespace aarf {

/// Extended physics system that rebuilds and queries an internal octree
/// to restrict repulsion calculations to O(n log n).
class OctreePhysicsSystem : public PhysicsSystem {
public:
    explicit OctreePhysicsSystem(PhysicsConfig cfg = {})
        : PhysicsSystem(cfg), octree_(120.f) {}

    /// Drop-in replacement for PhysicsSystem::step().
    void step(CartographerWorld& world, float dt);

    /// Expose octree for external LOD queries (renderer, raycaster).
    [[nodiscard]] const Octree& octree() const noexcept { return octree_; }

    /// Radius within which repulsion is computed exactly (near field).
    float cull_radius{55.f};
    /// Maximum aggregated range for the Barnes-Hut far field (world extent).
    float bh_radius{130.f};
    /// Barnes-Hut openness criterion s/d: below this, cells are collapsed to
    /// their centre of mass (0 = exact, ~0.5-0.8 = typical approximation).
    float theta{0.7f};

private:
    void rebuild_octree(CartographerWorld& world);
    void accumulate_repulsion_octree(CartographerWorld& world);
    void accumulate_bh_far_field(glm::vec3&       force,
                                 const glm::vec3& pos,
                                 const OctreeNode& node,
                                 float k_r, float f_max) const;

    Octree octree_;
};

} // namespace aarf
