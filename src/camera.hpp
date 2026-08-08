#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// camera.hpp — Arcball trackball camera with causality-filter focal point
// ─────────────────────────────────────────────────────────────────────────────
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace aarf {

class ArcballCamera {
public:
    ArcballCamera() = default;

    explicit ArcballCamera(glm::vec3 eye, glm::vec3 target = glm::vec3(0.f),
                           float fov_deg = 45.f)
        : eye_(eye), target_(target), fov_(fov_deg) {}

    // ── View / Projection ────────────────────────────────────────────────────

    [[nodiscard]] glm::mat4 view() const noexcept {
        return glm::lookAt(eye_, target_, up_);
    }

    [[nodiscard]] glm::mat4 projection(float aspect) const noexcept {
        return glm::perspective(glm::radians(fov_), aspect, near_, far_);
    }

    [[nodiscard]] glm::vec3 eye()    const noexcept { return eye_; }
    [[nodiscard]] glm::vec3 target() const noexcept { return target_; }
    [[nodiscard]] float     fov()    const noexcept { return fov_; }

    // ── Input handlers ───────────────────────────────────────────────────────

    /// Arcball orbit: dx/dy in pixels, sensitivity in radians/pixel.
    void on_mouse_drag(float dx, float dy, float sensitivity = 0.005f) noexcept {
        // Build rotation quaternion from screen-space deltas
        const glm::vec3 right = glm::normalize(glm::cross(eye_ - target_, up_));
        const glm::quat q_y   = glm::angleAxis(-dx * sensitivity,
                                               glm::vec3(0.f, 1.f, 0.f));
        const glm::quat q_x   = glm::angleAxis(-dy * sensitivity, right);
        const glm::quat q     = glm::normalize(q_y * q_x);

        // Rotate eye position around target
        const glm::vec3 arm   = eye_ - target_;
        eye_ = target_ + glm::rotate(q, arm);
        up_  = glm::rotate(q, up_);
    }

    /// Pan: shift both eye and target in the camera's XY plane.
    void on_pan(float dx, float dy, float sensitivity = 0.05f) noexcept {
        const float      dist  = glm::length(eye_ - target_);
        const glm::vec3  right = glm::normalize(glm::cross(eye_ - target_, up_));
        const glm::vec3  up_n  = glm::normalize(up_);
        const glm::vec3  shift = (-right * dx + up_n * dy)
                                  * sensitivity * dist * 0.01f;
        eye_    += shift;
        target_ += shift;
    }

    /// Dolly zoom (scroll wheel).
    void on_scroll(float delta, float sensitivity = 1.2f) noexcept {
        const glm::vec3 arm = eye_ - target_;
        const float     len = glm::length(arm);
        float           new_len = (delta > 0.f)
                                    ? len / sensitivity
                                    : len * sensitivity;
        new_len = glm::clamp(new_len, 1.f, 500.f);
        eye_ = target_ + glm::normalize(arm) * new_len;
    }

    /// Set focus (causality filter): re-center target without moving eye.
    void set_target(const glm::vec3& t) noexcept {
        const float dist = glm::length(eye_ - target_);
        target_ = t;
        eye_    = t + glm::normalize(eye_ - target_) * dist;
    }

    void reset() noexcept {
        eye_    = glm::vec3(0.f, 0.f, 120.f);
        target_ = glm::vec3(0.f);
        up_     = glm::vec3(0.f, 1.f, 0.f);
        fov_    = 45.f;
    }

private:
    glm::vec3 eye_   {0.f, 0.f, 120.f};
    glm::vec3 target_{0.f};
    glm::vec3 up_    {0.f, 1.f,   0.f};
    float     fov_   {45.f};
    float     near_  {0.1f};
    float     far_   {1000.f};
};

} // namespace aarf
