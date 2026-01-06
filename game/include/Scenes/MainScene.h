#pragma once
#include <functional>

#include "Collisions.h"
#include "GameScene.h"
#include "GlobalConstants.h"
#include "BoltManager.h"
#include "EnemyManager.h"

class MainScene final : public GameScene {
public:
    explicit MainScene(const std::string& resourceDir);

    void OnInit(VulkanRenderer* renderer) override;
    void OnUpdate(float deltaSec) override;
    void OnInput(const InputState& input) override;
    void OnLMBUp() override;

private:
    // Camera orbit
    float m_cameraYaw{-0.75f};
    float m_cameraPitch{-.1f};
    float m_cameraDistance{30.0f};
    float m_mouseSensitivity{0.005f};
    float m_cameraColliderRadius{ .01f };

    float m_cursorX{}, m_cursorY{};

    // Character
    std::string const m_characterMeshName{ "character" };
    float  m_characterYawRadians{};// Set to camera's yaw when character is moved
    Motor m_characterTranslation{ 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
    uint32_t m_characterMeshId{};
    float m_movementSpeed{ 60.f }; // units/s
    TriVector m_gunSocket{ - 0.5f * capsuleColliderRadius, 0.75f * capsuleColliderHeight, 0.f};
    BiVector m_characterInitialDirection{ -zAxis };
    BoltManager m_boltManager;

    // Enemies
    EnemyManager m_EnemyManager;

    void OnGui() override;

    void ProcessCameraMovement(InputState const& input);
    void ProcessCharacterMovement(float deltaSec);
    // Collision
    // Return if there were collisions
    bool ResolveCameraCollisions();
    bool ResolveCharacterPlaneCollisions();
    //bool ResolveCharacterEnemyCollisions();
    template <Collider T>
    bool ResolveWallCollisions(T const& collider, const std::function<void(Motor const&)>& sink);
    // Character
    TriVector GetCharacterOrigin() const;
    Motor GetCharacterDirection() const;
    Motor GetCharacterRotation() const;
    void Shoot();
};

template <Collider T>
bool MainScene::ResolveWallCollisions(T const& collider, std::function<void(Motor const&)> const& sink)
{
    bool hasCollision{};
    for (Scene::GPUPlane const& gpuPlane : m_sceneData.planes)
    {
        if (auto const transform{ ProcessWallCollision(collider, gpuPlane.GetPlane())};
            transform.has_value())
        {
            sink(transform.value());
            hasCollision = true;// Not returning in case there are other collisions to process
        }
    }
    return hasCollision;
}
