#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// history_ribbon.hpp — Renders a 2D sparkline "ribbon" trailing behind a
//                      selected node using ImGui DrawList injected into the
//                      3D framebuffer.
//
// The ribbon is drawn in screen-space via ImGui's background draw list, but
// its anchor position is the projected 3D world position of the node.
// ─────────────────────────────────────────────────────────────────────────────
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <vector>
#include "registry.hpp"
#include "components.hpp"

namespace aarf {

class HistoryRibbon {
public:
    /// Width of the ribbon in screen pixels.
    float ribbon_width{120.f};
    /// Height of the ribbon in screen pixels.
    float ribbon_height{32.f};

    /// Project a world-space position to screen coords.
    static glm::vec2 world_to_screen(const glm::vec3& world,
                                      const glm::mat4& vp,
                                      int screen_w, int screen_h) {
        glm::vec4 clip = vp * glm::vec4(world, 1.f);
        if (std::abs(clip.w) < 1e-5f) return {-9999.f, -9999.f};
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        return {
            (ndc.x * 0.5f + 0.5f) * screen_w,
            (1.f - (ndc.y * 0.5f + 0.5f)) * screen_h
        };
    }

    /// Draw ribbon for the given entity (if it has HistoryComponent).
    void draw(entt::entity e,
              CartographerWorld& world,
              const glm::mat4& vp,
              int screen_w, int screen_h) {
        auto& reg = world.reg();
        if (!reg.valid(e)) return;
        if (!reg.all_of<NodeComponent, HistoryComponent>(e)) return;

        const auto& n  = reg.get<NodeComponent>(e);
        const auto& h  = reg.get<HistoryComponent>(e);
        if (h.values.empty()) return;

        glm::vec2 anchor = world_to_screen(n.pos, vp, screen_w, screen_h);
        // Don't draw if off-screen or behind camera
        if (anchor.x < -500.f || anchor.x > screen_w + 500.f) return;

        // Offset ribbon above the node
        float x0 = anchor.x - ribbon_width * 0.5f;
        float y0 = anchor.y - 48.f - ribbon_height;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        // Background
        dl->AddRectFilled({x0 - 2.f, y0 - 2.f},
                          {x0 + ribbon_width + 2.f, y0 + ribbon_height + 2.f},
                          IM_COL32(10, 10, 20, 180), 4.f);

        // Sparkline
        const auto& vals = h.values;
        const int   N    = (int)vals.size();
        if (N < 2) return;

        const float step = ribbon_width / (float)(N - 1);
        for (int i = 1; i < N; ++i) {
            float x1 = x0 + (i - 1) * step;
            float x2 = x0 + i * step;
            float v1 = vals[i-1], v2 = vals[i];
            float y1s = y0 + ribbon_height * (1.f - v1);
            float y2s = y0 + ribbon_height * (1.f - v2);

            // Colour by value: cool → hot
            float t    = (v1 + v2) * 0.5f;
            uint8_t r  = (uint8_t)(t * 255.f);
            uint8_t g  = (uint8_t)((1.f - std::abs(t - 0.5f) * 2.f) * 200.f);
            uint8_t b  = (uint8_t)((1.f - t) * 200.f);

            dl->AddLine({x1, y1s}, {x2, y2s},
                        IM_COL32(r, g, b, 230), 1.5f);
        }

        // Label
        dl->AddText({x0, y0 - 14.f},
                    IM_COL32(255, 220, 80, 255),
                    n.name.c_str());
    }
};

} // namespace aarf
