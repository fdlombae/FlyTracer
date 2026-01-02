#pragma once

#include "GameScene.h"

class MainScene final : public GameScene {
public:
    explicit MainScene(const std::string& resourceDir);

    void OnInit(VulkanRenderer* renderer) override;
    void OnUpdate(float deltaSec) override;
    void OnInput(const InputState& input) override;

private:
    // Camera orbit
    float m_cameraYaw{-0.75f};
    float m_cameraPitch{-.1f};
    float m_cameraDistance{50.0f};
    float m_mouseSensitivity{0.005f};
    float m_cameraColliderRadius{ .01f };

    // Character
    float  m_characterYaw{};// Set to camera's yaw when character is moved
    float m_characterColliderRadius{ 3.f }, m_characterColliderHeight{ 200.f };
    TriVector m_characterUp{ 0.f, 1.f, 0.f};// Local up will change in case I implement laying on ground for instance
    Motor m_characterTranslation{ 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
    uint32_t m_characterMeshId{};
    MeshInstance* m_pCharacterMesh{};
    float m_movementSpeed{ 60.f }; // units/s

    void ProcessCameraMovement(InputState const& input);
    void ProcessCharacterMovement(float deltaSec);
    // Return if there were collisions
    bool ResolveCameraCollisions();
    bool ResolveCharacterCollisions();
};
