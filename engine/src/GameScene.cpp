#include "GameScene.h"
#include "VulkanRenderer.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>

GameScene::GameScene(const std::string& resourceDir)
    : m_resourceDir(resourceDir) {}

void GameScene::onInit([[maybe_unused]] VulkanRenderer* renderer) {}
void GameScene::onUpdate([[maybe_unused]] float deltaTime) {}
void GameScene::onInput([[maybe_unused]] const InputState& input) {}
void GameScene::onGui() {}
void GameScene::onShutdown() {}

void GameScene::updateFPS(float deltaTime) noexcept {
    m_fpsAccumulator += deltaTime;
    ++m_fpsFrameCount;

    if (m_fpsAccumulator >= 1.0f) {
        m_fps = static_cast<float>(m_fpsFrameCount) / m_fpsAccumulator;
        m_fpsAccumulator = 0.0f;
        m_fpsFrameCount = 0;
    }
}

void GameScene::getCameraMotor(float out[8]) const noexcept {
    const Motor identity(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    out[0] = identity.s();
    out[1] = identity.e01();
    out[2] = identity.e02();
    out[3] = identity.e03();
    out[4] = identity.e23();
    out[5] = identity.e31();
    out[6] = identity.e12();
    out[7] = identity.e0123();
}

// === Mesh Management ===

uint32_t GameScene::loadMesh(std::string_view objFilename, std::string_view textureFilename) {
    auto mesh = std::make_unique<Mesh>();
    std::string fullPath = m_resourceDir + "/" + std::string(objFilename);

    if (!mesh->loadFromFile(fullPath)) {
        throw std::runtime_error("Failed to load mesh: " + fullPath);
    }

    mesh->centerOnOrigin();
    mesh->buildBVH();

    if (!textureFilename.empty() && m_textureFilename.empty()) {
        m_textureFilename = m_resourceDir + "/" + std::string(textureFilename);
    }

    if (!textureFilename.empty()) {
        std::string fullTexturePath = m_resourceDir + "/" + std::string(textureFilename);
        for (size_t i = 0; i < mesh->materialCount(); ++i) {
            mesh->getMaterial(i).diffuseTexturePath = fullTexturePath;
            mesh->getMaterial(i).diffuseTextureIndex = 0;
        }
    }

    const auto meshId = static_cast<uint32_t>(m_meshes.size());
    m_meshes.push_back(std::move(mesh));
    m_meshCPUDataFreed.push_back(false);
    return meshId;
}

void GameScene::freeMeshCPUData(uint32_t meshId) {
    if (meshId >= m_meshes.size() || !m_meshes[meshId]) {
        return;
    }

    if (!m_meshCPUDataFreed[meshId]) {
        m_meshes[meshId]->clearCPUData();
        m_meshCPUDataFreed[meshId] = true;
    }
}

void GameScene::freeAllMeshCPUData() {
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_meshes.size()); ++i) {
        freeMeshCPUData(i);
    }
}

bool GameScene::isMeshCPUDataFreed(uint32_t meshId) const noexcept {
    return meshId < m_meshCPUDataFreed.size() && m_meshCPUDataFreed[meshId];
}

// === Mesh Instance Management ===

uint32_t GameScene::addMeshInstance(uint32_t meshId, const TriVector& position, std::string_view name) {
    Motor transform(
        1.0f,
        position.e032() * 0.5f,
        position.e013() * 0.5f,
        position.e021() * 0.5f,
        0.0f, 0.0f, 0.0f, 0.0f
    );
    return addMeshInstance(meshId, transform, name);
}

uint32_t GameScene::addMeshInstance(uint32_t meshId, const Motor& transform, std::string_view name) {
    if (meshId >= m_meshes.size()) {
        throw std::runtime_error("Invalid mesh ID: " + std::to_string(meshId));
    }

    MeshInstance instance;
    instance.meshId = meshId;
    instance.transform = transform;
    instance.visible = true;
    instance.name = std::string(name);

    const auto instanceId = static_cast<uint32_t>(m_meshInstances.size());
    m_meshInstances.push_back(std::move(instance));

    if (!name.empty()) {
        m_namedInstances[std::string(name)] = instanceId;
    }

    return instanceId;
}

MeshInstance* GameScene::getInstance(uint32_t instanceId) noexcept {
    return instanceId < m_meshInstances.size() ? &m_meshInstances[instanceId] : nullptr;
}

MeshInstance* GameScene::findInstance(std::string_view name) noexcept {
    const auto it = m_namedInstances.find(std::string(name));
    return it != m_namedInstances.end() ? &m_meshInstances[it->second] : nullptr;
}

void GameScene::setInstanceTransform(uint32_t instanceId, const Motor& transform) {
    if (auto* instance = getInstance(instanceId)) {
        instance->transform = transform;
    }
}

void GameScene::setInstancePosition(uint32_t instanceId, const TriVector& position) {
    if (auto* instance = getInstance(instanceId)) {
        instance->transform = Motor(
            1.0f,
            position.e032() * 0.5f,
            position.e013() * 0.5f,
            position.e021() * 0.5f,
            0.0f, 0.0f, 0.0f, 0.0f
        );
    }
}

void GameScene::setInstanceVisible(uint32_t instanceId, bool visible) {
    if (auto* instance = getInstance(instanceId)) {
        instance->visible = visible;
    }
}

// === Primitive Management ===

uint32_t GameScene::addSphere(const TriVector& center, float radius, const Scene::Material& material) {
    return addSphere(center.e032(), center.e013(), center.e021(), radius, material);
}

uint32_t GameScene::addPlane(const Vector& plane, const Scene::Material& material) {
    return addPlane(plane.e1(), plane.e2(), plane.e3(), -plane.e0(), material);
}

uint32_t GameScene::addSphere(float x, float y, float z, float radius, const Scene::Material& material) {
    auto sphere = Scene::GPUSphere::create(x, y, z, radius, material);
    const auto id = static_cast<uint32_t>(m_sceneData.spheres.size());
    m_sceneData.spheres.push_back(sphere);
    return id;
}

uint32_t GameScene::addPlane(float nx, float ny, float nz, float distance, const Scene::Material& material) {
    auto plane = Scene::GPUPlane::create(nx, ny, nz, distance, material);
    const auto id = static_cast<uint32_t>(m_sceneData.planes.size());
    m_sceneData.planes.push_back(plane);
    return id;
}

uint32_t GameScene::addGroundPlane(float height, const Scene::Material& material) {
    return addPlane(0.0f, 1.0f, 0.0f, height, material);
}

Scene::GPUSphere* GameScene::getSphere(uint32_t id) noexcept {
    return id < m_sceneData.spheres.size() ? &m_sceneData.spheres[id] : nullptr;
}

Scene::GPUPlane* GameScene::getPlane(uint32_t id) noexcept {
    return id < m_sceneData.planes.size() ? &m_sceneData.planes[id] : nullptr;
}

void GameScene::setSphereMaterial(uint32_t id, const Scene::Material& material) {
    if (auto* sphere = getSphere(id)) {
        sphere->setMaterial(material);
    }
}

void GameScene::setPlaneMaterial(uint32_t id, const Scene::Material& material) {
    if (auto* plane = getPlane(id)) {
        plane->setMaterial(material);
    }
}

// === Lighting ===

uint32_t GameScene::addDirectionalLight(const TriVector& direction,
                                         const Scene::Color& color,
                                         float intensity) {
    return addDirectionalLight(direction.e032(), direction.e013(), direction.e021(), color, intensity);
}

uint32_t GameScene::addPointLight(const TriVector& position,
                                   const Scene::Color& color,
                                   float intensity,
                                   float range) {
    return addPointLight(position.e032(), position.e013(), position.e021(), color, intensity, range);
}

uint32_t GameScene::addSpotLight(const TriVector& position,
                                  const TriVector& direction,
                                  const Scene::Color& color,
                                  float intensity,
                                  float angle,
                                  float range) {
    return addSpotLight(position.e032(), position.e013(), position.e021(),
                        direction.e032(), direction.e013(), direction.e021(),
                        color, intensity, angle, range);
}

uint32_t GameScene::addDirectionalLight(float dx, float dy, float dz,
                                         const Scene::Color& color,
                                         float intensity) {
    m_sceneData.lights.push_back(Scene::GPULight::Directional(dx, dy, dz, color, intensity));
    return static_cast<uint32_t>(m_sceneData.lights.size() - 1);
}

uint32_t GameScene::addPointLight(float x, float y, float z,
                                   const Scene::Color& color,
                                   float intensity,
                                   float range) {
    m_sceneData.lights.push_back(Scene::GPULight::Point(x, y, z, color, intensity, range));
    return static_cast<uint32_t>(m_sceneData.lights.size() - 1);
}

uint32_t GameScene::addSpotLight(float px, float py, float pz,
                                  float dx, float dy, float dz,
                                  const Scene::Color& color,
                                  float intensity,
                                  float angle,
                                  float range) {
    m_sceneData.lights.push_back(Scene::GPULight::Spot(px, py, pz, dx, dy, dz, color, intensity, range, angle, 0.1f));
    return static_cast<uint32_t>(m_sceneData.lights.size() - 1);
}

Scene::GPULight* GameScene::getLight(uint32_t id) noexcept {
    return id < m_sceneData.lights.size() ? &m_sceneData.lights[id] : nullptr;
}
