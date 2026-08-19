#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// raycaster.hpp — Ray-sphere intersection for node selection.
//
// On left-click, construct a world-space ray from the camera eye through the
// mouse pixel.  Test against each node's bounding sphere (radius = size/2).
// Returns the entity with the nearest intersection, or entt::null.
// ─────────────────────────────────────────────────────────────────────────────
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <entt/entt.hpp>
#include <cmath>
#include <limits>
#include "registry.hpp"
#include "components.hpp"
#include "camera.hpp"

namespace aarf {

class Raycaster {
public:
    /// Node bounding sphere radius.
    float sphere_radius{2.5f};

    [[nodiscard]] entt::entity pick(
        float mouse_x, float mouse_y,
        int screen_w, int screen_h,
        const ArcballCamera& camera,
        CartographerWorld& world) const
    {
        // Build ray direction from NDC
        float nx =  (2.f * mouse_x / screen_w - 1.f);
        float ny = -(2.f * mouse_y / screen_h - 1.f);

        const float asp = (float)screen_w / (float)screen_h;
        const glm::mat4 proj = camera.projection(asp);
        const glm::mat4 view = camera.view();

        glm::mat4 inv_vp = glm::inverse(proj * view);
        glm::vec4 ray_clip{nx, ny, -1.f, 1.f};
        glm::vec4 ray_world = inv_vp * ray_clip;
        ray_world /= ray_world.w;

        const glm::vec3 origin = camera.eye();
        const glm::vec3 dir    = glm::normalize(glm::vec3(ray_world) - origin);

        entt::entity best   = entt::null;
        float        best_t = std::numeric_limits<float>::max();

        auto& reg  = world.reg();
        auto  view_nodes = reg.view<NodeComponent>();

        for (auto e : view_nodes) {
            const auto& n  = view_nodes.get<NodeComponent>(e);
            const float r  = sphere_radius * (n.active ? 1.f + n.value : 0.5f);
            const float t  = ray_sphere(origin, dir, n.pos, r);
            if (t > 0.f && t < best_t) {
                best_t = t;
                best   = e;
            }
        }
        return best;
    }

private:
    /// Returns ray parameter t at intersection, or -1 if miss.
    static float ray_sphere(const glm::vec3& o, const glm::vec3& d,
                             const glm::vec3& center, float radius) noexcept {
        const glm::vec3 oc = o - center;
        float b = glm::dot(oc, d);
        float c = glm::dot(oc, oc) - radius * radius;
        float disc = b * b - c;
        if (disc < 0.f) return -1.f;
        return -b - std::sqrt(disc);
    }
};

} // namespace aarf
