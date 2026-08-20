// ─────────────────────────────────────────────────────────────────────────────
// renderer.cpp — OpenGL 4.1 instanced billboard renderer
// ─────────────────────────────────────────────────────────────────────────────
#include "renderer.hpp"
#include "registry.hpp"
#include "components.hpp"
#include "camera.hpp"
#include "imgui_panels.hpp"
#include "history_ribbon.hpp"
#include "physics.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include <stdexcept>

// ── Embedded GLSL ─────────────────────────────────────────────────────────────

static const char* kNodeVert = R"glsl(
#version 410 core

// Quad vertices (2 triangles, -0.5..+0.5)
layout(location = 0) in vec2 a_quad;

// Per-instance attributes
layout(location = 1) in vec3  i_world_pos;
layout(location = 2) in float i_size;
layout(location = 3) in vec3  i_color;
layout(location = 4) in float i_value;

uniform mat4 u_view;
uniform mat4 u_proj;

out vec2  v_uv;
out vec3  v_color;
out float v_value;
out float v_glow;

void main() {
    // Extract camera right/up from view matrix rows (billboard facing)
    vec3 cam_right = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
    vec3 cam_up    = vec3(u_view[0][1], u_view[1][1], u_view[2][1]);

    vec3 world = i_world_pos
               + cam_right * a_quad.x * i_size
               + cam_up    * a_quad.y * i_size;

    gl_Position = u_proj * u_view * vec4(world, 1.0);
    v_uv        = a_quad + 0.5;
    v_color     = i_color;
    v_value     = i_value;
    v_glow      = i_value;
}
)glsl";

static const char* kNodeFrag = R"glsl(
#version 410 core

in vec2  v_uv;
in vec3  v_color;
in float v_value;
in float v_glow;

out vec4 frag_color;

void main() {
    // SDF soft circle
    float dist = length(v_uv - 0.5) * 2.0;
    float alpha = 1.0 - smoothstep(0.7, 1.0, dist);

    // Glow ring
    float ring  = smoothstep(0.55, 0.65, dist) * (1.0 - smoothstep(0.65, 0.85, dist));
    float glow  = ring * v_glow * 2.5;

    vec3 col = v_color + glow * vec3(1.0, 0.8, 0.3);
    frag_color = vec4(col, alpha * max(0.2, v_glow + 0.3));
}
)glsl";

static const char* kEdgeVert = R"glsl(
#version 410 core
layout(location = 0) in vec3 a_pos;
uniform mat4 u_mvp;
void main() { gl_Position = u_mvp * vec4(a_pos, 1.0); }
)glsl";

static const char* kEdgeFrag = R"glsl(
#version 410 core
out vec4 frag_color;
void main() { frag_color = vec4(0.4, 0.6, 0.9, 0.25); }
)glsl";

// ── Renderer implementation ───────────────────────────────────────────────────

namespace aarf {

bool Renderer::init(GLFWwindow* window) {
    // Billboard quad: 6 vertices (2 tris), positions in [-0.5, +0.5]
    const float quad[] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f,  0.5f,
    };

    glGenVertexArrays(1, &billboard_vao_);
    glBindVertexArray(billboard_vao_);

    glGenBuffers(1, &billboard_vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, billboard_vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), nullptr);

    // Instance VBO (streamed per frame)
    glGenBuffers(1, &instance_vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 max_instances_ * sizeof(BillboardInstance),
                 nullptr, GL_DYNAMIC_DRAW);

    // Attrib layout for BillboardInstance
    const GLsizei stride = sizeof(BillboardInstance);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          (void*)offsetof(BillboardInstance, world_pos));
    glVertexAttribDivisor(1, 1);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride,
                          (void*)offsetof(BillboardInstance, size));
    glVertexAttribDivisor(2, 1);

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride,
                          (void*)offsetof(BillboardInstance, color));
    glVertexAttribDivisor(3, 1);

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride,
                          (void*)offsetof(BillboardInstance, value));
    glVertexAttribDivisor(4, 1);

    glBindVertexArray(0);

    // Edge VAO
    glGenVertexArrays(1, &edge_vao_);
    glBindVertexArray(edge_vao_);
    glGenBuffers(1, &edge_vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, edge_vbo_);
    glBufferData(GL_ARRAY_BUFFER, 8192 * 2 * sizeof(glm::vec3),
                 nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glBindVertexArray(0);

    if (!compile_shaders()) return false;

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    // GL state
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glPointSize(3.f);
    glLineWidth(1.f);

    return true;
}

void Renderer::render(CartographerWorld& world,
                      const ArcballCamera& camera,
                      int width, int height,
                      float fps, bool physics_paused,
                      std::size_t queue_size,
                      PhysicsConfig* phys_cfg) {
    glViewport(0, 0, width, height);
    glClearColor(0.04f, 0.04f, 0.08f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const float aspect = (height > 0) ? float(width) / float(height) : 1.f;
    const glm::mat4 view = camera.view();
    const glm::mat4 proj = camera.projection(aspect);
    const glm::mat4 vp   = proj * view;

    // ── Draw edges ────────────────────────────────────────────────────────────
    draw_edges(world, vp);

    // ── Build instance list ───────────────────────────────────────────────────
    std::vector<BillboardInstance> instances;
    instances.reserve(world.node_count());

    auto& reg  = world.reg();
    auto  view_nodes = reg.view<NodeComponent>();

    for (auto e : view_nodes) {
        const auto& n = view_nodes.get<NodeComponent>(e);
        BillboardInstance bi;
        bi.world_pos = n.pos;
        bi.value     = n.value;
        bi.color     = value_to_heatmap(n.value);
        bi.size      = n.active ? (1.0f + n.value * 1.5f) : 0.4f;
        if (reg.all_of<SelectedTag>(e)) {
            bi.color = glm::vec3(1.f, 1.f, 0.f);
            bi.size *= 1.8f;
        }
        instances.push_back(bi);
    }

    // Clamp to buffer capacity
    if (instances.size() > max_instances_)
        instances.resize(max_instances_);

    // Upload and draw billboards
    upload_instances(instances);

    glUseProgram(node_program_);
    glUniformMatrix4fv(glGetUniformLocation(node_program_, "u_view"),
                       1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(node_program_, "u_proj"),
                       1, GL_FALSE, glm::value_ptr(proj));

    glBindVertexArray(billboard_vao_);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)instances.size());
    glBindVertexArray(0);

    // ── ImGui ─────────────────────────────────────────────────────────────────
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    draw_imgui_overlay(world, camera, width, height, fps, physics_paused, queue_size, phys_cfg);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Renderer::upload_instances(const std::vector<BillboardInstance>& data) {
    glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    (GLsizeiptr)(data.size() * sizeof(BillboardInstance)),
                    data.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Renderer::draw_edges(CartographerWorld& world, const glm::mat4& vp) {
    auto& reg  = world.reg();
    auto  ev   = reg.view<EdgeComponent>();
    auto  nv   = reg.view<NodeComponent>();

    std::vector<glm::vec3> lines;
    lines.reserve(world.edge_count() * 2);

    for (auto e : ev) {
        const auto& ed = ev.get<EdgeComponent>(e);
        if (!reg.valid(ed.src) || !reg.valid(ed.dst)) continue;
        if (!nv.contains(ed.src) || !nv.contains(ed.dst)) continue;
        lines.push_back(nv.get<NodeComponent>(ed.src).pos);
        lines.push_back(nv.get<NodeComponent>(ed.dst).pos);
    }
    if (lines.empty()) return;

    const std::size_t max_lines = 8192;
    if (lines.size() > max_lines * 2) lines.resize(max_lines * 2);

    glBindBuffer(GL_ARRAY_BUFFER, edge_vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    (GLsizeiptr)(lines.size() * sizeof(glm::vec3)),
                    lines.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glUseProgram(edge_program_);
    glUniformMatrix4fv(glGetUniformLocation(edge_program_, "u_mvp"),
                       1, GL_FALSE, glm::value_ptr(vp));
    glBindVertexArray(edge_vao_);
    glDrawArrays(GL_LINES, 0, (GLsizei)lines.size());
    glBindVertexArray(0);
}

void Renderer::draw_imgui_overlay(CartographerWorld& world,
                                   const ArcballCamera& camera,
                                   int width, int height,
                                   float fps,
                                   bool physics_paused,
                                   std::size_t queue_size,
                                   PhysicsConfig* phys_cfg) {
    // Static panel instances — survive across frames
    static StatsPanel    stats;
    static NodeInspector inspector;
    static PhysicsPanel  phys_panel;
    static HistoryRibbon ribbon;

    // Stats overlay (always shown)
    stats.draw(world, fps, physics_paused, queue_size);

    // Physics tuner
    if (phys_cfg)
        phys_panel.draw(*phys_cfg);

    // Selected-node inspector + ribbon
    if (selected_entity != 0xFFFFFFFFu) {
        entt::entity sel = static_cast<entt::entity>(selected_entity);
        inspector.selected = sel;
        inspector.draw(world);

        const glm::mat4 vp = camera.projection((float)width / (float)height)
                           * camera.view();
        ribbon.draw(sel, world, vp, width, height);
    }
}

void Renderer::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteBuffers(1, &billboard_vbo_);
    glDeleteBuffers(1, &instance_vbo_);
    glDeleteVertexArrays(1, &billboard_vao_);
    glDeleteBuffers(1, &edge_vbo_);
    glDeleteVertexArrays(1, &edge_vao_);
    glDeleteProgram(node_program_);
    glDeleteProgram(edge_program_);
}

bool Renderer::compile_shaders() {
    uint32_t nv = compile_shader(kNodeVert, GL_VERTEX_SHADER);
    uint32_t nf = compile_shader(kNodeFrag, GL_FRAGMENT_SHADER);
    uint32_t ev = compile_shader(kEdgeVert, GL_VERTEX_SHADER);
    uint32_t ef = compile_shader(kEdgeFrag, GL_FRAGMENT_SHADER);
    if (!nv || !nf || !ev || !ef) return false;

    node_program_ = link_program(nv, nf);
    edge_program_ = link_program(ev, ef);
    glDeleteShader(nv); glDeleteShader(nf);
    glDeleteShader(ev); glDeleteShader(ef);
    return node_program_ && edge_program_;
}

uint32_t Renderer::compile_shader(const char* src, uint32_t type) {
    uint32_t s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, nullptr, log);
        std::fprintf(stderr, "[renderer] shader compile error:\n%s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

uint32_t Renderer::link_program(uint32_t vert, uint32_t frag) {
    uint32_t p = glCreateProgram();
    glAttachShader(p, vert);
    glAttachShader(p, frag);
    glLinkProgram(p);
    int ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(p, 512, nullptr, log);
        std::fprintf(stderr, "[renderer] link error:\n%s\n", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

glm::vec3 Renderer::value_to_heatmap(float t) noexcept {
    t = glm::clamp(t, 0.f, 1.f);
    // Cool (blue) → warm (orange) → hot (white)
    if (t < 0.33f) {
        float s = t / 0.33f;
        return glm::mix(glm::vec3(0.05f, 0.10f, 0.60f),
                        glm::vec3(0.10f, 0.70f, 0.30f), s);
    } else if (t < 0.66f) {
        float s = (t - 0.33f) / 0.33f;
        return glm::mix(glm::vec3(0.10f, 0.70f, 0.30f),
                        glm::vec3(1.00f, 0.50f, 0.05f), s);
    } else {
        float s = (t - 0.66f) / 0.34f;
        return glm::mix(glm::vec3(1.00f, 0.50f, 0.05f),
                        glm::vec3(1.00f, 1.00f, 1.00f), s);
    }
}

} // namespace aarf
