#include "../include/Scenes/MainScene.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>

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

    // Pheasant mesh
    m_pheasantMeshId = LoadMesh("pheasant.obj", "pheasant.png");
    AddMeshInstance(m_pheasantMeshId, TriVector(0.0f, m_pheasantHeight, 0.0f), "pheasant");

    if (auto* pheasant = FindInstance("pheasant")) {
        pheasant->scale = m_pheasantScale;
    }

    // Point light
    AddPointLight(TriVector(0.0f, 20.0f, 0.0f),
                  Scene::Color(1.0f, 1.0f, 1.0f), 2.0f, 100.0f);

    // Camera
    m_cameraEye = TriVector(0.0f, 15.0f, 60.0f);
    m_cameraTarget = TriVector(0.0f, 10.0f, 0.0f);
    m_cameraUp = TriVector(0.0f, 1.0f, 0.0f, 0.0f);

}

void MainScene::OnUpdate(float const deltaTime) {
    UpdateFPS(deltaTime);
}

void MainScene::OnInput(const InputState& input) {
    m_cameraYaw -= input.mouseDeltaX * m_mouseSensitivity;
    m_cameraPitch += input.mouseDeltaY * m_mouseSensitivity;
    m_cameraPitch = std::clamp(m_cameraPitch, -1.4f, 1.4f);

    m_cameraDistance -= input.scrollDelta * 2.0f;
    m_cameraDistance = std::clamp(m_cameraDistance, 10.0f, 150.0f);

    constexpr float targetY = 15.0f;

    const float camX = std::sin(m_cameraYaw) * std::cos(m_cameraPitch) * m_cameraDistance;
    const float camY = std::sin(m_cameraPitch) * m_cameraDistance + targetY;
    const float camZ = std::cos(m_cameraYaw) * std::cos(m_cameraPitch) * m_cameraDistance;

    m_cameraEye = TriVector(camX, camY, camZ);
    m_cameraTarget = TriVector(0.0f, targetY, 0.0f);
    m_cameraUp = TriVector(0.0f, 1.0f, 0.0f);

    ProcessMovement(input);
}

void MainScene::OnGui() {
    SDL_HideCursor();
    ImGui::Begin("Test Box Scene");
    ImGui::Text("FPS: %.1f", GetFPS());
    ImGui::Text("Controls:");
    ImGui::Text("WASD to move");
    ImGui::Text("Move mouse to look around");
    ImGui::End();
}

void MainScene::OnShutdown() {}

void MainScene::ProcessMovement(const InputState& input) {
    // TODO: Implement
    return;
}
