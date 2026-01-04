#include "Scenes/MainScene.h"
#include <algorithm>
#include <cmath>
#include "imgui.h"
#include "FlyTracer.h"
#include "SDL3/SDL.h"

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
        pCharacterMesh->scale = m_capsuleScale;
    }
    AddEnemy();

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
    ResolveCharacterCollisions();
    RotateEnemy();
    UpdateEnemyMeshTransform();
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

    Motor const R{ Motor::Rotation(m_cameraYaw * RAD_TO_DEG + 180.f, m_yAxis) };
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
    bool hasCollision{};
    // 1. Iterate over all the planes in the scene
    for (Scene::GPUPlane const& gpuPlane : m_sceneData.planes) {
        // 2. For each plane, get the distance to it and check if distance is smaller than the collider's radius
        if (float const planeCameraSignedDistance{ (gpuPlane.GetPlane() & m_cameraOrigin) };// NOTE: Both objects are already normalized
            planeCameraSignedDistance < m_cameraColliderRadius)
        {
            // 3. Move the camera along plane's normal by distance - radius
            Vector const t{ gpuPlane.GetPlane() * (planeCameraSignedDistance - m_cameraColliderRadius) };// Translator
            // NOTE: Vector's Euclidean coefficients are the components of its Euclidean normal
            Motor const T{ 1.f, t.e1(), t.e2(), t.e3(), 0.f, 0.f, 0.f, 0.f};
            m_cameraOrigin = (T * m_cameraOrigin * ~T).Grade3();
            hasCollision = true;// Not returning in case there are other collisions to process
        }
    }
    return hasCollision;
}

bool MainScene::ResolveCharacterCollisions()
{
    bool hasCollision{};
    // 1. Getting the top and bottom spheres(origins)
    TriVector const topOrigin{ GetCharacterTopSphereOrigin() }, bottomOrigin{ GetCharacterBottomSphereOrigin() };

    // 2. Iterating over planes
    for (Scene::GPUPlane const& gpuPlane : m_sceneData.planes)
    {
        // 2.1. Getting the signed distances of each sphere to the plane
        float const topDistance{ (gpuPlane.GetPlane() & topOrigin) },
            bottomDistance{ (gpuPlane.GetPlane() & bottomOrigin) };
        // 2.2. Finding the closest sphere
        float const smallestSignedDistance{
            std::abs(topDistance) < std::abs(bottomDistance) ? topDistance : bottomDistance
        };
        // 2.3. Seeing if the distance of the closest one is smaller than the radius
        if (std::abs(smallestSignedDistance) < m_capsuleColliderRadius)
        {
            // 4. Moving the character away by normal * (distance - radius)
            Vector vector{ gpuPlane.GetPlane() * (smallestSignedDistance - m_capsuleColliderRadius) };
            // NOTE: Vector's Euclidean coefficients are the components of its Euclidean normal
            Motor const T{ 1.f, vector.e1(), vector.e2(), vector.e3(), 0.f, 0.f, 0.f, 0.f};
            m_characterTranslation = T * m_characterTranslation;
            hasCollision = true;
        }

    }

    return hasCollision;
}

TriVector MainScene::GetCharacterTopSphereOrigin() const
{
    TriVector topOrigin{GetCharacterOrigin()};
    topOrigin.e013() = m_capsuleColliderHeight - m_capsuleColliderRadius;
    return topOrigin;
}

TriVector MainScene::GetCharacterBottomSphereOrigin() const
{
    TriVector bottomOrigin{ GetCharacterOrigin() };
    bottomOrigin.e013() = m_capsuleColliderRadius;
    return bottomOrigin;
}

TriVector MainScene::GetCharacterOrigin() const
{
    return (m_characterTranslation * TriVector{0.f,  0.f, 0.f} * ~m_characterTranslation).Grade3().Normalized();
}

void MainScene::OnGui()
{
    RenderDebugDraw();
    float x, y;
    SDL_GetMouseState(&x, &y);

    ImGui::Begin("Main Scene");
    ImGui::Text("Mouse location: %.1f, %.1f", x, y);
    ImGui::End();
}

void MainScene::AddEnemy()
{
    m_enemyMeshId = LoadMesh("capsule.obj", "capsule.png");
    AddMeshInstance(m_enemyMeshId, TriVector(0.f, 0.f, 20.f), "enemy");
    if (MeshInstance* pEnemyMesh = FindInstance(m_enemyMeshName); pEnemyMesh)
    {
        pEnemyMesh->scale = m_capsuleScale;
        m_enemyTranslation = pEnemyMesh->transform;
    }
}

void MainScene::RotateEnemy()
{
    // 1. Retrieving character and enemy origins, and enemy view direction
    TriVector const characterOrigin{ GetCharacterOrigin().Normalized() },
        enemyOrigin{ GetEnemyOrigin().Normalized() };
    BiVector const enemyViewDirection{ (m_enemyRotation * m_enemyInitialDirection * ~m_enemyRotation).Grade2().Normalized() };

    // 2. Calculating final enemy's direction
    BiVector const finalEnemyViewDirection{ (enemyOrigin & characterOrigin).Normalized() };

    // 3. Getting the angle between direction vectors
    // 3.1. Getting cos of the angle between the direction vectors
    // NOTE: enemyViewDirection and finalEnemyViewDirection are normalized, so there's no need to divide by the product of norms
    float cos{ (enemyViewDirection | finalEnemyViewDirection) };

    // Clamping dot to [-1, 1], because std::acos will result in NaN outside of it
    // NOTE: The value can get outside of this range due to floating point precision errors
    if (cos < -1) cos = -1;
    else if (cos > 1) cos = 1;

    // 3.2. Getting sin of the angle between the direction vectors
    // NOTE: Not using meet directly, because that alone will result in void
    BiVector const wedge{(enemyViewDirection * finalEnemyViewDirection).Grade2()};
    float const sinSign{ GetEuclideanSign(wedge) };
    std::cout << sinSign << std::endl;
    float const sin{ sinSign * wedge.Norm() };
    //std::cout << "Sin: " << sin << std::endl;

    // 3.3. Finding the angle
    float const directionRadians{ std::atan2(sin, cos) };

    // 4. Creating rotation motor and applying it to the enemy's mesh
    Motor const R{ Motor::Rotation(directionRadians * RAD_TO_DEG, m_yAxis) };
    m_enemyRotation = R * m_enemyRotation;

    std::cout << "Direction degrees: " << directionRadians * RAD_TO_DEG << std::endl << std::endl;
}

TriVector MainScene::GetEnemyOrigin() const
{
    return (m_enemyTranslation * TriVector{0.f,  0.f, 0.f} * ~m_enemyTranslation).Grade3().Normalized();
}

void MainScene::UpdateEnemyMeshTransform()
{
    if (MeshInstance* pEnemyMesh = FindInstance(m_enemyMeshName); pEnemyMesh)
    {
        pEnemyMesh->transform = m_enemyRotation * m_enemyTranslation;
    }
}

float MainScene::GetSign(float const value) const
{
    float const denominator{ std::abs(value) < SDL_FLT_EPSILON ? 1.f : std::abs(value) };
    return value / denominator;
}

float MainScene::GetEuclideanSign(BiVector const& biVector) const
{
    float const a{ std::abs(biVector.e23()) < SDL_FLT_EPSILON ? 1.f : biVector.e23() },
        b{ std::abs(biVector.e31()) < SDL_FLT_EPSILON ? 1.f : biVector.e31() },
        c{ std::abs(biVector.e12()) < SDL_FLT_EPSILON ? 1.f : biVector.e12()  };
    return GetSign(a*b*c);
}
