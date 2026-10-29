#include "HornetScene.h"
#include "PGA.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>

namespace {

const TriVector kLaser(0.0f, 0.0f, 0.0f);
const TriVector kFirstPivot(3.0f, 0.0f, 0.0f);   // On the laser line, the +x axis
const TriVector kSecondPivot(3.0f, 2.5f, 0.0f);
const std::array kPivots{kFirstPivot, kSecondPivot};
const BiVector kYAxis(0, 0, 0, 0, 1, 0);
constexpr float kGroundHeight = -1.0f;

const Scene::Color kAimColor(0.55f, 0.05f, 0.05f);
const Scene::Color kShotColor(1.0f, 0.15f, 0.1f);
const Scene::Color kFlashColor(1.0f, 0.9f, 0.3f);
const Scene::Color kPostColor(0.3f, 0.3f, 0.3f);

} // anonymous namespace

HornetScene::HornetScene(const std::string& resourceDir)
    : GameScene(resourceDir) {}

void HornetScene::OnInit() {
    AddGroundPlane(kGroundHeight, Scene::Material::Lambert(Scene::Color(0.55f, 0.7f, 0.45f)));
    AddSphere(kLaser, 0.35f, Scene::Material::Metal(Scene::Color(0.25f, 0.25f, 0.28f), 0.3f));

    const uint32_t mirrorMeshId = LoadMesh("quad.obj");
    GetMesh(mirrorMeshId)->GetMaterial(0) = Scene::Material::Phong(Scene::Color(0.9f, 0.92f, 0.95f), 128.0f, 0.85f);
    for (size_t i = 0; i < kPivots.size(); ++i) {
        m_mirrorInstanceIds[i] = AddMeshInstance(mirrorMeshId, kPivots[i]);
        GetInstance(m_mirrorInstanceIds[i])->scale = kMirrorScale;
    }

    const uint32_t pheasantMeshId = LoadMesh("pheasant.obj", "pheasant.png");
    const AABB bounds = GetMesh(pheasantMeshId)->Bounds();
    m_pheasantCenter = TriVector(kPheasantScale * bounds.Centroid(0), kPheasantScale * bounds.Centroid(1),
                                 kPheasantScale * bounds.Centroid(2));
    m_pheasantInstanceId = AddMeshInstance(pheasantMeshId, PheasantTransform(), "pheasant");
    GetInstance(m_pheasantInstanceId)->scale = kPheasantScale;

    AddDirectionalLight(TriVector(-0.4f, -1.0f, -0.3f, 0.0f), Scene::Color(1.0f, 1.0f, 0.95f), 0.8f);
    AddPointLight(TriVector(0.0f, 15.0f, 5.0f), Scene::Color::White(), 1.5f, 60.0f);
    SetCamera(m_orbit.Eye(), m_orbit.target);
}

// A loop around the laser that widens and narrows, rising and falling
TriVector HornetScene::FlightPosition(float time) {
    const float angle = kFlightSpeed * time;
    const float radius = 12.0f + 3.0f * std::sin(1.7f * angle);
    const float height = 7.0f + 1.5f * std::sin(2.3f * angle);
    return ApplyMotor(Motor::Rotation(angle * RAD_TO_DEG, kYAxis), TriVector(radius, height, 0.0f));
}

// Flying level along the path, the model's forward +z turned to the flight direction; once hit,
// falling from where it was hit while tumbling around its middle
Motor HornetScene::PheasantTransform() const {
    if (m_fallTime) {
        const float t = *m_fallTime;
        const Motor drop = TranslationMotor(0.0f, -4.9f * t * t, 0.0f);
        const BiVector tumbleAxis = m_pheasantCenter & TriVector(1.0f, 0.0f, 0.0f, 0.0f);
        return drop * m_hitTransform * Motor::Rotation(540.0f * t, tumbleAxis);
    }
    const TriVector position = FlightPosition(m_flightTime);
    const TriVector velocity = FlightPosition(m_flightTime + 0.05f) - position;
    const TriVector heading(velocity.e032(), 0.0f, velocity.e021(), 0.0f);
    return TranslationMotor(position) * RotationBetween(TriVector(0.0f, 0.0f, 1.0f, 0.0f), heading);
}

// The middle of the model, where the laser aims
TriVector HornetScene::PheasantCenter() const {
    return ApplyMotor(GetMeshInstances()[m_pheasantInstanceId].transform, m_pheasantCenter);
}

void HornetScene::OnInput(const Input& input) {
    m_orbit.HandleInput(input);
    SetCamera(m_orbit.Eye(), m_orbit.target);

    if (input.WasPressed(Key::H)) m_guiVisible = !m_guiVisible;
    if (input.WasPressed(Key::P)) m_paused = !m_paused;

    // The beam travels at the speed of light: a shot hits wherever the mirrors aim
    if (input.WasPressed(Key::Space) && m_solution && !m_fallTime) {
        m_shotTimer = kShotDuration;
        ++m_hits;
        m_fallTime = 0.0f;
        m_hitTransform = GetInstance(m_pheasantInstanceId)->transform;
    }
}

void HornetScene::OnUpdate(float deltaTime) {
    if (!m_paused) {
        m_flightTime += deltaTime;
    }
    m_shotTimer = std::max(m_shotTimer - deltaTime, 0.0f);
    if (m_fallTime) {
        *m_fallTime += deltaTime;
        if (*m_fallTime > kFallDuration) {
            m_fallTime.reset();
        }
    }
    GetInstance(m_pheasantInstanceId)->transform = PheasantTransform();

    m_target = PheasantCenter();
    m_solution = SolveMirrors(kLaser, kFirstPivot, kSecondPivot, m_target);
    UpdateMirrors();
    DrawBeam();
}

// The quad lies in the xz-plane: turn its normal +y onto the mirror's normal, at the pivot
void HornetScene::UpdateMirrors() {
    if (!m_solution) return;
    const std::array mirrors{m_solution->firstMirror, m_solution->secondMirror};
    for (size_t i = 0; i < mirrors.size(); ++i) {
        SetInstanceTransform(m_mirrorInstanceIds[i], TranslationMotor(kPivots[i]) *
                             RotationBetween(TriVector(0.0f, 1.0f, 0.0f, 0.0f), PlaneNormal(mirrors[i])));
    }
}

void HornetScene::DrawBeam() {
    for (const TriVector& pivot : {kLaser, kFirstPivot, kSecondPivot}) {
        DrawDebugLine(TriVector(pivot.e032(), kGroundHeight, pivot.e021()), pivot, kPostColor);
    }
    if (!m_solution) return;

    // Where the beam leaving the second mirror passes the target: the target itself for an exact solution
    const std::array points{kLaser, m_solution->firstHit, m_solution->secondHit,
                            ClosestPointOnLine(m_solution->beam, m_target)};
    const bool firing = m_shotTimer > 0.0f;
    // A fired beam is drawn three times, slightly apart, to look thicker
    const std::array offsets{TriVector(0.0f, 0.0f, 0.0f, 0.0f), TriVector(0.0f, 0.03f, 0.0f, 0.0f),
                             TriVector(0.0f, 0.0f, 0.03f, 0.0f)};
    for (size_t copy = 0; copy < (firing ? offsets.size() : 1); ++copy) {
        for (size_t i = 0; i + 1 < points.size(); ++i) {
            DrawDebugLine(points[i] + offsets[copy], points[i + 1] + offsets[copy], firing ? kShotColor : kAimColor);
        }
    }

    if (firing) {
        const TriVector hit = points.back();
        for (const TriVector& ray : {TriVector(1.0f, 0.0f, 0.0f, 0.0f), TriVector(0.0f, 1.0f, 0.0f, 0.0f),
                                     TriVector(0.0f, 0.0f, 1.0f, 0.0f), TriVector(0.7f, 0.7f, 0.0f, 0.0f),
                                     TriVector(0.0f, 0.7f, 0.7f, 0.0f), TriVector(0.7f, 0.0f, -0.7f, 0.0f)}) {
            DrawDebugLine(hit - ray * 1.5f, hit + ray * 1.5f, kFlashColor);
        }
    }
}

void HornetScene::OnGui() {
    if (!m_guiVisible) return;

    ImGui::Begin("HornetTracer", nullptr, ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("FPS: %.1f", GetFPS());
    ImGui::Text("Hits: %d", m_hits);
    if (m_fallTime) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Hit! The pheasant is going down");
    } else if (m_solution) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Tracking: press Space to fire");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Out of reach");
    }

    if (m_solution) {
        ImGui::Separator();
        ImGui::Text("PGA mirror solver");
        ImGui::Text("Mirror 1: M1 = %s", ToRoundedString(m_solution->firstMirror).c_str());
        ImGui::Text("Mirror 2: M2 = %s", ToRoundedString(m_solution->secondMirror).c_str());
        ImGui::Text("Tandem motor: T = M2 M1");
        ImGui::Text("  = %s", ToRoundedString(m_solution->tandem).c_str());
        ImGui::Text("Beam: the laser line L moved by T, T L ~T");
        ImGui::Text("  = %s", ToRoundedString(m_solution->beam).c_str());
        const TriVector miss = m_target - ClosestPointOnLine(m_solution->beam, m_target);
        ImGui::Text("Distance from beam to target: %.5f", miss.VNorm());
    }

    ImGui::Separator();
    ImGui::Text("[Space] Fire  |  [P] Pause flight  |  [H] Toggle GUI");
    ImGui::Text("Right-drag: Orbit  |  Scroll: Zoom");
    ImGui::End();
}
