// ─────────────────────────────────────────────────────────────────────────────
// registry.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "registry.hpp"
#include <cmath>

namespace aarf {

entt::entity CartographerWorld::create_node(const std::string& name,
                                             const glm::vec3&   pos,
                                             float              value) {
    auto e = reg_.create();
    auto& n      = reg_.emplace<NodeComponent>(e);
    n.pos        = pos;
    n.value      = value;
    n.prev_value = value;
    n.active     = true;
    n.name       = name;
    reg_.emplace<HistoryComponent>(e).push(value);
    ++node_count_;
    return e;
}

entt::entity CartographerWorld::create_edge(entt::entity src,
                                             entt::entity dst,
                                             float        strength) {
    auto e = reg_.create();
    auto& ed = reg_.emplace<EdgeComponent>(e);
    ed.src      = src;
    ed.dst      = dst;
    ed.strength = strength;
    ++edge_count_;
    return e;
}

void CartographerWorld::update_node_value(entt::entity e, float new_val) {
    if (!reg_.valid(e)) return;
    auto& n       = reg_.get<NodeComponent>(e);
    n.prev_value  = n.value;
    n.value       = new_val;
    n.active      = true;

    if (reg_.all_of<HistoryComponent>(e))
        reg_.get<HistoryComponent>(e).push(new_val);
}

void CartographerWorld::apply_impulse(entt::entity e, const glm::vec3& impulse) {
    if (!reg_.valid(e)) return;
    reg_.get<NodeComponent>(e).vel += impulse;
}

void CartographerWorld::tick(float dt) {
    constexpr float kDamping = 0.92f;
    auto view = reg_.view<NodeComponent>();
    for (auto e : view) {
        auto& n  = view.get<NodeComponent>(e);
        n.vel   += n.force * dt;
        n.vel   *= kDamping;
        n.pos   += n.vel * dt;
        n.force  = glm::vec3(0.f);  // reset; physics will re-accumulate
    }
}

std::vector<entt::entity> CartographerWorld::get_active_nodes() const {
    std::vector<entt::entity> out;
    auto view = reg_.view<NodeComponent>();
    for (auto e : view)
        if (view.get<NodeComponent>(e).active)
            out.push_back(e);
    return out;
}

std::vector<entt::entity> CartographerWorld::get_all_nodes() const {
    std::vector<entt::entity> out;
    auto view = reg_.view<NodeComponent>();
    for (auto e : view) out.push_back(e);
    return out;
}

std::vector<entt::entity> CartographerWorld::get_all_edges() const {
    std::vector<entt::entity> out;
    auto view = reg_.view<EdgeComponent>();
    for (auto e : view) out.push_back(e);
    return out;
}

} // namespace aarf
