#include "TestBoxScene.h"
#include "PGA.h"
#include <array>
#include <cmath>
#include <imgui.h>
#include <optional>

namespace {

const BiVector kYAxis = BiVector::LineFromPoints(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

} // anonymous namespace

TestBoxScene::TestBoxScene(const std::string& resourceDir)
    : GameScene(resourceDir) {}

void TestBoxScene::OnInit() {
    constexpr float halfWidth = 50.0f;
    constexpr float height = 30.0f;

    // Bottom, top, left and right planes a*x + b*y + c*z + d = 0, with their normals (a, b, c) pointing into the box
    AddPlane(Vector::Plane(0.0f, 1.0f, 0.0f, 0.0f), Scene::Material::Lambert(Scene::Color(0.6f, 0.6f, 0.6f)));
    AddPlane(Vector::Plane(0.0f, -1.0f, 0.0f, height), Scene::Material::Lambert(Scene::Color(0.8f, 0.8f, 0.9f)));
    AddPlane(Vector::Plane(1.0f, 0.0f, 0.0f, halfWidth), Scene::Material::Lambert(Scene::Color(0.9f, 0.5f, 0.5f)));
    AddPlane(Vector::Plane(-1.0f, 0.0f, 0.0f, halfWidth), Scene::Material::Lambert(Scene::Color(0.5f, 0.9f, 0.5f)));

    m_sphere1Id = AddSphere(TriVector::Point(m_sphere1Radius, m_sphere1Height, 0.0f), 5.0f,
                            Scene::Material::Metal(Scene::Color(0.9f, 0.3f, 0.3f), 0.2f));
    m_sphere2Id = AddSphere(TriVector::Point(-m_sphere2Radius, m_sphere2Height, 0.0f), 8.0f,
                            Scene::Material::Glossy(Scene::Color(0.3f, 0.3f, 0.9f), 0.1f));

    m_pheasantMeshId = LoadMesh("pheasant.obj", "pheasant.png");
    AddMeshInstance(m_pheasantMeshId, TriVector::Point(0.0f, m_pheasantHeight, 0.0f), "pheasant");
    AddMeshInstance(m_pheasantMeshId, TriVector::Point(5.0f, m_pheasantHeight, 0.0f), "pheasant2");
    if (auto* pheasant = FindInstance("pheasant")) {
        pheasant->scale = m_pheasantScale;
    }

    const uint32_t teapotMeshId = LoadMesh("teapot.obj", "teapot.png");
    AddMeshInstance(teapotMeshId, TriVector::Point(-10.0f, 10.0f, 0.0f));

    AddPointLight(TriVector::Point(0.0f, 20.0f, 0.0f), Scene::Color(1.0f, 1.0f, 1.0f), 2.0f, 100.0f);
}

void TestBoxScene::OnUpdate(float deltaTime) {
    // Rotate the spheres around the vertical axis through the origin
    m_rotationAngle = std::fmod(m_rotationAngle + m_rotationSpeed * deltaTime, 2.0f * kPi);
    const Motor rotation = Motor::Rotation(Angle::FromRadians(m_rotationAngle), kYAxis);

    if (auto* sphere = GetSphere(m_sphere1Id)) {
        sphere->center = ApplyMotor(rotation, TriVector::Point(m_sphere1Radius, m_sphere1Height, 0.0f));
    }
    if (auto* sphere = GetSphere(m_sphere2Id)) {
        sphere->center = ApplyMotor(rotation, TriVector::Point(-m_sphere2Radius, m_sphere2Height, 0.0f));
    }

    // The pheasant sways sideways and turns along with the spheres
    m_pheasantTime += m_pheasantSpeed * deltaTime;
    if (auto* pheasant = FindInstance("pheasant")) {
        const float sway = m_pheasantSway * std::sin(m_pheasantTime);
        pheasant->transform = rotation * Motor::Translation(sway, m_pheasantHeight, 0.0f);
    }

    for (const uint32_t id : GetLightIds()) {
        if (const auto* light = GetLight(id); light->type != Scene::LightType::Directional) {
            DrawDebugPoint(light->position, 1.0f, light->color);
        }
    }
}

void TestBoxScene::OnInput(const Input& input) {
    m_orbit.HandleInput(input);
    SetCamera(m_orbit.Eye(), m_orbit.target);

    if (input.WasPressed(Key::L)) SpawnLight();
}

void TestBoxScene::OnGui() {
    ImGui::Begin("Test Box Scene");
    ImGui::Text("FPS: %.1f", GetFPS());
    ImGui::Separator();
    ImGui::Text("Box: 100 wide, 30 high");
    ImGui::Text("4 Planes (bottom, top, left, right)");
    ImGui::Text("2 Spheres (rotating)");
    ImGui::Text("Pheasant (sine wave, scale=%.2f)", m_pheasantScale);
    ImGui::Separator();
    ImGui::Text("Sphere Rotation:");
    ImGui::Text("  Angle: %.2f rad (%.1f deg)", m_rotationAngle, m_rotationAngle * RAD_TO_DEG);
    ImGui::SliderFloat("Speed", &m_rotationSpeed, 0.0f, 3.0f);
    ImGui::Separator();
    DrawLightsGui();
    ImGui::Separator();
    DrawInstancesGui();
    ImGui::Separator();
    ImGui::Text("Controls:");
    ImGui::Text("  Right-drag: Orbit camera");
    ImGui::Text("  Scroll: Zoom");
    ImGui::Text("  L: Spawn light");
    ImGui::Separator();
    ImGui::Text("Camera Yaw: %.2f", m_orbit.yaw);
    ImGui::Text("Camera Pitch: %.2f", m_orbit.pitch);
    ImGui::Text("Camera Distance: %.1f", m_orbit.distance);
    ImGui::End();
}

// Each light is turned a golden angle further around the vertical axis, so they spread around the box
void TestBoxScene::SpawnLight() {
    constexpr float goldenAngle = 137.5f;
    const std::array colors{Scene::Color(1.0f, 0.5f, 0.3f), Scene::Color(0.3f, 0.6f, 1.0f),
                            Scene::Color(0.4f, 1.0f, 0.4f), Scene::Color(1.0f, 0.9f, 0.4f)};

    const Motor turn = Motor::Rotation(Angle::FromDegrees(goldenAngle * static_cast<float>(m_spawnedLightCount)),
                                       kYAxis);
    const TriVector position = ApplyMotor(turn, TriVector::Point(30.0f, 15.0f, 0.0f));
    AddPointLight(position, colors[m_spawnedLightCount % colors.size()], 1.5f, 30.0f);
    ++m_spawnedLightCount;
}

void TestBoxScene::DrawLightsGui() {
    ImGui::Text("Lights: %zu", GetLightIds().size());
    ImGui::SameLine();
    if (ImGui::SmallButton("Spawn [L]")) SpawnLight();

    std::optional<uint32_t> removed;
    for (const uint32_t id : GetLightIds()) {
        const auto* light = GetLight(id);
        const TriVector p = light->position;
        ImGui::PushID(static_cast<int>(id));
        if (ImGui::SmallButton("Remove")) removed = id;
        ImGui::SameLine();
        ImGui::ColorButton("##color", ImVec4(light->color.r, light->color.g, light->color.b, 1.0f),
                           ImGuiColorEditFlags_NoTooltip, ImVec2(12.0f, 12.0f));
        ImGui::SameLine();
        ImGui::Text("Light %u (%.0f, %.0f, %.0f)", id, p.e032(), p.e013(), p.e021());
        ImGui::PopID();
    }
    if (removed) RemoveLight(*removed);
}

void TestBoxScene::DrawInstancesGui() {
    ImGui::Text("Mesh instances: %zu", GetInstanceIds().size());

    std::optional<uint32_t> removed;
    for (const uint32_t id : GetInstanceIds()) {
        const auto* instance = GetInstance(id);
        ImGui::PushID(static_cast<int>(id));
        if (ImGui::SmallButton("Remove")) removed = id;
        ImGui::SameLine();
        ImGui::Text("Instance %u: %s", id, instance->name.empty() ? "unnamed" : instance->name.c_str());
        ImGui::PopID();
    }
    if (removed) RemoveMeshInstance(*removed);
}
