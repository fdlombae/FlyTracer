#include "Scenes/MainScene.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>

#include "FlyTracer.h"
#include "SDL3/SDL.h"
#include "SDL3/SDL_mouse.h"

MainScene::MainScene(const std::string& resourceDir)
    : GameScene(resourceDir) {
}

void MainScene::OnInit([[maybe_unused]] VulkanRenderer* renderer) {
    constexpr float halfWidth = 50.0f;
    constexpr float height = 30.0f;

    // Bottom plane
    AddPlane(Vector(0.0f, 0.0f, 1.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.6f, 0.6f, 0.6f)));

    // Top plane
    AddPlane(Vector(height, 0.0f, -1.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.8f, 0.8f, 0.9f)));

    // Left plane
    AddPlane(Vector(halfWidth, 1.0f, 0.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.9f, 0.5f, 0.5f)));

    // Right plane
    AddPlane(Vector(halfWidth, -1.0f, 0.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.5f, 0.9f, 0.5f)));

    // Capsule
    m_characterMeshId = LoadMesh("capsule.obj", "capsule.png");
    AddMeshInstance(m_characterMeshId, TriVector(0.f, 0.f, 0.f), "character");
    m_pCharacterMesh = FindInstance("character");
    m_pCharacterMesh ->scale = 5.f;

    // Point lights
    for (int lightIdx{}; lightIdx < 10; ++lightIdx) {
        AddPointLight(TriVector(0.0f, 20.0f, static_cast<float>(lightIdx) * 100.f),
                      Scene::Color(1.0f, 1.0f, 1.0f), 2.0f, 100.0f);
    }

    // NOTE: Excessive since get overridden immediately by ProcessCameraMovement()
    // Camera
    //m_cameraOrigin = TriVector(0.0f, 10.0f, 40.0f);
    // m_cameraTarget = TriVector(0.0f, 10.0f, 0.0f);
    // m_cameraUp = TriVector(0.0f, 1.0f, 0.0f, 0.0f);
}

void MainScene::OnUpdate(float const deltaSec) {
    UpdateFPS(deltaSec);
    ProcessCharacterMovement(deltaSec);
    ResolveCharacterCollisions();
}

void MainScene::OnInput(const InputState& input) {
    float const oldCameraYaw{ m_cameraYaw }, oldCameraPitch{ m_cameraPitch };
    ProcessCameraMovement(input);
    if (ResolveCameraCollisions())// There was a collision
    {
        // Camera did not move, so rotation should not change
        m_cameraYaw = oldCameraYaw;
        m_cameraPitch = oldCameraPitch;
    }
}

void MainScene::ProcessCameraMovement(InputState const &input) {
    m_cameraYaw -= input.mouseDeltaX * m_mouseSensitivity;
    m_cameraPitch += input.mouseDeltaY * m_mouseSensitivity;
    m_cameraPitch = std::clamp(m_cameraPitch, -1.4f, 1.4f);

    m_cameraDistance -= input.scrollDelta * 2.0f;
    m_cameraDistance = std::clamp(m_cameraDistance, 10.0f, 150.0f);

    constexpr float targetY = 15.0f;

    const float camX = std::sin(m_cameraYaw) * std::cos(m_cameraPitch) * m_cameraDistance;
    const float camY = std::sin(m_cameraPitch) * m_cameraDistance + targetY;
    const float camZ = std::cos(m_cameraYaw) * std::cos(m_cameraPitch) * m_cameraDistance;

    m_cameraOrigin = (~m_characterTranslation * TriVector(camX, camY, camZ) * m_characterTranslation).Grade3().Normalized();
    m_cameraTarget = (~m_characterTranslation * TriVector(0.0f, targetY, 0.0f) * m_characterTranslation).Grade3().Normalized();
    m_cameraUp = TriVector(0.0f, 1.0f, 0.0f);
}

void MainScene::ProcessCharacterMovement(float const deltaSec) {
    bool const* const pKeyboardState{ SDL_GetKeyboardState(nullptr) };
    BiVector direction{};// e01 - x, e02 - y, e03 - z. LHS, Y up
    if (pKeyboardState[SDL_SCANCODE_W]) direction.e03() += 1;
    if (pKeyboardState[SDL_SCANCODE_S]) direction.e03() -= 1;
    if (pKeyboardState[SDL_SCANCODE_A]) direction.e01() += 1;
    if (pKeyboardState[SDL_SCANCODE_D]) direction.e01() -= 1;
    if (direction.VNorm() == 0) return;// Not moving if no key is pressed
    direction /= direction.VNorm();// Normalizing vanishing part to prevent speed increase when moving diagonally
    float const speed{ m_movementSpeed * deltaSec };

    Motor const R{ Motor::Rotation(m_cameraYaw * 1 / DEG_TO_RAD + 180.f, BiVector{0.f, 0.f, 0.f, 0.f, 1.f, 0.f}) };
    direction = (R * direction * ~R).Grade2();// Making character move along its local axes
    // NOTE: Dividing by 2, because bireflection doubles the distance
    Motor const T{1.f, direction.e01() * speed * 0.5f, 0.f, direction.e03() * speed * 0.5f, 0.f, 0.f, 0.f, 0.f};
    m_characterTranslation = T * m_characterTranslation;
    m_pCharacterMesh->transform = R * m_characterTranslation;
}

bool MainScene::ResolveCameraCollisions() {
    bool haveCollision{};
    // 1. Iterate over all the planes in the scene
    for (Scene::GPUPlane const& gpuPlane : m_sceneData.planes) {
        // 2. For each plane, get the distance to it and check if distance is smaller than the collider's radius
        if (float const planeCameraSignedDistance{ (gpuPlane.GetPlane() & m_cameraOrigin) };// NOTE: Both objects are already normalized
            planeCameraSignedDistance < m_cameraColliderRadius)
        {
            // 3. Move the camera along plane's normal by distance - radius
            Vector vector{ gpuPlane.GetPlane() * (planeCameraSignedDistance - m_cameraColliderRadius) };
            // NOTE: Vector's Euclidean coefficients are the components of its Euclidean normal
            Motor const T{ 1.f, vector.e1(), vector.e2(), vector.e3(), 0.f, 0.f, 0.f, 0.f};
            m_cameraOrigin = (T * m_cameraOrigin * ~T).Grade3();
            haveCollision = true;
        }
    }
    return haveCollision;
}

bool MainScene::ResolveCharacterCollisions()
{
    return false;
    // 1. Iterate over all the planes in the scene
    for (Scene::GPUPlane const& gpuPlane : m_sceneData.planes) {
        // 2. For each plane, get the distance to it and check if distance is smaller than the collider's radius
        TriVector const characterOrigin{ (m_characterTranslation * TriVector{0.f, 0.f, 0.f, 1.f} * ~m_characterTranslation).Grade3() };
        Vector const plane{ gpuPlane.GetPlane() };
        if (float const planeCharacterSignedDistance{ (gpuPlane.GetPlane() & characterOrigin) };// NOTE: Both objects are already normalized
            planeCharacterSignedDistance < m_characterColliderRadius)
        {
            // 3. Move the character along plane's normal by distance - radius
            Vector pgaPlane{ gpuPlane.GetPlane().Normalized() * (planeCharacterSignedDistance - m_characterColliderRadius) };
            // NOTE: Vector's Euclidean coefficients are the components of its Euclidean normal
            Motor const T{ 1.f, pgaPlane.e1(), pgaPlane.e2(), pgaPlane.e3(), 0.f, 0.f, 0.f, 0.f};
            m_characterTranslation = T * m_characterTranslation;
            std::cout << planeCharacterSignedDistance << std::endl;
        }
    }
}
