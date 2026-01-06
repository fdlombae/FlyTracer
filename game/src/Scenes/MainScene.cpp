#include <algorithm>
#include <cmath>

#include "Scenes/MainScene.h"
#include "Collisions.h"
#include "imgui.h"
#include "FlyTracer.h"
#include "SDL3/SDL.h"
#include "Utils.h"

MainScene::MainScene(const std::string& resourceDir)
    : GameScene(resourceDir)
    , m_EnemyManager(*this)
{
}

void MainScene::OnInit([[maybe_unused]] VulkanRenderer* renderer) {
    constexpr float halfWidth = 50.0f;
    constexpr float height = 30.0f;

    // Bottom plane
    AddPlane(Vector(0.0f, 0.0f, 1.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.6f, 0.6f, 0.6f)));

    // Top plane
    AddPlane(Vector(height, 0.0f, -1.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.f, 0.8f, 0.f)));

    // Left plane
    AddPlane(Vector(halfWidth, 1.0f, 0.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.9f, 0.5f, 0.5f)));

    // Right plane
    AddPlane(Vector(halfWidth, -1.0f, 0.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.5f, 0.9f, 0.5f)));

    // Capsule
    m_characterMeshId = LoadMesh("capsule.obj", "capsule.png");
    AddMeshInstance(m_characterMeshId, TriVector(0.f, 0.f, 0.f), m_characterMeshName);
    if (MeshInstance* pCharacterMesh = FindInstance(m_characterMeshName); pCharacterMesh)
    {
        pCharacterMesh->scale = capsuleScale;
    }

    m_EnemyManager.AddEnemy({0.f, 0.f, 50.f});
    m_EnemyManager.AddEnemy({10.f, 0.f, 100.f});
    m_EnemyManager.AddEnemy({-25.f, 0.f, 175.f});

    // Point lights
    for (int lightIdx{}; lightIdx < 10; ++lightIdx) {
        AddPointLight(TriVector(0.0f, 20.0f, static_cast<float>(lightIdx) * 100.f),
                      Scene::Color(1.0f, 1.0f, 1.0f), 2.0f, 100.0f);
    }

    // NOTE: Excessive since get overridden immediately by ProcessCameraMovement()
    // Camera
    //m_cameraOrigin = TriVector(0.0f, 10.0f, 40.0f);
    // m_cameraTarget = TriVector(0.0f, 10.0f, 0.0f);
    m_cameraUp = TriVector(0.0f, 1.0f, 0.0f);
}

void MainScene::OnUpdate(float const deltaSec) {
    UpdateFPS(deltaSec);
    ProcessCharacterMovement(deltaSec);
    ResolveCharacterPlaneCollisions();
    m_EnemyManager.Update(deltaSec, GetCharacterOrigin());

    m_boltManager.Update(deltaSec);
    // Drawing bolts
    for (Bolt const& bolt : m_boltManager.GetBolts())
    {
        auto const[A, B]{ bolt.GetPoints() };
        DrawDebugLine(A, B);
    }
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

void MainScene::OnLMBUp()
{
    Shoot();
}

void MainScene::ProcessCameraMovement(InputState const &input) {
    float cursorX, cursorY;
    SDL_GetMouseState(&cursorX, &cursorY);
    m_cursorX -= m_cursorX - cursorX;
    m_cursorY -= m_cursorY - cursorY;

    m_cameraYaw -= input.mouseDeltaX * m_mouseSensitivity;
    m_cameraPitch += input.mouseDeltaY * m_mouseSensitivity;
    //m_cameraPitch = std::clamp(m_cameraPitch, -1.4f, 1.4f);

    m_cameraDistance -= input.scrollDelta * 2.0f;
    //m_cameraDistance = std::clamp(m_cameraDistance, 10.0f, 150.0f);

    constexpr float targetY = 15.0f;

    const float camX = std::sin(m_cameraYaw) * std::cos(m_cameraPitch) * m_cameraDistance;
    const float camY = std::sin(m_cameraPitch) * m_cameraDistance + targetY;
    const float camZ = std::cos(m_cameraYaw) * std::cos(m_cameraPitch) * m_cameraDistance;

    m_cameraOrigin = (~m_characterTranslation * TriVector(camX, camY, camZ) * m_characterTranslation).Grade3().Normalized();
    m_cameraTarget = (~m_characterTranslation * TriVector(0.0f, targetY, 0.0f) * m_characterTranslation).Grade3().Normalized();
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

    // Rotating character in the movement direction
    m_characterYawRadians = m_cameraYaw + std::numbers::pi;
    Motor const R{ GetCharacterRotation() };
    direction = (R * direction * ~R).Grade2();// Making character move along its local axes
    // NOTE: Dividing by 2, because bireflection doubles the distance
    Motor const T{1.f, direction.e01() * speed * 0.5f, 0.f, direction.e03() * speed * 0.5f, 0.f, 0.f, 0.f, 0.f};
    m_characterTranslation = T * m_characterTranslation;
    if (MeshInstance* pCharacterMesh{ FindInstance(m_characterMeshName)}; pCharacterMesh)// Must be done, after a new instance is created, the order is not valid anymore.
    {
        pCharacterMesh->transform = R * m_characterTranslation;
    }
}

bool MainScene::ResolveCameraCollisions() {
    return ResolveWallCollisions(
        Sphere( m_cameraOrigin, m_cameraColliderRadius ),
        [this](Motor const& T){ m_cameraOrigin = (T * m_cameraOrigin * ~T).Grade3(); }
        );
}

bool MainScene::ResolveCharacterPlaneCollisions()
{
    return ResolveWallCollisions(
        Capsule( GetCharacterOrigin(), capsuleColliderRadius, capsuleColliderHeight ),
        [this](Motor const& T){ m_characterTranslation = T * m_characterTranslation; }
        );
}

TriVector MainScene::GetCharacterOrigin() const
{
    return (m_characterTranslation * TriVector{0.f,  0.f, 0.f} * ~m_characterTranslation).Grade3().Normalized();
}

Motor MainScene::GetCharacterDirection() const
{
    Motor const R{ GetCharacterRotation() };
    return R * m_characterInitialDirection * ~R;
}

Motor MainScene::GetCharacterRotation() const
{
    return Motor::Rotation(m_characterYawRadians * RAD_TO_DEG, yAxis);
}


void MainScene::Shoot()
{
    // Getting gun socket
    Motor const characterRotation{ GetCharacterRotation() };
    TriVector gunSocket{ (characterRotation * m_gunSocket * ~characterRotation).Grade3() };
    gunSocket = (~m_characterTranslation * gunSocket * m_characterTranslation).Grade3();

    // Getting trajectory
    Motor const gunPointOffset{ 1.f, gunSocket.e032(), gunSocket.e013(), gunSocket.e021(), 0.f, 0.f, 0.f, 0.f };
    BiVector const trajectory{ (gunPointOffset * GetCharacterDirection() * ~gunPointOffset).Grade2().Normalized() };

    // Creating bolt
    m_boltManager.AddBolt({gunSocket, trajectory});
}

void MainScene::OnGui()
{
    RenderDebugDraw();
    float x, y;
    SDL_GetMouseState(&x, &y);

    ImGui::Begin("Main Scene");
    ImGui::Text("Mouse location: %.1f, %.1f", x, y);
    ImGui::End();

    RenderDebugDraw();// Rendering blaster bolts
}

