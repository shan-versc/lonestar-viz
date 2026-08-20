// ─────────────────────────────────────────────────────────────────────────────
// test_core.cpp — headless unit tests for the octree, Barnes-Hut far field and
// octree-accelerated physics. Builds against GLM + EnTT only (no GL / window).
// ─────────────────────────────────────────────────────────────────────────────
#include "octree.hpp"
#include "octree_physics.hpp"
#include "registry.hpp"
#include "components.hpp"

#include <glm/glm.hpp>
#include <entt/entt.hpp>

#include <cstdio>
#include <cmath>

namespace {

int g_fail = 0;
int g_pass = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (cond) { ++g_pass; }                                              \
        else { ++g_fail; std::fprintf(stderr, "FAIL %s:%d: %s\n",            \
                                      __FILE__, __LINE__, #cond); }          \
    } while (0)

#define CHECK_NEAR(a, b, eps) CHECK(std::fabs((a) - (b)) <= (eps))

constexpr entt::entity E(entt::id_type id) { return entt::entity{id}; }

// ── octree: insert / query ───────────────────────────────────────────────────
void test_insert_and_query() {
    aarf::Octree tree(80.f);
    tree.insert(E(1), glm::vec3(0.f, 0.f, 0.f));
    tree.insert(E(2), glm::vec3(10.f, 0.f, 0.f));
    tree.insert(E(3), glm::vec3(40.f, 0.f, 0.f));

    auto near = tree.query_sphere(glm::vec3(0.f), 15.f);
    CHECK(near.size() == 2);   // e1, e2
    auto far = tree.query_sphere(glm::vec3(0.f), 5.f);
    CHECK(far.size() == 1);    // only e1 (e2 is 10 away)

    auto whole = tree.query_sphere(glm::vec3(0.f), 200.f);
    CHECK(whole.size() == 3);
    CHECK(tree.root().count() == 3);
}

// ── octree: clear() must preserve the configured world_half ───────────────────
void test_clear_preserves_world_half() {
    aarf::Octree tree(120.f);
    tree.clear();   // must reset to 120, not the old hard-coded 100
    tree.insert(E(1), glm::vec3(119.f, 0.f, 0.f));
    auto hit = tree.query_sphere(glm::vec3(119.f, 0.f, 0.f), 1.f);
    CHECK(!hit.empty());   // would be missed if clear dropped the extents to 100
}

// ── octree: centre-of-mass / weight aggregation (feeds Barnes-Hut) ───────────
void test_masses() {
    aarf::Octree tree(120.f);
    tree.insert(E(1), glm::vec3(0.f, 0.f, 0.f));
    tree.insert(E(2), glm::vec3(10.f, 0.f, 0.f));
    tree.insert(E(3), glm::vec3(20.f, 0.f, 0.f));
    tree.rebuild_masses();

    CHECK_NEAR(tree.root().mass, 3.f, 1e-4f);
    CHECK_NEAR(tree.root().com.x, 10.f, 1e-3f);
    CHECK_NEAR(tree.root().com.y, 0.f, 1e-3f);
    CHECK_NEAR(tree.root().com.z, 0.f, 1e-3f);
}

// ── physics: repulsion pushes a node away from a distant cluster ─────────────
// Two nodes more than cull_radius apart → only the Barnes-Hut far field acts,
// so node A must gain velocity pointing away from node B.
void test_bh_repulsion_direction() {
    aarf::CartographerWorld world;
    auto a = world.create_node("a", glm::vec3(0.f), 0.5f);
    auto b = world.create_node("b", glm::vec3(80.f, 0.f, 0.f), 0.5f);

    aarf::OctreePhysicsSystem phys;
    phys.step(world, 0.016f);

    const auto& na = world.reg().get<aarf::NodeComponent>(a);
    const auto& nb = world.reg().get<aarf::NodeComponent>(b);
    CHECK(na.vel.x > 0.f);   // a pushed away from b (+x)
    CHECK(nb.vel.x < 0.f);   // b pushed away from a (-x)
}

// ── physics: everything stays finite and bounded after many steps ────────────
void test_long_step_stability() {
    aarf::CartographerWorld world;
    for (int i = 0; i < 8; ++i) {
        char name[16];
        std::snprintf(name, sizeof(name), "n%d", i);
        world.create_node(name,
                          glm::vec3(20.f * (i % 4), 0.f, 20.f * (i / 4)), 0.5f);
    }

    aarf::OctreePhysicsSystem phys;
    for (int s = 0; s < 50; ++s)
        phys.step(world, 0.016f);

    auto view = world.reg().view<aarf::NodeComponent>();
    bool finite = true;
    for (auto e : view) {
        const auto& n = view.get<aarf::NodeComponent>(e);
        if (!std::isfinite(n.pos.x) || !std::isfinite(n.pos.y) ||
            !std::isfinite(n.pos.z)) finite = false;
    }
    CHECK(finite);
}

} // namespace

int main() {
    test_insert_and_query();
    test_clear_preserves_world_half();
    test_masses();
    test_bh_repulsion_direction();
    test_long_step_stability();

    std::printf("aarf_core: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}