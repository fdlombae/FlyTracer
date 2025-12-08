#pragma once

#include "Scene.h"
#include "FlyFish.h"
#include "Mesh.h"
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <cstdint>

class VulkanRenderer;

struct MeshInstance {
    uint32_t meshId;
    Motor transform;
    float scale{1.0f};
    bool visible{true};
    std::string name;
};

// Input state passed to onInput
struct InputState {
    bool leftMouseDown{false};
    bool rightMouseDown{false};
    bool middleMouseDown{false};
    float mouseX{0.0f};
    float mouseY{0.0f};
    float mouseDeltaX{0.0f};
    float mouseDeltaY{0.0f};
    float scrollDelta{0.0f};

    bool keyW{false}, keyA{false}, keyS{false}, keyD{false};
    bool keyQ{false}, keyE{false};
    bool keyUp{false}, keyDown{false}, keyLeft{false}, keyRight{false};

    float deltaTime{0.0f};
};

class GameScene {
public:
    explicit GameScene(const std::string& resourceDir);
    virtual ~GameScene() = default;

    GameScene(const GameScene&) = delete;
    GameScene& operator=(const GameScene&) = delete;
    GameScene(GameScene&&) = default;
    GameScene& operator=(GameScene&&) = default;

    virtual void onInit(VulkanRenderer* renderer);
    virtual void onUpdate(float deltaTime);
    virtual void onInput(const InputState& input);
    virtual void onGui();
    virtual void onShutdown();

    [[nodiscard]] Scene::SceneData& getSceneData() noexcept { return m_sceneData; }
    [[nodiscard]] const Scene::SceneData& getSceneData() const noexcept { return m_sceneData; }

    [[nodiscard]] const std::vector<std::unique_ptr<Mesh>>& getMeshes() const noexcept { return m_meshes; }
    [[nodiscard]] std::vector<std::unique_ptr<Mesh>>& getMeshes() noexcept { return m_meshes; }
    [[nodiscard]] const std::vector<MeshInstance>& getMeshInstances() const noexcept { return m_meshInstances; }
    [[nodiscard]] std::vector<MeshInstance>& getMeshInstances() noexcept { return m_meshInstances; }

    [[nodiscard]] Mesh* getMesh(uint32_t id) noexcept {
        return id < m_meshes.size() ? m_meshes[id].get() : nullptr;
    }
    [[nodiscard]] const Mesh* getMesh(uint32_t id) const noexcept {
        return id < m_meshes.size() ? m_meshes[id].get() : nullptr;
    }

    [[nodiscard]] const TriVector& getCameraEye() const noexcept { return m_cameraEye; }
    [[nodiscard]] const TriVector& getCameraTarget() const noexcept { return m_cameraTarget; }
    [[nodiscard]] const TriVector& getCameraUp() const noexcept { return m_cameraUp; }
    [[nodiscard]] float getCameraFov() const noexcept { return m_cameraFov; }

    void setCamera(const TriVector& eye, const TriVector& target, const TriVector& up) noexcept {
        m_cameraEye = eye;
        m_cameraTarget = target;
        m_cameraUp = up;
    }

    void setCameraFov(float fov) noexcept { m_cameraFov = fov; }

    void getCameraMotor(float out[8]) const noexcept;

    [[nodiscard]] float getFPS() const noexcept { return m_fps; }
    void updateFPS(float deltaTime) noexcept;

    [[nodiscard]] const std::string& getResourceDir() const noexcept { return m_resourceDir; }
    [[nodiscard]] const std::string& getTextureFilename() const noexcept { return m_textureFilename; }

protected:
    uint32_t loadMesh(std::string_view objFilename, std::string_view textureFilename = {});

    void freeMeshCPUData(uint32_t meshId);
    void freeAllMeshCPUData();
    [[nodiscard]] bool isMeshCPUDataFreed(uint32_t meshId) const noexcept;

    uint32_t addMeshInstance(uint32_t meshId, const TriVector& position, std::string_view name = {});
    uint32_t addMeshInstance(uint32_t meshId, const Motor& transform, std::string_view name = {});

    [[nodiscard]] MeshInstance* getInstance(uint32_t instanceId) noexcept;
    [[nodiscard]] MeshInstance* findInstance(std::string_view name) noexcept;
    void setInstanceTransform(uint32_t instanceId, const Motor& transform);
    void setInstancePosition(uint32_t instanceId, const TriVector& position);
    void setInstanceVisible(uint32_t instanceId, bool visible);

    uint32_t addSphere(const TriVector& center, float radius, const Scene::Material& material);
    uint32_t addPlane(const Vector& plane, const Scene::Material& material);
    uint32_t addSphere(float x, float y, float z, float radius, const Scene::Material& material);
    uint32_t addPlane(float nx, float ny, float nz, float distance, const Scene::Material& material);
    uint32_t addGroundPlane(float height, const Scene::Material& material);
    [[nodiscard]] Scene::GPUSphere* getSphere(uint32_t id) noexcept;
    [[nodiscard]] Scene::GPUPlane* getPlane(uint32_t id) noexcept;

    void setSphereMaterial(uint32_t id, const Scene::Material& material);
    void setPlaneMaterial(uint32_t id, const Scene::Material& material);

    uint32_t addDirectionalLight(const TriVector& direction, const Scene::Color& color, float intensity);
    uint32_t addPointLight(const TriVector& position, const Scene::Color& color, float intensity, float range);
    uint32_t addSpotLight(const TriVector& position, const TriVector& direction,
                          const Scene::Color& color, float intensity, float angle, float range);
    uint32_t addDirectionalLight(float dx, float dy, float dz, const Scene::Color& color, float intensity);
    uint32_t addPointLight(float x, float y, float z, const Scene::Color& color, float intensity, float range);
    uint32_t addSpotLight(float px, float py, float pz, float dx, float dy, float dz,
                          const Scene::Color& color, float intensity, float angle, float range);
    [[nodiscard]] Scene::GPULight* getLight(uint32_t id) noexcept;

    std::string m_resourceDir;
    Scene::SceneData m_sceneData;

    std::vector<std::unique_ptr<Mesh>> m_meshes;
    std::vector<MeshInstance> m_meshInstances;
    std::unordered_map<std::string, uint32_t> m_namedInstances;
    std::vector<bool> m_meshCPUDataFreed;
    std::string m_textureFilename;

    TriVector m_cameraEye{0.0f, 3.5f, 8.0f};
    TriVector m_cameraTarget{0.0f, 1.5f, 0.0f};
    TriVector m_cameraUp{0.0f, 1.0f, 0.0f};
    float m_cameraFov{45.0f};

    float m_fps{0.0f};
    float m_fpsAccumulator{0.0f};
    uint32_t m_fpsFrameCount{0};
};
