#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// imgui_panels.hpp — Extended Dear ImGui panels for the Cartographer:
//
//  • NodeInspector  — selected node detail + temporal history plot
//  • StatsPanel     — live FPS, entity counts, queue depth histogram
//  • PhysicsPanel   — tweak PhysicsConfig sliders at runtime
//  • HistoryRibbon  — 2D sparkline ribbon of the last 128 values per node
// ─────────────────────────────────────────────────────────────────────────────
#include <imgui.h>
#include <string>
#include <vector>
#include "registry.hpp"
#include "components.hpp"
#include "physics.hpp"
#include <entt/entt.hpp>

namespace aarf {

// ── NodeInspector ─────────────────────────────────────────────────────────────

class NodeInspector {
public:
    entt::entity selected{entt::null};

    void draw(CartographerWorld& world) {
        if (!world.reg().valid(selected)) {
            selected = entt::null;
            return;
        }
        auto& n = world.reg().get<NodeComponent>(selected);

        ImGui::SetNextWindowPos({10, 380}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({300, 260}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.82f);
        ImGui::Begin("Node Inspector", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        ImGui::TextColored({1.f, 0.85f, 0.2f, 1.f}, "%s", n.name.c_str());
        ImGui::Separator();
        ImGui::Text("Value     : %.4f", n.value);
        ImGui::Text("Prev      : %.4f", n.prev_value);
        ImGui::Text("Delta     : %+.4f", n.value - n.prev_value);
        ImGui::Text("Pos       : (%.1f, %.1f, %.1f)", n.pos.x, n.pos.y, n.pos.z);
        ImGui::Text("Speed     : %.3f", glm::length(n.vel));
        ImGui::Text("Active    : %s", n.active ? "YES" : "no");

        // History sparkline
        if (world.reg().all_of<HistoryComponent>(selected)) {
            auto& h = world.reg().get<HistoryComponent>(selected);
            if (!h.values.empty()) {
                std::vector<float> vals(h.values.begin(), h.values.end());
                ImGui::Separator();
                ImGui::Text("History (%zu samples)", vals.size());
                ImGui::PlotLines("##hist", vals.data(), (int)vals.size(),
                                 0, nullptr, 0.f, 1.f, {280, 60});
            }
        }
        ImGui::End();
    }
};

// ── StatsPanel ────────────────────────────────────────────────────────────────

class StatsPanel {
public:
    static constexpr int kFpsHistory = 128;
    float fps_buf[kFpsHistory]{};
    int   fps_idx{0};

    void push_fps(float fps) {
        fps_buf[fps_idx % kFpsHistory] = fps;
        ++fps_idx;
    }

    void draw(CartographerWorld& world,
              float fps,
              bool  physics_paused,
              std::size_t queue_size) {
        ImGui::SetNextWindowPos({10, 10}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({310, 220}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.78f);
        ImGui::Begin("Cartographer Stats", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse);

        push_fps(fps);
        ImGui::Text("FPS"); ImGui::SameLine();
        ImGui::PlotLines("##fps", fps_buf, kFpsHistory,
                         fps_idx % kFpsHistory,
                         nullptr, 0.f, 120.f, {200, 30});

        ImGui::Text("Nodes      : %zu  (active: %zu)",
                    world.node_count(), world.get_active_nodes().size());
        ImGui::Text("Edges      : %zu", world.edge_count());
        ImGui::Text("Queue depth: %zu", queue_size);
        ImGui::Separator();

        ImGui::TextColored(
            physics_paused
                ? ImVec4(1.f, 0.3f, 0.3f, 1.f)
                : ImVec4(0.3f, 1.f, 0.3f, 1.f),
            physics_paused ? "⏸ Physics PAUSED [SPACE]" : "▶ Physics running");

        ImGui::Separator();
        ImGui::TextDisabled("LMB drag: orbit  |  MMB: pan");
        ImGui::TextDisabled("Scroll: zoom  |  R: reset  |  ESC: quit");
        ImGui::End();
    }
};

// ── PhysicsPanel ─────────────────────────────────────────────────────────────

class PhysicsPanel {
public:
    void draw(PhysicsConfig& cfg) {
        ImGui::SetNextWindowPos({10, 240}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({310, 135}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.78f);
        ImGui::Begin("Physics Tuner", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        ImGui::SliderFloat("Repulsion k",  &cfg.k_repulse,  0.f,  200.f);
        ImGui::SliderFloat("Spring k",     &cfg.k_spring,   0.f,    1.f);
        ImGui::SliderFloat("Rest length",  &cfg.rest_len,   1.f,   30.f);
        ImGui::SliderFloat("Damping",      &cfg.damping,    0.5f,   1.f);
        ImGui::SliderFloat("Impulse k",    &cfg.impulse_k,  0.f,   20.f);
        ImGui::End();
    }
};

} // namespace aarf
