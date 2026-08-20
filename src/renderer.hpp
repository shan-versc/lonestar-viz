#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// renderer.hpp — OpenGL 4.1 instanced billboard + edge line renderer + ImGui
// ─────────────────────────────────────────────────────────────────────────────
#include <cstdint>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <entt/entt.hpp>

struct GLFWwindow;

namespace aarf {

class CartographerWorld;
class ArcballCamera;
struct PhysicsConfig;

/// Per-instance GPU data (packed for std140/instanced arrays).
struct BillboardInstance {
    glm::vec3 world_pos;
    float     size;
    glm::vec3 color;
    float     value;   ///< [0,1] — also drives glow in fragment shader
};

class Renderer {
public:
    Renderer()  = default;
    ~Renderer() = default;

    /// Sentinel for "no entity selected" (sentinel, not a real entity id).
    static constexpr uint32_t kNoSelection = 0xFFFFFFFFu;

    bool init(GLFWwindow* window);
    void render(CartographerWorld& world,
                const ArcballCamera& camera,
                int width, int height,
                float fps, bool physics_paused,
                std::size_t queue_size,
                PhysicsConfig* phys_cfg = nullptr,
                const std::vector<entt::entity>* visible_nodes = nullptr);
    void shutdown();

    /// Expose selected entity index (-1 = none) set by raycasting in main.cpp
    int  selected_node_idx{-1};
    /// Entity selected for inspector + ribbon (kNoSelection = none).
    uint32_t selected_entity{kNoSelection};

private:
    // ── OpenGL handles ────────────────────────────────────────────────────────
    uint32_t billboard_vao_{0};
    uint32_t billboard_vbo_{0};       ///< quad vertices
    uint32_t instance_vbo_{0};        ///< per-instance data (streamed each frame)
    uint32_t edge_vao_{0};
    uint32_t edge_vbo_{0};

    uint32_t node_program_{0};
    uint32_t edge_program_{0};

    // ── Internal helpers ─────────────────────────────────────────────────────
    bool compile_shaders();
    uint32_t compile_shader(const char* src, uint32_t type);
    uint32_t link_program(uint32_t vert, uint32_t frag);

    void upload_instances(const std::vector<BillboardInstance>& data);
    void draw_edges(CartographerWorld& world,
                    const glm::mat4& vp);
    void draw_imgui_overlay(CartographerWorld& world,
                            const ArcballCamera& camera,
                            int width, int height,
                            float fps,
                            bool physics_paused,
                            std::size_t queue_size,
                            PhysicsConfig* phys_cfg);

    static glm::vec3 value_to_heatmap(float t) noexcept;

    std::size_t max_instances_{4096};
};

} // namespace aarf
