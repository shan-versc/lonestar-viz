// ─────────────────────────────────────────────────────────────────────────────
// physics.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "physics.hpp"
#include "components.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <vector>

namespace aarf {

void PhysicsSystem::step(CartographerWorld& world, float dt) {
    if (paused_) return;
    accumulate_repulsion(world);
    accumulate_springs(world);
    apply_value_impulses(world);
    world.tick(dt);
}

void PhysicsSystem::accumulate_repulsion(CartographerWorld& world) {
    auto& reg  = world.reg();
    auto  view = reg.view<NodeComponent>();

    // O(n^2) with early-out for large distances — acceptable up to ~2000 nodes.
    // For larger counts use octree spatial queries (see feature/octree-physics).
    std::vector<entt::entity> nodes(view.begin(), view.end());
    const std::size_t N = nodes.size();

    for (std::size_t i = 0; i < N; ++i) {
        auto& ni = view.get<NodeComponent>(nodes[i]);
        for (std::size_t j = i + 1; j < N; ++j) {
            auto& nj  = view.get<NodeComponent>(nodes[j]);
            glm::vec3 d = ni.pos - nj.pos;
            float     r2 = glm::dot(d, d) + 0.01f;  // epsilon to avoid div/0
            if (r2 > 3600.f) continue;               // skip if > 60 units

            float r       = std::sqrt(r2);
            glm::vec3 dir = d / r;
            float     f   = cfg_.k_repulse / r2;
            f             = std::min(f, cfg_.max_force);

            ni.force += dir * f;
            nj.force -= dir * f;
        }
    }
}

void PhysicsSystem::accumulate_springs(CartographerWorld& world) {
    auto& reg       = world.reg();
    auto  edge_view = reg.view<EdgeComponent>();
    auto  node_view = reg.view<NodeComponent>();

    for (auto ee : edge_view) {
        const auto& edge = edge_view.get<EdgeComponent>(ee);
        if (!reg.valid(edge.src) || !reg.valid(edge.dst)) continue;
        if (!node_view.contains(edge.src) || !node_view.contains(edge.dst)) continue;

        auto& ns = node_view.get<NodeComponent>(edge.src);
        auto& nd = node_view.get<NodeComponent>(edge.dst);

        glm::vec3 d   = nd.pos - ns.pos;
        float     len = glm::length(d);
        if (len < 0.001f) continue;

        float     stretch = len - cfg_.rest_len * edge.strength;
        glm::vec3 f_dir   = glm::normalize(d);
        float     f_mag   = cfg_.k_spring * stretch;
        f_mag             = glm::clamp(f_mag, -cfg_.max_force, cfg_.max_force);

        ns.force += f_dir * f_mag;
        nd.force -= f_dir * f_mag;
    }
}

void PhysicsSystem::apply_value_impulses(CartographerWorld& world) {
    auto& reg  = world.reg();
    auto  view = reg.view<NodeComponent>();

    for (auto e : view) {
        auto& n   = view.get<NodeComponent>(e);
        float d   = std::abs(n.value - n.prev_value);
        if (d < 0.05f) continue;

        // Deterministic pseudo-random direction from entity id
        const uint32_t id  = static_cast<uint32_t>(entt::to_integral(e));
        float          ang = (id * 2654435761u & 0xFFFFFF) / float(0xFFFFFF)
                              * glm::two_pi<float>();
        float          el  = (((id * 1234567891u) & 0xFFFF) / float(0xFFFF) - 0.5f)
                              * glm::pi<float>();
        glm::vec3 dir {
            std::cos(el) * std::cos(ang),
            std::sin(el),
            std::cos(el) * std::sin(ang)
        };
        n.vel += dir * d * cfg_.impulse_k;
    }
}

void PhysicsSystem::reset_velocities(CartographerWorld& world) {
    auto& reg  = world.reg();
    auto  view = reg.view<NodeComponent>();
    for (auto e : view) {
        auto& n = view.get<NodeComponent>(e);
        n.vel   = glm::vec3(0.f);
        n.force = glm::vec3(0.f);
    }
}

} // namespace aarf
