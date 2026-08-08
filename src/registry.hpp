#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// registry.hpp — ECS world wrapper around entt::registry
// ─────────────────────────────────────────────────────────────────────────────
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <entt/entt.hpp>
#include "components.hpp"

namespace aarf {

class CartographerWorld {
public:
    CartographerWorld() = default;

    // ── Entity factories ─────────────────────────────────────────────────────

    entt::entity create_node(const std::string& name,
                              const glm::vec3&   pos,
                              float              value = 0.f);

    entt::entity create_edge(entt::entity src,
                              entt::entity dst,
                              float        strength = 1.f);

    // ── Mutation ─────────────────────────────────────────────────────────────

    void update_node_value(entt::entity e, float new_val);

    void apply_impulse(entt::entity e, const glm::vec3& impulse);

    /// Advance velocities and apply damping (called before physics step).
    void tick(float dt);

    // ── Queries ──────────────────────────────────────────────────────────────

    [[nodiscard]] std::vector<entt::entity> get_active_nodes() const;
    [[nodiscard]] std::vector<entt::entity> get_all_nodes()    const;
    [[nodiscard]] std::vector<entt::entity> get_all_edges()    const;

    [[nodiscard]] std::size_t node_count() const noexcept { return node_count_; }
    [[nodiscard]] std::size_t edge_count() const noexcept { return edge_count_; }

    // ── Direct registry access (for systems) ─────────────────────────────────
    [[nodiscard]] entt::registry&       reg()       noexcept { return reg_; }
    [[nodiscard]] const entt::registry& reg() const noexcept { return reg_; }

private:
    entt::registry reg_;
    std::size_t    node_count_{0};
    std::size_t    edge_count_{0};
};

} // namespace aarf
