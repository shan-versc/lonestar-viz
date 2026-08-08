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
    explicit Octree(float world_half = 100.f) {
        root_.bounds = { glm::vec3(0.f), world_half };
    }

    void clear() {
        root_ = OctreeNode{};
        root_.bounds = { glm::vec3(0.f), 100.f };
    }

    void insert(entt::entity e, const glm::vec3& pos) {
        root_.insert(e, pos);
    }

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
    OctreeNode root_;
};

} // namespace aarf
