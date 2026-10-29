#include "GameScene.h"
#include "PGA.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <utility>

namespace {

// Position of an id in an ascending list of ids, which is also the position of its object
std::optional<size_t> indexOf(const std::vector<uint32_t>& ids, uint32_t id) {
    const auto it = std::ranges::lower_bound(ids, id);
    if (it == ids.end() || *it != id) return std::nullopt;
    return static_cast<size_t>(it - ids.begin());
}

}  // namespace

TriVector MeshInstance::ToWorld(const TriVector& localPoint) const {
    const TriVector p = Euclidean(localPoint);
    const TriVector scaled(p.e032() * scale, p.e013() * scale, p.e021() * scale, p.e123());
    const TriVector moved = ApplyMotor(transform, scaled);
    return mirror ? ReflectInPlane(*mirror, moved) : moved;
}

MeshInstance MeshInstance::Reflected(const Vector& plane) const {
    MeshInstance result = *this;
    if (mirror) {
        // Reflecting in the mirror and then in the plane is the motor plane * mirror
        result.transform = ReflectionMotor(*mirror, plane) * transform;
        result.mirror.reset();
    } else {
        result.mirror = plane;
    }
    return result;
}

GameScene::GameScene(std::string resourceDir)
    : m_resourceDir(std::move(resourceDir)) {}

// === Engine interface ===

void GameScene::Init() {
    OnInit();
}

void GameScene::HandleInput(const Input& input) {
    OnInput(input);
}

void GameScene::Update(float deltaTime) {
    updateFPS(deltaTime);
    m_debugLines.clear();
    OnUpdate(deltaTime);
}

void GameScene::DrawGui() {
    OnGui();
}

void GameScene::Shutdown() {
    OnShutdown();
}

void GameScene::updateFPS(float deltaTime) noexcept {
    m_fpsAccumulator += deltaTime;
    ++m_fpsFrameCount;
    if (m_fpsAccumulator >= 0.5f) {
        m_fps = static_cast<float>(m_fpsFrameCount) / m_fpsAccumulator;
        m_fpsAccumulator = 0.0f;
        m_fpsFrameCount = 0;
    }
}

void GameScene::SetCamera(const TriVector& eye, const TriVector& target, const TriVector& up) noexcept {
    m_camera.eye = eye;
    m_camera.target = target;
    m_camera.up = up;
}

// === Meshes ===

uint32_t GameScene::LoadMesh(std::string_view objFilename, std::string_view textureFilename) {
    auto mesh = std::make_unique<Mesh>();
    const std::string fullPath = m_resourceDir + "/" + std::string(objFilename);
    if (!mesh->LoadFromFile(fullPath)) {
        throw std::runtime_error("Failed to load mesh: " + fullPath);
    }

    mesh->StandOnOrigin();

    if (!textureFilename.empty()) {
        if (!mesh->HasUVCoordinates()) {
            mesh->GenerateCylindricalUVs();
        }
        const std::string fullTexturePath = m_resourceDir + "/" + std::string(textureFilename);
        for (size_t i = 0; i < mesh->MaterialCount(); ++i) {
            mesh->GetMaterial(i).texturePath = fullTexturePath;
        }
    }

    return AddMesh(std::move(mesh));
}

uint32_t GameScene::AddMesh(std::unique_ptr<Mesh> mesh) {
    mesh->BuildBVH();
    const auto meshId = static_cast<uint32_t>(m_meshes.size());
    m_meshes.push_back(std::move(mesh));
    return meshId;
}

Mesh* GameScene::GetMesh(uint32_t id) noexcept {
    return id < m_meshes.size() ? m_meshes[id].get() : nullptr;
}

uint32_t GameScene::AddMeshInstance(uint32_t meshId, const TriVector& position, std::string_view name) {
    return AddMeshInstance(meshId, TranslationMotor(Euclidean(position)), name);
}

uint32_t GameScene::AddMeshInstance(uint32_t meshId, const Motor& transform, std::string_view name) {
    if (meshId >= m_meshes.size()) {
        throw std::runtime_error("Invalid mesh ID: " + std::to_string(meshId));
    }

    const uint32_t instanceId = m_nextInstanceId++;
    m_meshInstances.push_back({.meshId = meshId, .transform = transform, .name = std::string(name)});
    m_instanceIds.push_back(instanceId);
    if (!name.empty()) {
        m_namedInstances[std::string(name)] = instanceId;
    }
    return instanceId;
}

MeshInstance* GameScene::GetInstance(uint32_t instanceId) noexcept {
    const auto index = indexOf(m_instanceIds, instanceId);
    return index ? &m_meshInstances[*index] : nullptr;
}

MeshInstance* GameScene::FindInstance(std::string_view name) noexcept {
    const auto it = m_namedInstances.find(std::string(name));
    return it != m_namedInstances.end() ? GetInstance(it->second) : nullptr;
}

void GameScene::SetInstanceTransform(uint32_t instanceId, const Motor& transform) {
    if (auto* instance = GetInstance(instanceId)) {
        instance->transform = transform;
    }
}

void GameScene::SetInstancePosition(uint32_t instanceId, const TriVector& position) {
    SetInstanceTransform(instanceId, TranslationMotor(Euclidean(position)));
}

void GameScene::SetInstanceVisible(uint32_t instanceId, bool visible) {
    if (auto* instance = GetInstance(instanceId)) {
        instance->visible = visible;
    }
}

bool GameScene::RemoveMeshInstance(uint32_t instanceId) {
    const auto index = indexOf(m_instanceIds, instanceId);
    if (!index) return false;

    m_meshInstances.erase(m_meshInstances.begin() + *index);
    m_instanceIds.erase(m_instanceIds.begin() + *index);
    std::erase_if(m_namedInstances, [instanceId](const auto& entry) { return entry.second == instanceId; });
    return true;
}

// === Primitives ===

uint32_t GameScene::AddSphere(const TriVector& center, float radius, const Scene::Material& material) {
    m_sceneData.spheres.push_back({Euclidean(center), radius, material});
    return static_cast<uint32_t>(m_sceneData.spheres.size() - 1);
}

uint32_t GameScene::AddPlane(const Vector& plane, const Scene::Material& material) {
    m_sceneData.planes.push_back({plane, material});
    return static_cast<uint32_t>(m_sceneData.planes.size() - 1);
}

uint32_t GameScene::AddGroundPlane(float height, const Scene::Material& material) {
    // y - height = 0, facing up
    return AddPlane(Vector(-height, 0.0f, 1.0f, 0.0f), material);
}

Scene::Sphere* GameScene::GetSphere(uint32_t id) noexcept {
    return id < m_sceneData.spheres.size() ? &m_sceneData.spheres[id] : nullptr;
}

Scene::Plane* GameScene::GetPlane(uint32_t id) noexcept {
    return id < m_sceneData.planes.size() ? &m_sceneData.planes[id] : nullptr;
}

// === Lights ===

uint32_t GameScene::AddDirectionalLight(const TriVector& direction, const Scene::Color& color, float intensity) {
    return addLight(Scene::Light::Directional(direction, color, intensity));
}

uint32_t GameScene::AddPointLight(const TriVector& position, const Scene::Color& color,
                                  float intensity, float range) {
    return addLight(Scene::Light::Point(Euclidean(position), color, intensity, range));
}

uint32_t GameScene::AddSpotLight(const TriVector& position, const TriVector& direction,
                                 const Scene::Color& color, float intensity, float angle, float range) {
    return addLight(Scene::Light::Spot(Euclidean(position), direction, color, intensity, range, angle));
}

uint32_t GameScene::addLight(const Scene::Light& light) {
    m_sceneData.lights.push_back(light);
    m_lightIds.push_back(m_nextLightId);
    return m_nextLightId++;
}

Scene::Light* GameScene::GetLight(uint32_t id) noexcept {
    const auto index = indexOf(m_lightIds, id);
    return index ? &m_sceneData.lights[*index] : nullptr;
}

bool GameScene::RemoveLight(uint32_t id) {
    const auto index = indexOf(m_lightIds, id);
    if (!index) return false;

    m_sceneData.lights.erase(m_sceneData.lights.begin() + *index);
    m_lightIds.erase(m_lightIds.begin() + *index);
    return true;
}

// === Debug drawing ===

void GameScene::DrawDebugLine(const TriVector& from, const TriVector& to, const Scene::Color& color) {
    if (m_debugDrawEnabled) {
        m_debugLines.push_back({Euclidean(from), Euclidean(to), color});
    }
}

void GameScene::DrawDebugLine(const BiVector& line, float halfLength, const Scene::Color& color) {
    // The line's direction is its ideal point, drawn to both sides of its point closest to the origin
    const float length = LineDirection(line).VNorm();
    if (length < 1e-6f) return;  // Ideal line, nothing to draw
    const TriVector direction = LineDirection(line) * (halfLength / length);

    const TriVector closest = ClosestPointOnLine(line, TriVector(0.0f, 0.0f, 0.0f));
    DrawDebugLine(closest - direction, closest + direction, color);
}

void GameScene::DrawDebugPoint(const TriVector& point, float size, const Scene::Color& color) {
    if (std::abs(point.e123()) < 1e-6f) return;  // Directions have no position

    const TriVector p = Euclidean(point);
    for (const TriVector& axis : {TriVector(size, 0.0f, 0.0f, 0.0f), TriVector(0.0f, size, 0.0f, 0.0f),
                                  TriVector(0.0f, 0.0f, size, 0.0f)}) {
        DrawDebugLine(p - axis, p + axis, color);
    }
}

void GameScene::DrawDebugAxes(const TriVector& position, float size) {
    DrawDebugAxes(TranslationMotor(Euclidean(position)), size);
}

void GameScene::DrawDebugAxes(const Motor& transform, float size) {
    const TriVector origin = ApplyMotor(transform, TriVector(0.0f, 0.0f, 0.0f));
    DrawDebugLine(origin, ApplyMotor(transform, TriVector(size, 0.0f, 0.0f)), Scene::Color::Red());
    DrawDebugLine(origin, ApplyMotor(transform, TriVector(0.0f, size, 0.0f)), Scene::Color::Green());
    DrawDebugLine(origin, ApplyMotor(transform, TriVector(0.0f, 0.0f, size)), Scene::Color::Blue());
}
