#pragma once

#include "Camera.h"
#include "FlyFish.h"
#include "Input.h"
#include "Mesh.h"
#include "Scene.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// A mesh placed in the world. Local points p map to M (scale p) ~M, followed by a
// reflection in the mirror plane when there is one (an odd versor: mirror * M).
struct MeshInstance {
    uint32_t meshId{0};
    Motor transform{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float scale{1.0f};
    std::optional<Vector> mirror;
    bool visible{true};
    std::string name;

    [[nodiscard]] TriVector ToWorld(const TriVector& localPoint) const;
    // This instance reflected in a plane. A second reflection turns the mirror into a motor again.
    [[nodiscard]] MeshInstance Reflected(const Vector& plane) const;
};

struct DebugLine {
    TriVector from;
    TriVector to;
    Scene::Color color;
};

class GameScene {
public:
    explicit GameScene(std::string resourceDir);
    virtual ~GameScene() = default;

    GameScene(const GameScene&) = delete;
    GameScene& operator=(const GameScene&) = delete;

    // Called by the engine, these wrap the virtual hooks below
    void Init();
    void HandleInput(const Input& input);
    void Update(float deltaTime);
    void DrawGui();
    void Shutdown();

    // Read by the renderer
    [[nodiscard]] const Scene::SceneData& GetSceneData() const noexcept { return m_sceneData; }
    [[nodiscard]] const std::vector<std::unique_ptr<Mesh>>& GetMeshes() const noexcept { return m_meshes; }
    [[nodiscard]] const std::vector<MeshInstance>& GetMeshInstances() const noexcept { return m_meshInstances; }
    [[nodiscard]] const std::vector<DebugLine>& GetDebugLines() const noexcept { return m_debugLines; }
    [[nodiscard]] const Camera& GetCamera() const noexcept { return m_camera; }

protected:
    virtual void OnInit() {}
    virtual void OnInput([[maybe_unused]] const Input& input) {}
    virtual void OnUpdate([[maybe_unused]] float deltaTime) {}
    virtual void OnGui() {}
    virtual void OnShutdown() {}

    [[nodiscard]] const std::string& GetResourceDir() const noexcept { return m_resourceDir; }
    [[nodiscard]] float GetFPS() const noexcept { return m_fps; }

    // Camera
    void SetCamera(const TriVector& eye, const TriVector& target,
                   const TriVector& up = TriVector(0.0f, 1.0f, 0.0f, 0.0f)) noexcept;
    void SetCameraFov(float fovDegrees) noexcept { m_camera.fov = fovDegrees; }
    void SetExposure(float exposure) noexcept { m_camera.exposure = exposure; }

    // Meshes: loaded once, then placed any number of times as instances.
    // Instance and light ids stay valid when others are removed, and are never reused.
    uint32_t LoadMesh(std::string_view objFilename, std::string_view textureFilename = {});
    // A mesh built in code; its BVH is built here
    uint32_t AddMesh(std::unique_ptr<Mesh> mesh);
    [[nodiscard]] Mesh* GetMesh(uint32_t id) noexcept;

    uint32_t AddMeshInstance(uint32_t meshId, const TriVector& position, std::string_view name = {});
    uint32_t AddMeshInstance(uint32_t meshId, const Motor& transform, std::string_view name = {});
    [[nodiscard]] MeshInstance* GetInstance(uint32_t instanceId) noexcept;
    [[nodiscard]] MeshInstance* FindInstance(std::string_view name) noexcept;
    void SetInstanceTransform(uint32_t instanceId, const Motor& transform);
    void SetInstancePosition(uint32_t instanceId, const TriVector& position);
    void SetInstanceVisible(uint32_t instanceId, bool visible);
    // Pointers from GetInstance and FindInstance are invalid after adding or removing an instance
    bool RemoveMeshInstance(uint32_t instanceId);
    [[nodiscard]] const std::vector<uint32_t>& GetInstanceIds() const noexcept { return m_instanceIds; }

    // Analytic primitives
    uint32_t AddSphere(const TriVector& center, float radius, const Scene::Material& material);
    uint32_t AddPlane(const Vector& plane, const Scene::Material& material);
    uint32_t AddGroundPlane(float height, const Scene::Material& material);
    [[nodiscard]] Scene::Sphere* GetSphere(uint32_t id) noexcept;
    [[nodiscard]] Scene::Plane* GetPlane(uint32_t id) noexcept;

    // Lights; directions are the direction the light travels, the spot angle is in degrees
    uint32_t AddDirectionalLight(const TriVector& direction, const Scene::Color& color, float intensity);
    uint32_t AddPointLight(const TriVector& position, const Scene::Color& color, float intensity, float range);
    uint32_t AddSpotLight(const TriVector& position, const TriVector& direction,
                          const Scene::Color& color, float intensity, float angle, float range);
    [[nodiscard]] Scene::Light* GetLight(uint32_t id) noexcept;
    bool RemoveLight(uint32_t id);
    [[nodiscard]] const std::vector<uint32_t>& GetLightIds() const noexcept { return m_lightIds; }

    // Debug lines, drawn by the renderer on top of the scene (dimmed where hidden).
    // They are cleared before every OnUpdate.
    void DrawDebugLine(const TriVector& from, const TriVector& to,
                       const Scene::Color& color = Scene::Color::White());
    // A BiVector line, drawn halfLength to both sides of the point on the line closest to the origin
    void DrawDebugLine(const BiVector& line, float halfLength = 10.0f,
                       const Scene::Color& color = Scene::Color::White());
    void DrawDebugPoint(const TriVector& point, float size = 0.1f,
                        const Scene::Color& color = Scene::Color::White());
    void DrawDebugAxes(const TriVector& position, float size = 1.0f);
    // The coordinate frame of a motor: the transformed world axes
    void DrawDebugAxes(const Motor& transform, float size = 1.0f);
    void SetDebugDrawEnabled(bool enabled) noexcept { m_debugDrawEnabled = enabled; }
    [[nodiscard]] bool IsDebugDrawEnabled() const noexcept { return m_debugDrawEnabled; }

private:
    void updateFPS(float deltaTime) noexcept;
    uint32_t addLight(const Scene::Light& light);

    std::string m_resourceDir;
    Scene::SceneData m_sceneData;
    std::vector<std::unique_ptr<Mesh>> m_meshes;
    std::vector<MeshInstance> m_meshInstances;
    std::vector<uint32_t> m_instanceIds;  // Ascending, one per instance
    uint32_t m_nextInstanceId{0};
    std::unordered_map<std::string, uint32_t> m_namedInstances;  // Name to instance id
    std::vector<uint32_t> m_lightIds;  // Ascending, one per light in m_sceneData.lights
    uint32_t m_nextLightId{0};
    std::vector<DebugLine> m_debugLines;
    Camera m_camera;
    bool m_debugDrawEnabled{true};

    float m_fps{0.0f};
    float m_fpsAccumulator{0.0f};
    uint32_t m_fpsFrameCount{0};
};
