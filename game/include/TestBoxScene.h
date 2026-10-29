#pragma once

#include "GameScene.h"
#include "OrbitCamera.h"
#include <cstdint>
#include <numbers>

class TestBoxScene final : public GameScene {
public:
    explicit TestBoxScene(const std::string& resourceDir);

protected:
    void OnInit() override;
    void OnUpdate(float deltaTime) override;
    void OnInput(const Input& input) override;
    void OnGui() override;

private:
    static constexpr float kPi = std::numbers::pi_v<float>;

    OrbitCamera m_orbit{TriVector(0.0f, 15.0f, 0.0f)};

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
    float m_pheasantSway{5.0f};
    float m_pheasantHeight{0.0f};
    float m_pheasantScale{0.5f};

    // Lights spawned at runtime
    uint32_t m_spawnedLightCount{0};

    void SpawnLight();
    void DrawLightsGui();
    void DrawInstancesGui();
};
