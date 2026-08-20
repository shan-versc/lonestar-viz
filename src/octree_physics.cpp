// ─────────────────────────────────────────────────────────────────────────────
// octree_physics.cpp — Octree-accelerated repulsion for the force-directed layout
// ─────────────────────────────────────────────────────────────────────────────
#include "octree_physics.hpp"
#include "components.hpp"
#include <glm/glm.hpp>
#include <cmath>
#include <vector>

namespace aarf {

void OctreePhysicsSystem::step(CartographerWorld& world, float dt) {
    if (paused()) return;

    // Rebuild the octree from the *current* node positions every step so
    // neighbour queries never hit a stale spatial index (positions moved
    // during the previous tick).
    rebuild_octree(world);

    accumulate_repulsion_octree(world);

    // Springs and value impulses from base class (they don't benefit from
    // spatial acceleration at these edge counts)
    accumulate_springs(world);
    apply_value_impulses(world);
    world.tick(dt);
}

void OctreePhysicsSystem::rebuild_octree(CartographerWorld& world) {
    octree_.clear();
    auto& reg  = world.reg();
    auto  view = reg.view<NodeComponent>();
    for (auto e : view)
        octree_.insert(e, view.get<NodeComponent>(e).pos);
}

void OctreePhysicsSystem::accumulate_repulsion_octree(CartographerWorld& world) {
    auto& reg  = world.reg();
    auto  view = reg.view<NodeComponent>();
    const float k_r   = config().k_repulse;
    const float f_max = config().max_force;

    for (auto e : view) {
        auto& ni = view.get<NodeComponent>(e);

        // Query only neighbours within cull_radius
        auto neighbours = octree_.query_sphere(ni.pos, cull_radius);

        for (auto other : neighbours) {
            if (other == e) continue;
            if (!reg.valid(other) || !view.contains(other)) continue;

            const auto& nj = view.get<NodeComponent>(other);
            glm::vec3   d  = ni.pos - nj.pos;
            float       r2 = glm::dot(d, d) + 0.01f;
            if (r2 < 0.001f) continue;

            float     r   = std::sqrt(r2);
            float     f   = std::min(k_r / r2, f_max);
            ni.force      += (d / r) * f;
            // Note: we only update ni here; nj gets its own pass.
            // This is correct for octree queries (not pair-wise).
        }
    }
}

} // namespace aarf
