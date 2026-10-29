#pragma once

#include "GameScene.h"
#include "OrbitCamera.h"
#include <array>
#include <optional>

// A line in Plücker coordinates: its direction v, its moment m = p x v for any point p on it, and its
// point closest to the origin d = v x m / |v|^2. As a bivector the line is
// m_x e01 + m_y e02 + m_z e03 + v_x e23 + v_y e31 + v_z e12. The tips of v, m and d can be dragged.
class PluckerScene final : public GameScene {
public:
    explicit PluckerScene(const std::string& resourceDir);

protected:
    void OnInit() override;
    void OnInput(const Input& input) override;
    void OnUpdate(float deltaTime) override;
    void OnGui() override;

private:
    enum class Handle { Direction, Moment, ClosestPoint, Count };

    static constexpr float kHandleRadius = 0.25f;
    static constexpr float kMinDirectionLength = 0.2f;

    [[nodiscard]] TriVector Direction() const;
    [[nodiscard]] TriVector Moment() const;
    [[nodiscard]] TriVector ClosestPoint() const;
    [[nodiscard]] TriVector HandlePosition(Handle handle) const;
    [[nodiscard]] std::optional<Handle> PickHandle(const BiVector& ray) const;
    void Drag(const TriVector& target);
    void Reset();

    OrbitCamera m_orbit{TriVector(0.0f, 1.0f, 0.0f), 0.6f, 0.35f, 14.0f, 5.0f, 40.0f};
    BiVector m_line{0, 0, 0, 1, 0, 0};
    std::array<uint32_t, static_cast<size_t>(Handle::Count)> m_handleSpheres{};
    std::optional<Handle> m_hovered;
    std::optional<Handle> m_dragged;
    Vector m_dragPlane{0, 0, 0, 1};      // Through the grabbed tip, perpendicular to the mouse ray
    TriVector m_dragPivot{0, 0, 0};      // The closest point when dragging the direction started
    bool m_guiVisible{true};
};
