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
    octree_.rebuild_masses();   // for Barnes-Hut centre-of-mass aggregation
}

void OctreePhysicsSystem::accumulate_repulsion_octree(CartographerWorld& world) {
    auto& reg  = world.reg();
    auto  view = reg.view<NodeComponent>();
    const float k_r   = config().k_repulse;
    const float f_max = config().max_force;

    for (auto e : view) {
        auto& ni = view.get<NodeComponent>(e);

        // Near field: exact pairwise repulsion for neighbours within cull_radius.
        auto neighbours = octree_.query_sphere(ni.pos, cull_radius);
        for (auto other : neighbours) {
            if (other == e) continue;
            if (!reg.valid(other) || !view.contains(other)) continue;

            const auto& nj = view.get<NodeComponent>(other);
            glm::vec3   d  = ni.pos - nj.pos;
            float       r2 = glm::dot(d, d) + 0.01f;
            if (r2 < 0.001f) continue;

            float r = std::sqrt(r2);
            float f = std::min(k_r / r2, f_max);
            ni.force += (d / r) * f;
        }

        // Far field: Barnes-Hut aggregation over the octree — O(log n) cells
        // approximate the repulsion of everything beyond cull_radius.
        accumulate_bh_far_field(ni.force, ni.pos, octree_.root(), k_r, f_max);
    }
}

void OctreePhysicsSystem::accumulate_bh_far_field(
    glm::vec3& force, const glm::vec3& pos,
    const OctreeNode& node, float k_r, float f_max) const {
    if (node.mass <= 0.f) return;

    const float s = 2.f * node.bounds.half;
    const float d = std::max(glm::length(node.com - pos), 1e-5f);

    // Cell fully inside the exact near field → handled by the pairwise pass.
    if (d + s < cull_radius) return;
    // Cell fully beyond the aggregated far-field range → ignore.
    if (d - s > bh_radius)    return;

    // Collapse to centre of mass if the cell is small/far enough, or is a leaf.
    if (node.is_leaf || (s / d) < theta) {
        const float r2 = glm::dot(node.com - pos, node.com - pos) + 0.01f;
        const float r  = std::sqrt(r2);
        float       f  = std::min(k_r * node.mass / r2, f_max);
        force += ((node.com - pos) / r) * f;
        return;
    }

    for (const auto& c : node.children)
        if (c) accumulate_bh_far_field(force, pos, *c, k_r, f_max);
}

} // namespace aarf
