#include "BiReflectionScene.h"
#include "PGA.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <string>

BiReflectionScene::BiReflectionScene(const std::string& resourceDir, DemoMode mode)
    : GameScene(resourceDir), m_demoMode(mode) {}

void BiReflectionScene::OnInit() {
    AddGroundPlane(-10.0f, Scene::Material::Lambert(Scene::Color(0.72f, 0.88f, 0.72f)));

    m_pheasantMeshId = LoadMesh("pheasant.obj", "pheasant.png");
    m_pheasantInstanceId = AddMeshInstance(m_pheasantMeshId, TriVector(0.0f, 0.0f, 0.0f), "pheasant");
    GetInstance(m_pheasantInstanceId)->scale = kPheasantScale;

    // A separate quad mesh per plane, so each can have its own color
    const std::array<Scene::Color, 2> quadColors{Scene::Color(1.0f, 1.0f, 0.0f), Scene::Color(1.0f, 0.0f, 1.0f)};
    for (size_t i = 0; i < quadColors.size(); ++i) {
        m_quadMeshIds[i] = LoadMesh("quad.obj");
        GetMesh(m_quadMeshIds[i])->GetMaterial(0) = Scene::Material::Lambert(quadColors[i]);
        m_planes[i].color = quadColors[i];
    }

    AddDirectionalLight(TriVector(-0.5f, -1.0f, -0.3f, 0.0f), Scene::Color(1.0f, 1.0f, 0.95f), 0.5f);
    AddPointLight(TriVector(0.0f, 20.0f, 0.0f), Scene::Color(1.0f, 1.0f, 1.0f), 1.5f, 100.0f);

    SetDebugDrawEnabled(true);
    SetupDemoMode();
}

void BiReflectionScene::SetupDemoMode() {
    if (m_demoMode == DemoMode::Interactive) return;

    AddPlane();
    AddPlane();

    // Two planes through the y-axis 30 degrees apart, or two parallel planes 8 apart
    const bool rotation = m_demoMode == DemoMode::Rotation;
    m_planes[0].yaw = rotation ? 105.0f * DEG_TO_RAD : 0.0f;
    m_planes[1].yaw = rotation ? 75.0f * DEG_TO_RAD : 0.0f;
    m_planes[0].distance = rotation ? 0.0f : -4.0f;
    m_planes[1].distance = rotation ? 0.0f : 4.0f;

    m_reflected = {true, true};
    for (int i = 0; i < 2; ++i) {
        UpdatePlaneInstance(i);
        ReflectOverPlane(i);
    }
    m_selectedPlane = 0;
}

void BiReflectionScene::OnUpdate(float deltaTime) {
    for (int i = 0; i < m_planeCount; ++i) {
        UpdatePlane(i);
        UpdatePlaneInstance(i);
        DrawPlaneNormal(m_planes[i]);
    }

    if (m_planeCount == 2) {
        ComputeIntersection();
        if (m_planesIntersect) {
            const Scene::Color lineColor(0.0f, 0.0f, 0.5f);
            DrawDebugLine(m_intersectionLine, 15.0f, lineColor);
            const TriVector closestToOrigin = ClosestPointOnLine(m_intersectionLine, TriVector(0.0f, 0.0f, 0.0f));
            DrawDebugPoint(closestToOrigin, 0.8f, lineColor);
        }
    }

    if (!m_animating) return;

    m_animProgress = std::min(m_animProgress + deltaTime / m_animDuration, 1.0f);
    m_animating = m_animProgress < 1.0f;
    const float t = m_animProgress * m_animProgress * (3.0f - 2.0f * m_animProgress);  // Smoothstep

    // The bireflection motor raised to the power t
    if (auto* pheasant = GetInstance(m_pheasantInstanceId)) {
        pheasant->transform = MotorPower(m_animMotor, t) * m_originalTransform;
    }
}

void BiReflectionScene::OnInput(const Input& input) {
    m_orbit.HandleInput(input);
    SetCamera(m_orbit.Eye(), m_orbit.target);

    if (input.WasPressed(Key::H)) m_guiVisible = !m_guiVisible;

    // [1] / [2] Add and select a plane
    if (input.WasPressed(Key::Num1)) {
        if (m_planeCount < 1) AddPlane();
        m_selectedPlane = 0;
    }
    if (input.WasPressed(Key::Num2)) {
        if (m_planeCount < 1) AddPlane();
        if (m_planeCount < 2) AddPlane();
        m_selectedPlane = 1;
    }

    if (input.WasPressed(Key::T)) {
        ResetPheasant();
    }

    // [L] Toggle intersection lock
    if (input.WasPressed(Key::L) && m_planeCount == 2 && m_planesIntersect) {
        if (m_intersectionLocked)
            UnlockIntersection();
        else
            LockIntersection();
    }

    // Arrow keys: adjust the selected plane
    const float dt = input.DeltaTime();
    if (m_selectedPlane >= 0 && m_selectedPlane < m_planeCount) {
        if (m_intersectionLocked) {
            if (input.IsDown(Key::Left))  m_lockedAngle[m_selectedPlane] -= kYawSpeed * dt;
            if (input.IsDown(Key::Right)) m_lockedAngle[m_selectedPlane] += kYawSpeed * dt;
            ReflectOverPlane(m_selectedPlane);
        } else {
            auto& rp = m_planes[m_selectedPlane];
            if (input.IsDown(Key::Left))  rp.yaw -= kYawSpeed * dt;
            if (input.IsDown(Key::Right)) rp.yaw += kYawSpeed * dt;
            if (input.IsDown(Key::Up))    rp.distance += kDistanceSpeed * dt;
            if (input.IsDown(Key::Down))  rp.distance -= kDistanceSpeed * dt;
            rp.yaw = std::clamp(rp.yaw, 0.0f, kPi);

            if (input.IsDown(Key::Shift)) {
                if (input.IsDown(Key::Q)) rp.pitch -= kPitchSpeed * dt;
                if (input.IsDown(Key::E)) rp.pitch += kPitchSpeed * dt;
                rp.pitch = std::clamp(rp.pitch, -kPi / 2.0f, kPi / 2.0f);
            }
        }
    }

    // [R] Reflect over the selected plane
    if (input.WasPressed(Key::R) && m_selectedPlane >= 0 && m_selectedPlane < m_planeCount) {
        m_reflected[m_selectedPlane] = true;
        ReflectOverPlane(m_selectedPlane);
    }

    // [Space] Animate the bireflection
    if (input.WasPressed(Key::Space) && m_planeCount == 2 && m_reflected[0] && m_reflected[1] && !m_animating) {
        StartAnimation();
    }
}

void BiReflectionScene::OnGui() {
    if (m_guiVisible) {
        ImGui::Begin("Bireflection Demo", nullptr, ImGuiWindowFlags_NoNavInputs);
        ImGui::Text("FPS: %.1f", GetFPS());
        ImGui::Separator();

        if (m_planeCount < 2) {
            if (ImGui::Button("Add Reflection Plane")) {
                AddPlane();
            }
        }
        ImGui::Text("Planes: %d / 2", m_planeCount);
        ImGui::Separator();

        for (int i = 0; i < m_planeCount; ++i) {
            ImGui::PushID(i);
            auto& rp = m_planes[i];

            bool isSelected = (m_selectedPlane == i);
            const char* label = (i == 0) ? "Plane 1 (yellow)" : "Plane 2 (magenta)";

            if (isSelected) ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.5f, 0.3f, 1.0f));
            if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::IsItemClicked()) m_selectedPlane = i;

                if (m_intersectionLocked) {
                    constexpr float kStep = 5.0f * DEG_TO_RAD;
                    if (ImGui::SliderAngle("Angle", &m_lockedAngle[i], -180.0f, 180.0f, "%.0f deg")) {
                        m_lockedAngle[i] = std::round(m_lockedAngle[i] / kStep) * kStep;
                        ReflectOverPlane(i);
                    }
                } else {
                    constexpr float kStep = 5.0f * DEG_TO_RAD;
                    if (ImGui::SliderAngle("Yaw", &rp.yaw, 0.0f, 180.0f, "%.0f deg")) {
                        rp.yaw = std::round(rp.yaw / kStep) * kStep;
                        ReflectOverPlane(i);
                    }
                    if (ImGui::SliderAngle("Pitch", &rp.pitch, -90.0f, 90.0f, "%.0f deg")) {
                        rp.pitch = std::round(rp.pitch / kStep) * kStep;
                        ReflectOverPlane(i);
                    }
                    if (ImGui::SliderFloat("Distance", &rp.distance, -20.0f, 20.0f)) ReflectOverPlane(i);
                }

                if (ImGui::Button(m_reflected[i] ? "Re-Reflect" : "Reflect")) {
                    m_reflected[i] = true;
                    ReflectOverPlane(i);
                }
                if (m_reflected[i]) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Active");
                }
            }
            if (isSelected) ImGui::PopStyleColor();
            ImGui::PopID();
        }

        if (m_planeCount == 2) {
            ImGui::Separator();
            if (m_intersectionLocked) {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Intersection LOCKED");
                DrawLockFormulas();
                if (ImGui::Button("Unlock [L]")) UnlockIntersection();
            } else if (m_planesIntersect) {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "Planes intersect");
                if (ImGui::Button("Lock Intersection [L]")) LockIntersection();
            } else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "Planes are parallel");
            }

            if (m_reflected[0] && m_reflected[1] && !m_animating) {
                const char* animLabel = m_planesIntersect ? "Animate Rotation" : "Animate Translation";
                if (ImGui::Button(animLabel)) {
                    StartAnimation();
                }
            }
        }

        if (m_animating) {
            ImGui::Separator();
            ImGui::ProgressBar(m_animProgress);
        }

        ImGui::Separator();
        if (ImGui::Button("Reset Pheasant")) {
            ResetPheasant();
        }

        ImGui::Separator();
        ImGui::Text("Keyboard Shortcuts:");
        ImGui::BulletText("[1] / [2]  Add & select plane");
        ImGui::BulletText("Arrows     Yaw / Distance");
        ImGui::BulletText("Shift+Q/E  Pitch");
        ImGui::BulletText("[R]        Reflect");
        ImGui::BulletText("[L]        Lock intersection");
        ImGui::BulletText("[Space]    Animate");
        ImGui::BulletText("[T]        Reset pheasant");
        ImGui::BulletText("[H]        Toggle GUI");
        ImGui::Text("Right-drag: Orbit  |  Scroll: Zoom");
        ImGui::End();
    }
}

// In the lock the planes turn around the locked line, by their angle from the reference plane;
// otherwise the yaw, pitch and distance place them
void BiReflectionScene::UpdatePlane(int index) {
    auto& rp = m_planes[index];
    if (m_intersectionLocked) {
        const Motor turn = Motor::Rotation(m_lockedAngle[index] * RAD_TO_DEG, m_lockedLine);
        SetPlane(rp, ApplyMotor(turn, m_lockedReference));
    } else {
        rp.plane = ApplyMotor(ComputePlaneMotor(rp), Vector(0.0f, 0.0f, 1.0f, 0.0f));
    }
}

// Stores a plane with the yaw, pitch and distance that ComputePlaneMotor turns into it. The yaw only
// covers half a turn, so a normal in the other half is flipped: the same plane and the same reflection.
void BiReflectionScene::SetPlane(ReflectionPlane& rp, const Vector& plane) {
    rp.plane = plane.Normalized();
    if (std::atan2(-rp.plane.e3(), rp.plane.e1()) < 0.0f) {
        rp.plane = rp.plane * -1.0f;
    }
    rp.yaw = std::atan2(-rp.plane.e3(), rp.plane.e1());
    rp.pitch = std::asin(std::clamp(rp.plane.e2(), -1.0f, 1.0f));
    rp.distance = -rp.plane.e0();
}

// Angles around the line are measured from plane 1, the reference a. For plane 2, b, a | b is the cosine
// of its angle, and the meet a ^ b is the line scaled by the sine, so (a ^ b) | line is minus the sine
void BiReflectionScene::LockIntersection() {
    if (!m_planesIntersect || m_planeCount != 2) return;

    m_intersectionLocked = true;
    m_lockedLine = m_intersectionLine.Normalized();
    m_lockedReference = m_planes[0].plane;
    const Vector& other = m_planes[1].plane;
    m_lockedAngle = {0.0f, std::atan2(-((m_lockedReference ^ other) | m_lockedLine), m_lockedReference | other)};
}

// The planes as vectors, and the locked line as their meet, a bivector in Plücker coordinates:
// direction (e23, e31, e12) and moment (e01, e02, e03)
void BiReflectionScene::DrawLockFormulas() const {
    const auto formula = [](const Scene::Color& color, const std::string& text) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(color.r, color.g, color.b, 1.0f));
        ImGui::TextWrapped("%s", text.c_str());
        ImGui::PopStyleColor();
    };
    const Scene::Color lineColor(0.5f, 0.6f, 1.0f);

    for (int i = 0; i < 2; ++i) {
        formula(m_planes[i].color, "P" + std::to_string(i + 1) + " = " + ToRoundedString(m_planes[i].plane));
    }
    formula(lineColor, "L = P2 ^ P1 = " + ToRoundedString(m_intersectionLine));
    formula(lineColor, "L / |L| = " + ToRoundedString(m_lockedLine));
}

// The yaw, pitch and distance are already kept in sync by SetPlane
void BiReflectionScene::UnlockIntersection() {
    m_intersectionLocked = false;
}

void BiReflectionScene::AddPlane() {
    if (m_planeCount >= 2) return;
    const int index = m_planeCount++;
    m_selectedPlane = index;

    UpdatePlane(index);
    const uint32_t id = AddMeshInstance(m_quadMeshIds[index], ComputePlaneMotor(m_planes[index]),
                                        "plane" + std::to_string(index));
    GetInstance(id)->scale = kPlaneScale;
    m_planes[index].meshInstanceId = id;
}

// Moves the xz-plane, where the quad lies, onto the plane: tilt it upright onto x = 0, move it out
// to x = distance, then turn it by the pitch around the z-axis and by the yaw around the y-axis
Motor BiReflectionScene::ComputePlaneMotor(const ReflectionPlane& rp) {
    const BiVector yAxis(0, 0, 0, 0, 1, 0);
    const BiVector zAxis(0, 0, 0, 0, 0, 1);
    return Motor::Rotation(rp.yaw * RAD_TO_DEG, yAxis) * Motor::Rotation(rp.pitch * RAD_TO_DEG, zAxis) *
           TranslationMotor(rp.distance, 0.0f, 0.0f) * Motor::Rotation(-90.0f, zAxis);
}

void BiReflectionScene::UpdatePlaneInstance(int index) {
    const ReflectionPlane& rp = m_planes[index];
    if (rp.meshInstanceId) {
        SetInstanceTransform(*rp.meshInstanceId, ComputePlaneMotor(rp));
    }
}

// The normal, drawn from the point of the plane closest to the origin: the origin's projection
void BiReflectionScene::DrawPlaneNormal(const ReflectionPlane& rp) {
    const TriVector center = ClosestPointOnPlane(rp.plane, TriVector(0.0f, 0.0f, 0.0f));
    DrawDebugLine(center, center + PlaneNormal(rp.plane) * 4.0f, rp.color);
    DrawDebugPoint(center, 0.5f, rp.color);
}

// Plane 1 reflects the pheasant, plane 2 the pheasant reflected in plane 1 when there is one.
// A reflection makes the instance odd (a mirror plane); the second turns it back into a motor.
void BiReflectionScene::ReflectOverPlane(int planeIndex) {
    if (!m_reflected[planeIndex]) return;

    UpdatePlane(planeIndex);
    const MeshInstance* source = GetInstance(m_pheasantInstanceId);
    if (planeIndex == 1 && m_reflected[0] && m_reflectedInstances[0]) {
        source = GetInstance(*m_reflectedInstances[0]);
    }
    MeshInstance reflected = source->Reflected(m_planes[planeIndex].plane);
    reflected.name = "reflected" + std::to_string(planeIndex);
    reflected.visible = true;

    if (!m_reflectedInstances[planeIndex]) {
        m_reflectedInstances[planeIndex] = AddMeshInstance(m_pheasantMeshId, reflected.transform, reflected.name);
    }
    *GetInstance(*m_reflectedInstances[planeIndex]) = reflected;
}

void BiReflectionScene::ComputeIntersection() {
    m_intersectionLine = m_planes[1].plane ^ m_planes[0].plane;
    m_planesIntersect = m_intersectionLine.Norm() > 1e-4f;
}

void BiReflectionScene::StartAnimation() {
    auto* pheasant = GetInstance(m_pheasantInstanceId);
    if (!pheasant) return;

    m_originalTransform = pheasant->transform;
    m_animMotor = ReflectionMotor(m_planes[0].plane, m_planes[1].plane);
    m_animProgress = 0.0f;
    m_animating = true;
}

void BiReflectionScene::ResetPheasant() {
    m_animating = false;
    m_animProgress = 0.0f;
    m_reflected = {false, false};

    if (auto* pheasant = GetInstance(m_pheasantInstanceId)) {
        pheasant->transform = Motor{1, 0, 0, 0, 0, 0, 0, 0};
    }
    for (const auto& id : m_reflectedInstances) {
        if (id) {
            SetInstanceVisible(*id, false);
        }
    }
}
