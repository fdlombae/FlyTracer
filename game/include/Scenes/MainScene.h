#pragma once

#include "GameScene.h"

class MainScene final : public GameScene {
public:
    explicit MainScene(const std::string& resourceDir);

    void OnInit(VulkanRenderer* renderer) override;
    void OnUpdate(float deltaSec) override;
    void OnInput(const InputState& input) override;
    void OnGui() override;
    void OnShutdown() override;

private:
    // Camera orbit
    float m_cameraYaw{0.0f};
    float m_cameraPitch{0.1f};
    float m_cameraDistance{50.0f};
    float m_mouseSensitivity{0.005f};
    float m_cameraColliderRadius{ 2.f };

    uint32_t m_characterMeshId{};
    MeshInstance* m_pCharacterMesh{};
    float m_movementSpeed{ 60.f }; // units/s

    void ProcessCameraMovement(InputState const& input);
    void ProcessMovement(float deltaSec);
    void ResolveCameraCollisions();
};
