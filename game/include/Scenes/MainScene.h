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
    float m_cameraDistance{40.0f};
    float m_mouseSensitivity{0.005f};
    float m_cameraColliderRadius{ .01f };

    float m_cursorX{}, m_cursorY{};

    float m_capsuleScale{ 5.f };
    float m_capsuleColliderRadius{ 3.f }, m_capsuleColliderHeight{ 6.f };
    // Character
    std::string const m_characterMeshName{ "character" };
    float  m_characterYaw{};// Set to camera's yaw when character is moved
    Motor m_characterTranslation{ 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
    uint32_t m_characterMeshId{};
    MeshInstance* m_pCharacterMesh{};
    float m_movementSpeed{ 60.f }; // units/s
    // Enemy
    std::string const m_enemyMeshName{ "enemy" };
    uint32_t m_enemyMeshId{};
    TriVector m_GunSocket{ m_capsuleColliderRadius, 0.5f * m_capsuleColliderHeight, 0.f};

    void ProcessCameraMovement(InputState const& input);
    void ProcessCharacterMovement(float deltaSec);
    // Return if there were collisions
    bool ResolveCameraCollisions();
    bool ResolveCharacterCollisions();
    void OnGui() override;
    // Enemy
    void AddEnemy();
    void UpdateEnemy(float deltaSec);
};
