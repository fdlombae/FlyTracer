# Scene Overview

FlyTracer uses the `GameScene` class as the base for all scenes. Your scene inherits from `GameScene` and overrides lifecycle methods to build and animate your raytraced world.

## Lifecycle Methods

```cpp
class MyScene : public GameScene {
public:
    explicit MyScene(const std::string& resourceDir);

    void onInit(VulkanRenderer* renderer) override;   // Called once at startup
    void onUpdate(float deltaTime) override;          // Called every frame
    void onInput(const InputState& input) override;   // Called every frame with input state
    void onGui() override;                            // Called every frame for ImGui
    void onShutdown() override;                       // Called on cleanup
};
```

### onInit

Called once when the scene is loaded. Use this to:

- Add primitives (spheres, planes)
- Load meshes
- Set up lights
- Configure the initial camera position

```cpp
void MyScene::onInit(VulkanRenderer* renderer) {
    addGroundPlane(0.0f, Scene::Material::Lambert(Scene::Color::Gray()));
    addSphere(TriVector(0.0f, 2.0f, 0.0f), 2.0f,
              Scene::Material::Metal(Scene::Color::Red()));
    addPointLight(TriVector(0.0f, 10.0f, 0.0f), Scene::Color::White(), 1.5f, 50.0f);

    m_cameraEye = TriVector(0.0f, 5.0f, 15.0f);
    m_cameraTarget = TriVector(0.0f, 2.0f, 0.0f);
}
```

### onUpdate

Called every frame with the time elapsed since the last frame. Use this for animations.

```cpp
void MyScene::onUpdate(float deltaTime) {
    m_time += deltaTime;

    // Animate a sphere up and down
    if (auto* sphere = getSphere(m_sphereId)) {
        float y = 2.0f + std::sin(m_time) * 1.0f;
        sphere->setCenter(TriVector(0.0f, y, 0.0f));
    }
}
```

### onInput

Called every frame with the current input state. Use this for camera control and interaction.

```cpp
void MyScene::onInput(const InputState& input) {
    if (input.rightMouseDown) {
        m_cameraYaw += input.mouseDeltaX * 0.005f;
        m_cameraPitch += input.mouseDeltaY * 0.005f;
    }
}
```

### onGui

Called every frame to render ImGui debug UI.

```cpp
void MyScene::onGui() {
    ImGui::Begin("Debug");
    ImGui::Text("FPS: %.1f", getFPS());
    ImGui::SliderFloat("Speed", &m_speed, 0.0f, 5.0f);
    ImGui::End();
}
```

## Scene Data Access

The `GameScene` provides access to the underlying scene data:

```cpp
// Get all scene data (spheres, planes, lights, materials)
Scene::SceneData& data = getSceneData();

// Access meshes
const std::vector<std::unique_ptr<Mesh>>& meshes = getMeshes();
Mesh* mesh = getMesh(meshId);

// Access mesh instances
const std::vector<MeshInstance>& instances = getMeshInstances();
MeshInstance* instance = getInstance(instanceId);
MeshInstance* named = findInstance("myObject");
```

## Protected Members

These members are available in your scene class:

| Member | Type | Description |
|--------|------|-------------|
| `m_resourceDir` | `std::string` | Path to resources folder |
| `m_sceneData` | `Scene::SceneData` | All primitives, lights, materials |
| `m_meshes` | `vector<unique_ptr<Mesh>>` | Loaded meshes |
| `m_meshInstances` | `vector<MeshInstance>` | Mesh instances in the scene |
| `m_cameraEye` | `TriVector` | Camera position |
| `m_cameraTarget` | `TriVector` | Camera look-at point |
| `m_cameraUp` | `TriVector` | Camera up direction |
| `m_cameraFov` | `float` | Field of view in degrees |
