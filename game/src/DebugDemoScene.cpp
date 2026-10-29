#include "DebugDemoScene.h"
#include "PGA.h"
#include <cmath>
#include <imgui.h>

DebugDemoScene::DebugDemoScene(const std::string& resourceDir)
    : GameScene(resourceDir) {}

void DebugDemoScene::OnInit() {
    AddGroundPlane(0.0f, Scene::Material::Lambert(Scene::Color(0.3f, 0.4f, 0.3f)));

    // Central sphere that will be animated
    m_sphereId = AddSphere(m_sphereStartPosition, 1.5f,
                           Scene::Material::Metal(Scene::Color(0.8f, 0.3f, 0.3f), 0.2f));

    // Some static spheres for reference
    AddSphere(TriVector(5.0f, 1.0f, 0.0f), 1.0f, Scene::Material::Glossy(Scene::Color(0.3f, 0.3f, 0.8f), 0.1f));
    AddSphere(TriVector(-5.0f, 1.0f, 0.0f), 1.0f, Scene::Material::Glossy(Scene::Color(0.3f, 0.8f, 0.3f), 0.1f));
    AddSphere(TriVector(0.0f, 1.0f, 5.0f), 1.0f, Scene::Material::Glossy(Scene::Color(0.8f, 0.8f, 0.3f), 0.1f));

    AddPointLight(TriVector(5.0f, 10.0f, 5.0f), Scene::Color(1.0f, 1.0f, 1.0f), 1.5f, 50.0f);
}

void DebugDemoScene::OnUpdate(float deltaTime) {
    if (!m_paused) {
        m_time += deltaTime * m_rotationSpeed;
    }

    // Animate the main sphere in a circle around the vertical line through its start, bobbing up and down
    if (auto* sphere = GetSphere(m_sphereId)) {
        const float radius = 3.0f;
        const BiVector axis = m_sphereStartPosition & TriVector(0.0f, 1.0f, 0.0f, 0.0f);
        const Motor motor = TranslationMotor(0.0f, 0.5f * std::sin(3.0f * m_time), 0.0f) *
                            Motor::Rotation(m_time * RAD_TO_DEG, axis);
        sphere->center = ApplyMotor(motor, m_sphereStartPosition + TriVector(radius, 0.0f, 0.0f, 0.0f));
    }

    // === Debug Visualization ===

    if (m_showAxes) {
        DrawDebugAxes(TriVector(0.0f, 0.0f, 0.0f), 5.0f);
        if (auto* sphere = GetSphere(m_sphereId)) {
            DrawDebugAxes(sphere->center, 1.0f);
        }
    }

    // A grid on the ground plane
    if (m_showGrid) {
        const float gridSize = 10.0f;
        const float gridStep = 2.0f;
        const Scene::Color gridColor(0.5f, 0.5f, 0.5f);
        for (float x = -gridSize; x <= gridSize; x += gridStep) {
            DrawDebugLine(TriVector(x, 0.01f, -gridSize), TriVector(x, 0.01f, gridSize), gridColor);
        }
        for (float z = -gridSize; z <= gridSize; z += gridStep) {
            DrawDebugLine(TriVector(-gridSize, 0.01f, z), TriVector(gridSize, 0.01f, z), gridColor);
        }
    }

    // Points at each static sphere
    DrawDebugPoint(TriVector(5.0f, 1.0f, 0.0f, 1.0f), 0.3f, Scene::Color::Cyan());
    DrawDebugPoint(TriVector(-5.0f, 1.0f, 0.0f, 1.0f), 0.3f, Scene::Color::Magenta());
    DrawDebugPoint(TriVector(0.0f, 1.0f, 5.0f, 1.0f), 0.3f, Scene::Color::Yellow());

    // BiVector lines (Plücker lines)
    if (m_showLines) {
        // A line as the regressive product (join) of two points: p1 & p2
        TriVector p1(0.0f, 2.0f, 0.0f, 1.0f);
        TriVector p2(5.0f, 4.0f, 5.0f, 1.0f);
        BiVector line1 = p1 & p2;
        DrawDebugLine(line1, 8.0f, Scene::Color::Yellow());

        // Animated rotating line
        const float angle = m_time;
        TriVector pA(0.0f, 5.0f, 0.0f, 1.0f);
        TriVector pB(3.0f, 5.0f, 3.0f, 1.0f);
        BiVector line2 = pA & pB;

        const Motor rotation = Motor::Rotation(angle * RAD_TO_DEG, BiVector(0, 0, 0, 0, 1, 0));
        BiVector rotatedLine = (rotation * line2 * ~rotation).Grade2();
        DrawDebugLine(rotatedLine, 6.0f, Scene::Color::Cyan());

        // Line connecting two static spheres
        TriVector sphereA(5.0f, 1.0f, 0.0f, 1.0f);
        TriVector sphereB(-5.0f, 1.0f, 0.0f, 1.0f);
        BiVector connectingLine = sphereA & sphereB;
        DrawDebugLine(connectingLine, 7.0f, Scene::Color::Magenta());
    }
}

void DebugDemoScene::OnInput(const Input& input) {
    m_orbit.HandleInput(input);
    SetCamera(m_orbit.Eye(), m_orbit.target);

    if (input.WasPressed(Key::P)) m_paused = !m_paused;
    if (input.WasPressed(Key::Num1)) m_showAxes = !m_showAxes;
    if (input.WasPressed(Key::Num2)) m_showGrid = !m_showGrid;
    if (input.WasPressed(Key::Num3)) m_showLines = !m_showLines;
    if (input.WasPressed(Key::R)) Reset();
}

void DebugDemoScene::OnGui() {
    ImGui::Begin("Debug Visualization Demo");
    ImGui::Text("FPS: %.1f", GetFPS());

    ImGui::Separator();
    ImGui::Text("Animation");
    ImGui::Checkbox("Paused (P)", &m_paused);
    ImGui::SliderFloat("Speed", &m_rotationSpeed, 0.0f, 3.0f);
    if (ImGui::Button("Reset (R)")) {
        Reset();
    }

    ImGui::Separator();
    ImGui::Text("Debug Visualization");
    ImGui::Checkbox("Show Axes (1)", &m_showAxes);
    ImGui::Checkbox("Show Grid (2)", &m_showGrid);
    ImGui::Checkbox("Show BiVector Lines (3)", &m_showLines);

    ImGui::Separator();
    ImGui::Text("Controls");
    ImGui::Text("  Right-drag: Orbit camera");
    ImGui::Text("  Scroll: Zoom");
    ImGui::Text("  P: Pause animation");
    ImGui::Text("  1-3: Toggle visualizations");
    ImGui::Text("  R: Reset time");
    ImGui::End();
}

void DebugDemoScene::Reset() {
    m_time = 0.0f;
    if (auto* sphere = GetSphere(m_sphereId)) {
        sphere->center = m_sphereStartPosition;
    }
}
