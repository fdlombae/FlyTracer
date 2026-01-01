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
    m_characterMeshId = LoadMesh("capsule.obj", "");
    AddMeshInstance(m_characterMeshId, TriVector(0.f, 0.f, 0.f), "character");
    m_pCharacterMesh = FindInstance("character");
    m_pCharacterMesh ->scale = 5.f;

    // Point light
    AddPointLight(TriVector(0.0f, 20.0f, 0.0f),
                  Scene::Color(1.0f, 1.0f, 1.0f), 2.0f, 100.0f);

    // Camera
    m_cameraOrigin = TriVector(0.0f, 10.0f, 60.0f);
    m_cameraTarget = TriVector(0.0f, 10.0f, 0.0f);
    m_cameraUp = TriVector(0.0f, 1.0f, 0.0f, 0.0f);
}

void MainScene::OnUpdate(float const deltaSec) {
    UpdateFPS(deltaSec);
    ProcessMovement(deltaSec);
}

void MainScene::OnInput(const InputState& input) {
    //ProcessCameraMovement(input);
    ResolveCameraCollisions();
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

    m_cameraOrigin = TriVector(camX, camY, camZ);
    m_cameraTarget = TriVector(0.0f, targetY, 0.0f);
    m_cameraUp = TriVector(0.0f, 1.0f, 0.0f);
}

void MainScene::ProcessMovement(float const deltaSec) {
    bool const* const pKeyboardState{ SDL_GetKeyboardState(nullptr) };
    BiVector direction{};// e01 - x, e02 - y, e03 - z. LHS, Y up
    if (pKeyboardState[SDL_SCANCODE_W]) direction.e03() += 1;
    if (pKeyboardState[SDL_SCANCODE_S]) direction.e03() -= 1;
    if (pKeyboardState[SDL_SCANCODE_A]) direction.e01() += 1;
    if (pKeyboardState[SDL_SCANCODE_D]) direction.e01() -= 1;
    if (direction.VNorm() == 0) return;// Not moving if no key is pressed
    direction /= direction.VNorm();// Normalizing vanishing part to prevent speed increase when moving diagonally
    float const speed{ m_movementSpeed * deltaSec };
    Motor const T{1.f, direction.e01() * speed * 0.5f, 0.f, direction.e03() * speed * 0.5f, 0.f, 0.f, 0.f, 0.f};
    m_pCharacterMesh->transform = ~T * m_pCharacterMesh->transform;
    m_cameraOrigin = ((speed * T) * m_cameraOrigin * ~(speed * T)).Grade3();
    m_cameraTarget = ((speed * T) * m_cameraTarget * ~(speed * T)).Grade3();

}

void MainScene::ResolveCameraCollisions() {
    // 1. Iterate over all the planes in the scene
    for (Scene::GPUPlane const& plane : m_sceneData.planes) {
        // 2. For each plane, get the distance to it
        if (float const planeCameraDistance{ (m_cameraOrigin & plane.distance).Norm() };
            planeCameraDistance < m_cameraColliderRadius)
        {
            // 3. If distance is smaller than radius,
            //    move the sphere in the opposite direction by distance - radius
            // 3.1. Getting the opposite direction
            TriVector planeToCameraDir{ m_cameraOrigin };
        }
    }

}

void MainScene::OnGui() {
    SDL_HideCursor();
    ImGui::Begin("Main Scene");
    ImGui::Text("FPS: %.1f", GetFPS());
    ImGui::Text("Controls:");
    ImGui::Text("WASD to move");
    ImGui::Text("Move mouse to look around");
    ImGui::End();
}

void MainScene::OnShutdown() {}


