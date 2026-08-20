#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// octree.hpp — Spatial octree for LOD bucketing and range queries
// ─────────────────────────────────────────────────────────────────────────────
#include <array>
#include <memory>
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include <entt/entt.hpp>

namespace aarf {

struct AABB {
    glm::vec3 center{0.f};
    float     half{50.f};

    [[nodiscard]] bool contains(const glm::vec3& p) const noexcept {
        return glm::all(glm::greaterThanEqual(p, center - half))
            && glm::all(glm::lessThan(p, center + half));
    }

    [[nodiscard]] bool intersects(const AABB& o) const noexcept {
        return glm::all(glm::lessThanEqual(glm::abs(center - o.center),
                                           glm::vec3(half + o.half)));
    }
};

struct OctreeNode {
    AABB                                  bounds;
    std::vector<std::pair<entt::entity, glm::vec3>> items;
    std::array<std::unique_ptr<OctreeNode>, 8>      children{};
    bool                                  is_leaf{true};

    /// Aggregate centre of mass and total weight (for Barnes-Hut far-field).
    /// Set by recalc() / Octree::rebuild_masses() after the tree is populated.
    glm::vec3 com{0.f};
    float     mass{0.f};

    static constexpr int kMaxItems = 16;
    static constexpr int kMaxDepth = 6;

    void insert(entt::entity e, const glm::vec3& pos, int depth = 0) {
        if (!bounds.contains(pos)) return;

        if (is_leaf) {
            items.emplace_back(e, pos);
            if (static_cast<int>(items.size()) > kMaxItems && depth < kMaxDepth)
                subdivide(depth);
        } else {
            for (auto& child : children)
                if (child) child->insert(e, pos, depth + 1);
        }
    }

    void query(const AABB& range, std::vector<entt::entity>& out) const {
        if (!bounds.intersects(range)) return;
        if (is_leaf) {
            for (auto& [e, p] : items)
                if (range.contains(p)) out.push_back(e);
        } else {
            for (auto& child : children)
                if (child) child->query(range, out);
        }
    }

    /// Returns number of entities stored (leaf nodes only).
    [[nodiscard]] std::size_t count() const noexcept {
        if (is_leaf) return items.size();
        std::size_t n = 0;
        for (auto& c : children) if (c) n += c->count();
        return n;
    }

    /// Recompute the centre of mass / total weight for this node and its
    /// descendants (post-order). Leaves average their items; interior nodes
    /// weight-average their children.
    void recalc() noexcept {
        if (is_leaf) {
            com  = glm::vec3(0.f);
            mass = 0.f;
            for (const auto& [e, p] : items) { com += p; mass += 1.f; }
            if (mass > 0.f) com /= mass;
            return;
        }
        com  = glm::vec3(0.f);
        mass = 0.f;
        for (auto& c : children) {
            if (!c) continue;
            c->recalc();
            com  += c->com * c->mass;
            mass += c->mass;
        }
        if (mass > 0.f) com /= mass;
    }

private:
    void subdivide(int depth) {
        is_leaf = false;
        const float q = bounds.half * 0.5f;
        const glm::vec3 offsets[8] = {
            {-q,-q,-q},{+q,-q,-q},{-q,+q,-q},{+q,+q,-q},
            {-q,-q,+q},{+q,-q,+q},{-q,+q,+q},{+q,+q,+q}
        };
        for (int i = 0; i < 8; ++i) {
            children[i] = std::make_unique<OctreeNode>();
            children[i]->bounds = { bounds.center + offsets[i], q };
        }
        for (auto& [e, p] : items)
            for (auto& child : children)
                if (child) child->insert(e, p, depth + 1);
        items.clear();
    }
};

class Octree {
public:
    explicit Octree(float world_half = 100.f)
        : world_half_(world_half) {
        root_.bounds = { glm::vec3(0.f), world_half_ };
    }

    void clear() {
        root_      = OctreeNode{};
        root_.bounds = { glm::vec3(0.f), world_half_ };
    }

    void insert(entt::entity e, const glm::vec3& pos) {
        root_.insert(e, pos);
    }

    /// Recompute centre-of-mass / weight aggregates once the tree is populated.
    void rebuild_masses() noexcept { root_.recalc(); }

    [[nodiscard]] std::vector<entt::entity>
    query_range(const AABB& range) const {
        std::vector<entt::entity> result;
        root_.query(range, result);
        return result;
    }

    [[nodiscard]] std::vector<entt::entity>
    query_sphere(const glm::vec3& center, float radius) const {
        return query_range({ center, radius });
    }

    [[nodiscard]] const OctreeNode& root() const noexcept { return root_; }

private:
    float       world_half_{100.f};
    OctreeNode  root_;
};

} // namespace aarf
