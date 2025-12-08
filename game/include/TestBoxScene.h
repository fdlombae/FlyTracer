#pragma once

#include "GameScene.h"
#include <cstdint>
#include <numbers>

class TestBoxScene final : public GameScene {
public:
    explicit TestBoxScene(const std::string& resourceDir);

    void onInit(VulkanRenderer* renderer) override;
    void onUpdate(float deltaTime) override;
    void onInput(const InputState& input) override;
    void onGui() override;
    void onShutdown() override;

private:
    static constexpr float kPi = std::numbers::pi_v<float>;

    // Camera orbit
    float m_cameraYaw{0.0f};
    float m_cameraPitch{0.3f};
    float m_cameraDistance{50.0f};
    float m_mouseSensitivity{0.005f};

    // Sphere rotation
    uint32_t m_sphere1Id{0};
    uint32_t m_sphere2Id{0};
    float m_rotationAngle{0.0f};
    float m_rotationSpeed{0.5f};
    float m_sphere1Radius{20.0f};
    float m_sphere2Radius{25.0f};
    float m_sphere1Height{5.0f};
    float m_sphere2Height{8.0f};

    // Pheasant mesh
    uint32_t m_pheasantMeshId{0};
    float m_pheasantTime{0.0f};
    float m_pheasantSpeed{1.0f};
    float m_pheasantAmplitudeX{20.0f};
    float m_pheasantAmplitudeZ{10.0f};
    float m_pheasantHeight{0.0f};
    float m_pheasantScale{0.5f};
};
