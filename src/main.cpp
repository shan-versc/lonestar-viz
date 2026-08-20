// ─────────────────────────────────────────────────────────────────────────────
// main.cpp — 3D Causal Cartographer entry point
//
// Thread model:
//   Main thread:  GLFW + OpenGL + ImGui + Physics step + ECS reads
//   Ingest thread: synthetic data producer → SpscQueue
// ─────────────────────────────────────────────────────────────────────────────
#include <glad/gl.h>        // must come before GLFW
#include <GLFW/glfw3.h>

#include "registry.hpp"
#include "physics.hpp"
#include "octree_physics.hpp"
#include "renderer.hpp"
#include "ingest.hpp"
#include "camera.hpp"
#include "raycaster.hpp"
#include "components.hpp"

#include <imgui.h>

#include <glm/glm.hpp>

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <random>
#include <string>
#include <chrono>

// ─────────────────────────────────────────────────────────────────────────────
// Globals shared with GLFW callbacks (minimal, callback-only)
// ─────────────────────────────────────────────────────────────────────────────
namespace {

aarf::ArcballCamera g_camera{glm::vec3(0.f, 0.f, 140.f)};

struct MouseState {
    double last_x{0}, last_y{0};
    bool   lmb{false}, mmb{false};
} g_mouse;

bool  g_physics_paused{false};
bool  g_should_reset{false};
int   g_width{1280}, g_height{720};
bool  g_click_pending{false};
double g_click_x{0}, g_click_y{0};

void glfw_key_callback(GLFWwindow* w, int key, int /*sc*/, int action, int /*mod*/) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE)  glfwSetWindowShouldClose(w, GLFW_TRUE);
    if (key == GLFW_KEY_R)       g_should_reset = true;
    if (key == GLFW_KEY_SPACE)   g_physics_paused = !g_physics_paused;
}

void glfw_mouse_button_callback(GLFWwindow* /*w*/, int btn, int action, int /*mod*/) {
    if (btn == GLFW_MOUSE_BUTTON_LEFT) {
        g_mouse.lmb = (action == GLFW_PRESS);
        if (action == GLFW_PRESS) {
            g_click_pending = true;
            g_click_x = g_mouse.last_x;
            g_click_y = g_mouse.last_y;
        }
    }
    if (btn == GLFW_MOUSE_BUTTON_MIDDLE)
        g_mouse.mmb = (action == GLFW_PRESS);
}

void glfw_cursor_callback(GLFWwindow* /*w*/, double xpos, double ypos) {
    const float dx = (float)(xpos - g_mouse.last_x);
    const float dy = (float)(ypos - g_mouse.last_y);
    g_mouse.last_x = xpos;
    g_mouse.last_y = ypos;
    if (g_mouse.lmb) g_camera.on_mouse_drag(dx, dy);
    if (g_mouse.mmb) g_camera.on_pan(dx, dy);
}

void glfw_scroll_callback(GLFWwindow* /*w*/, double /*xo*/, double yo) {
    g_camera.on_scroll((float)yo);
}

void glfw_resize_callback(GLFWwindow* /*w*/, int w, int h) {
    g_width = w; g_height = h;
}

void glfw_error_callback(int err, const char* desc) {
    std::fprintf(stderr, "[GLFW %d] %s\n", err, desc);
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
int main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) { std::fputs("GLFW init failed\n", stderr); return 1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(g_width, g_height,
                                          "Lonestar 3D Causal Cartographer",
                                          nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // vsync

    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        std::fputs("GLAD init failed\n", stderr);
        glfwTerminate(); return 1;
    }
    std::printf("[GL] %s | GLSL %s\n",
                glGetString(GL_RENDERER),
                glGetString(GL_SHADING_LANGUAGE_VERSION));

    glfwSetKeyCallback(window,         glfw_key_callback);
    glfwSetMouseButtonCallback(window, glfw_mouse_button_callback);
    glfwSetCursorPosCallback(window,   glfw_cursor_callback);
    glfwSetScrollCallback(window,      glfw_scroll_callback);
    glfwSetFramebufferSizeCallback(window, glfw_resize_callback);

    // ── Build world ───────────────────────────────────────────────────────────
    constexpr int   kNodes = 200;
    constexpr int   kEdges = 320;
    constexpr float kSpread = 60.f;

    aarf::CartographerWorld world;
    aarf::SpscQueue<4096>   queue;
    aarf::OctreePhysicsSystem physics;
    aarf::Renderer          renderer;
    aarf::IngestSystem      ingest;
    aarf::Raycaster         raycaster;

    std::mt19937                          rng{42};
    std::uniform_real_distribution<float> pos_dist(-kSpread, kSpread);
    std::uniform_real_distribution<float> val_dist(0.f, 1.f);
    std::uniform_int_distribution<int>    node_pick(0, kNodes - 1);

    std::vector<entt::entity> node_entities;
    node_entities.reserve(kNodes);
    for (int i = 0; i < kNodes; ++i) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "reg_%03d", i);
        auto e = world.create_node(
            buf,
            {pos_dist(rng), pos_dist(rng), pos_dist(rng)},
            val_dist(rng));
        node_entities.push_back(e);
    }

    for (int i = 0; i < kEdges; ++i) {
        int a = node_pick(rng), b = node_pick(rng);
        if (a != b)
            world.create_edge(node_entities[a], node_entities[b],
                              0.5f + val_dist(rng) * 0.5f);
    }

    // Init subsystems
    if (!renderer.init(window)) {
        std::fputs("Renderer init failed\n", stderr);
        return 1;
    }
    ingest.init(world, &queue);

    // ── Main loop ─────────────────────────────────────────────────────────────
    using clock = std::chrono::steady_clock;
    auto  prev  = clock::now();
    float fps   = 60.f;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        auto  now = clock::now();
        float dt  = std::chrono::duration<float>(now - prev).count();
        dt        = std::min(dt, 0.05f);  // cap at 50ms
        prev      = now;
        fps       = fps * 0.95f + (1.f / (dt + 1e-9f)) * 0.05f;  // EMA

        // Reset request
        if (g_should_reset) {
            g_should_reset = false;
            physics.reset_velocities(world);
            g_camera.reset();
        }

        // Drain ingest queue
        ingest.drain(world);

        // Physics — OctreePhysicsSystem rebuilds and owns its own spatial index.
        if (!g_physics_paused)
            physics.step(world, dt);

        // Raycast on click (only if ImGui didn't consume it)
        if (g_click_pending && !ImGui::GetIO().WantCaptureMouse) {
            g_click_pending = false;
            auto& reg = world.reg();

            entt::entity hit = raycaster.pick(
                (float)g_click_x, (float)g_click_y,
                g_width, g_height, g_camera, world);

            // Clear the previous selection first; clicking empty space
            // deselects, clicking a node re-selects it.
            auto prev = reg.view<aarf::SelectedTag>();
            for (auto e : prev) reg.remove<aarf::SelectedTag>(e);
            renderer.selected_entity = 0xFFFFFFFFu;

            if (hit != entt::null) {
                reg.emplace_or_replace<aarf::SelectedTag>(hit);
                renderer.selected_entity =
                    static_cast<uint32_t>(entt::to_integral(hit));
            }
        } else if (g_click_pending) {
            g_click_pending = false;
        }

        // Render
        glfwGetFramebufferSize(window, &g_width, &g_height);
        renderer.render(world, g_camera, g_width, g_height,
                        fps, g_physics_paused, ingest.queue_approx_size(),
                        &physics.config());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ingest.shutdown();
    renderer.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
