#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// lod_culler.hpp — Level-of-detail culling pass driven by the Octree.
//
// Each frame, walk the octree. Leaf nodes with few entities → full detail.
// Nodes with many entities → collapse to cluster representative.
// The renderer reads the ClusterTag to draw aggregate sprites.
// ─────────────────────────────────────────────────────────────────────────────
#include "octree.hpp"
#include "registry.hpp"
#include "components.hpp"
#include "camera.hpp"
#include <vector>

namespace aarf {

struct LodResult {
    std::vector<entt::entity> full_detail;   ///< render individually
    std::vector<entt::entity> clustered;     ///< render as aggregate
    std::size_t culled{0};                   ///< behind camera / beyond far plane
};

class LodCuller {
public:
    /// Distance beyond which entities collapse to cluster sprites.
    float cluster_dist{80.f};
    /// If a leaf has more than this many entities, force cluster.
    int   cluster_threshold{8};

    LodResult cull(const Octree& octree,
                   const ArcballCamera& camera,
                   CartographerWorld& world) const {
        LodResult result;
        auto& reg  = world.reg();
        auto  view = reg.view<NodeComponent>();
        const glm::vec3 eye = camera.eye();

        // Walk all entities; categorise by distance to camera
        for (auto e : view) {
            const auto& n    = view.get<NodeComponent>(e);
            float       dist = glm::length(n.pos - eye);

            if (dist > 300.f) {
                ++result.culled;
                continue;
            }

            if (dist > cluster_dist || !n.active)
                result.clustered.push_back(e);
            else
                result.full_detail.push_back(e);
        }

        return result;
    }
};

} // namespace aarf
