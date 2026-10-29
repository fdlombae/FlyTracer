#pragma once

#include "GameScene.h"
#include "OrbitCamera.h"
#include <cstdint>

// Demo scene showcasing debug visualization features
class DebugDemoScene final : public GameScene {
public:
    explicit DebugDemoScene(const std::string& resourceDir);

protected:
    void OnInit() override;
    void OnUpdate(float deltaTime) override;
    void OnInput(const Input& input) override;
    void OnGui() override;

private:
    void Reset();

    OrbitCamera m_orbit{TriVector(0.0f, 3.0f, 0.0f), 0.5f, 0.4f, 25.0f, 8.0f, 50.0f};

    // Animation
    float m_time{0.0f};
    float m_rotationSpeed{1.0f};
    bool m_paused{false};
    const TriVector m_sphereStartPosition{0.0f, 3.0f, 0.0f, 1.0f};

    uint32_t m_sphereId{0};

    // Debug visualization toggles
    bool m_showAxes{true};
    bool m_showGrid{true};
    bool m_showLines{true};
};
