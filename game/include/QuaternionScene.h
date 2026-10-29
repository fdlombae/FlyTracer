#pragma once

#include "GameScene.h"
#include "OrbitCamera.h"
#include <numbers>

class QuaternionScene final : public GameScene {
public:
    explicit QuaternionScene(const std::string& resourceDir);

protected:
    void OnInit() override;
    void OnUpdate(float deltaTime) override;
    void OnInput(const Input& input) override;
    void OnGui() override;

private:
    static constexpr float kPi = std::numbers::pi_v<float>;
    static constexpr float kYawSpeed = 1.5f;
    static constexpr float kPitchSpeed = 1.0f;
    static constexpr float kPosSpeed = 8.0f;
    static constexpr float kAngleSpeed = 1.5f;

    bool m_guiVisible{true};
    OrbitCamera m_orbit{TriVector(0.0f, 5.0f, 0.0f)};

    // Pheasant
    uint32_t m_pheasantMeshId{0};
    uint32_t m_pheasantInstanceId{0};
    float m_pheasantScale{0.5f};

    // Rotation axis: direction from yaw/pitch, through a point
    float m_lineYaw{-30.0f * DEG_TO_RAD};  // Right-handed around the y-axis, like OrbitCamera
    float m_linePitch{20.0f * DEG_TO_RAD};
    float m_linePosX{0.0f};
    float m_linePosY{0.0f};
    float m_linePosZ{0.0f};

    // Rotation angle in radians
    float m_rotationAngle{60.0f * DEG_TO_RAD};

    void ResetAll();
    [[nodiscard]] TriVector LinePoint() const;
    [[nodiscard]] TriVector LineDirection() const;
    [[nodiscard]] BiVector BuildLine() const;
};
