#include "QuaternionScene.h"
#include "PGA.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>

QuaternionScene::QuaternionScene(const std::string& resourceDir)
    : GameScene(resourceDir) {}

void QuaternionScene::OnInit() {
    AddGroundPlane(-10.0f, Scene::Material::Lambert(Scene::Color(0.72f, 0.88f, 0.72f)));

    m_pheasantMeshId = LoadMesh("pheasant.obj", "pheasant.png");
    m_pheasantInstanceId = AddMeshInstance(m_pheasantMeshId, TriVector(0.0f, 0.0f, 0.0f), "pheasant");
    GetInstance(m_pheasantInstanceId)->scale = m_pheasantScale;

    AddDirectionalLight(TriVector(-0.5f, -1.0f, -0.3f, 0.0f), Scene::Color(1.0f, 1.0f, 0.95f), 0.5f);
    AddPointLight(TriVector(0.0f, 20.0f, 0.0f), Scene::Color(1.0f, 1.0f, 1.0f), 1.5f, 100.0f);
}

TriVector QuaternionScene::LinePoint() const {
    return TriVector(m_linePosX, m_linePosY, m_linePosZ);
}

// The x direction turned by the pitch around the z-axis and by the yaw around the y-axis
TriVector QuaternionScene::LineDirection() const {
    const Motor pitch = Motor::Rotation(m_linePitch * RAD_TO_DEG, BiVector(0, 0, 0, 0, 0, 1));
    const Motor yaw = Motor::Rotation(m_lineYaw * RAD_TO_DEG, BiVector(0, 0, 0, 0, 1, 0));
    return ApplyMotor(yaw * pitch, TriVector(1.0f, 0.0f, 0.0f, 0.0f));
}

// The rotation axis is the join of its point and its direction (an ideal point)
BiVector QuaternionScene::BuildLine() const {
    return LinePoint() & LineDirection();
}

void QuaternionScene::OnUpdate([[maybe_unused]] float deltaTime) {
    const BiVector line = BuildLine();
    SetInstanceTransform(m_pheasantInstanceId, Motor::Rotation(m_rotationAngle * RAD_TO_DEG, line));

    DrawDebugLine(line, 15.0f, Scene::Color(0.0f, 0.0f, 0.5f));
    DrawDebugPoint(LinePoint(), 0.8f, Scene::Color(0.0f, 0.0f, 0.5f));

    // An arc showing the rotation: a point next to the axis, rotated by growing parts of the angle.
    // The plane through the axis and a reference direction has a normal perpendicular to the axis.
    const TriVector reference = std::abs(LineDirection().e013()) < 0.9f ? TriVector(0.0f, 1.0f, 0.0f, 0.0f)
                                                                        : TriVector(1.0f, 0.0f, 0.0f, 0.0f);
    const Vector plane = (line & reference).Normalized();
    const TriVector arcStart = LinePoint() + PlaneNormal(plane) * 3.0f;

    constexpr int arcSegments = 16;
    TriVector previous = arcStart;
    for (int i = 1; i <= arcSegments; ++i) {
        const float angle = m_rotationAngle * static_cast<float>(i) / arcSegments;
        const TriVector next = ApplyMotor(Motor::Rotation(angle * RAD_TO_DEG, line), arcStart);
        DrawDebugLine(previous, next, Scene::Color(1.0f, 0.5f, 0.0f));
        previous = next;
    }
}

void QuaternionScene::OnInput(const Input& input) {
    m_orbit.HandleInput(input);
    SetCamera(m_orbit.Eye(), m_orbit.target);

    if (input.WasPressed(Key::H)) m_guiVisible = !m_guiVisible;
    if (input.WasPressed(Key::R)) {
        ResetAll();
        return;
    }

    const float dt = input.DeltaTime();
    if (input.IsDown(Key::Shift)) {
        // Shift+arrows: line position X/Z, Shift+Q/E: rotation angle
        if (input.IsDown(Key::Left))  m_linePosX -= kPosSpeed * dt;
        if (input.IsDown(Key::Right)) m_linePosX += kPosSpeed * dt;
        if (input.IsDown(Key::Up))    m_linePosZ -= kPosSpeed * dt;
        if (input.IsDown(Key::Down))  m_linePosZ += kPosSpeed * dt;
        if (input.IsDown(Key::Q)) m_rotationAngle -= kAngleSpeed * dt;
        if (input.IsDown(Key::E)) m_rotationAngle += kAngleSpeed * dt;
        m_rotationAngle = std::clamp(m_rotationAngle, -kPi, kPi);
    } else {
        // Arrows: line direction yaw/pitch, Q/E: line position Y
        if (input.IsDown(Key::Left))  m_lineYaw -= kYawSpeed * dt;
        if (input.IsDown(Key::Right)) m_lineYaw += kYawSpeed * dt;
        if (input.IsDown(Key::Up))    m_linePitch += kPitchSpeed * dt;
        if (input.IsDown(Key::Down))  m_linePitch -= kPitchSpeed * dt;
        m_linePitch = std::clamp(m_linePitch, -kPi / 2.0f, kPi / 2.0f);
        if (input.IsDown(Key::Q)) m_linePosY -= kPosSpeed * dt;
        if (input.IsDown(Key::E)) m_linePosY += kPosSpeed * dt;
    }

    m_linePosX = std::clamp(m_linePosX, -20.0f, 20.0f);
    m_linePosY = std::clamp(m_linePosY, -20.0f, 20.0f);
    m_linePosZ = std::clamp(m_linePosZ, -20.0f, 20.0f);
}

void QuaternionScene::OnGui() {
    if (!m_guiVisible) return;

    ImGui::Begin("Quaternion Rotation Demo", nullptr, ImGuiWindowFlags_NoNavInputs);
    ImGui::Text("FPS: %.1f", GetFPS());
    ImGui::Separator();

    constexpr float kStep = 5.0f * DEG_TO_RAD;

    if (ImGui::CollapsingHeader("Rotation Axis", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::SliderAngle("Yaw", &m_lineYaw, -180.0f, 180.0f, "%.0f deg")) {
            m_lineYaw = std::round(m_lineYaw / kStep) * kStep;
        }
        if (ImGui::SliderAngle("Pitch", &m_linePitch, -90.0f, 90.0f, "%.0f deg")) {
            m_linePitch = std::round(m_linePitch / kStep) * kStep;
        }
        ImGui::SliderFloat("Pos X", &m_linePosX, -20.0f, 20.0f);
        ImGui::SliderFloat("Pos Y", &m_linePosY, -20.0f, 20.0f);
        ImGui::SliderFloat("Pos Z", &m_linePosZ, -20.0f, 20.0f);
    }

    if (ImGui::CollapsingHeader("Rotation", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::SliderAngle("Angle", &m_rotationAngle, -180.0f, 180.0f, "%.0f deg")) {
            m_rotationAngle = std::round(m_rotationAngle / kStep) * kStep;
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Reset [R]")) {
        ResetAll();
    }

    ImGui::Separator();
    ImGui::Text("Keyboard Shortcuts:");
    ImGui::BulletText("Arrows       Line yaw / pitch");
    ImGui::BulletText("Shift+Arrows Line pos X / Z");
    ImGui::BulletText("Q / E        Line pos Y");
    ImGui::BulletText("Shift+Q/E    Rotation angle");
    ImGui::BulletText("[R]          Reset all");
    ImGui::BulletText("[H]          Toggle GUI");
    ImGui::Text("Right-drag: Orbit  |  Scroll: Zoom");
    ImGui::End();
}

void QuaternionScene::ResetAll() {
    m_lineYaw = 0.0f;
    m_linePitch = 0.0f;
    m_linePosX = 0.0f;
    m_linePosY = 0.0f;
    m_linePosZ = 0.0f;
    m_rotationAngle = 0.0f;
}
