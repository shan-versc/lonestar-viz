#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// components.hpp — EnTT ECS components for the 3D Causal Cartographer
// ─────────────────────────────────────────────────────────────────────────────
#include <deque>
#include <string>
#include <glm/glm.hpp>
#include <entt/entt.hpp>

namespace aarf {

/// Primary spatial/simulation node.
struct NodeComponent {
    glm::vec3   pos{0.f};
    glm::vec3   vel{0.f};
    glm::vec3   force{0.f};
    float       value{0.f};       ///< normalised [0,1]
    float       prev_value{0.f};
    bool        active{true};
    std::string name;
};

/// Directed dependency edge between two nodes.
struct EdgeComponent {
    entt::entity src{entt::null};
    entt::entity dst{entt::null};
    float        strength{1.f};
};

/// Rolling history ring for temporal scrubbing.
struct HistoryComponent {
    static constexpr std::size_t kMaxHistory = 128;
    std::deque<float> values;

    void push(float v) {
        values.push_back(v);
        if (values.size() > kMaxHistory)
            values.pop_front();
    }
};

/// Tag: this node has been collapsed into a cluster representative.
struct ClusterTag {};

/// Tag: node is selected (raycasted).
struct SelectedTag {};

} // namespace aarf
