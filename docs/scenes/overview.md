# Scene Overview

FlyTracer uses the `GameScene` class as the base for all scenes. Your scene inherits from `GameScene` and overrides the protected lifecycle hooks to build and animate your raytraced world. The engine calls them; the scene only describes what is in the world, the renderer takes care of the GPU.

## Lifecycle Hooks

```cpp
class MyScene : public GameScene {
public:
    explicit MyScene(const std::string& resourceDir);

protected:
    void OnInit() override;                     // Called once at startup
    void OnInput(const Input& input) override;  // Called every frame with the input state
    void OnUpdate(float deltaTime) override;    // Called every frame
    void OnGui() override;                      // Called every frame for ImGui
    void OnShutdown() override;                 // Called on cleanup
};
```

All hooks have an empty default, so override only what you need. Every frame the engine calls `OnInput`, then `OnUpdate`, then `OnGui`, and then renders the scene.

### OnInit

Called once when the scene is loaded. Use this to:

- Add primitives (spheres, planes)
- Load meshes and place instances
- Set up lights
- Configure the initial camera

```cpp
void MyScene::OnInit() {
    AddGroundPlane(0.0f, Scene::Material::Lambert(Scene::Color::Gray()));
    m_sphereId = AddSphere(TriVector(0.0f, 2.0f, 0.0f), 2.0f,
                           Scene::Material::Metal(Scene::Color::Red()));
    AddPointLight(TriVector(0.0f, 10.0f, 0.0f), Scene::Color::White(), 1.5f, 50.0f);

    SetCamera(TriVector(0.0f, 5.0f, 15.0f), TriVector(0.0f, 2.0f, 0.0f));
}
```

### OnUpdate

Called every frame with the time elapsed since the last frame (at most 0.1 s, so a stall does not make animations jump). Use this for animations. Debug lines are cleared just before it, so draw them here.

```cpp
void MyScene::OnUpdate(float deltaTime) {
    m_time += deltaTime;

    // Animate a sphere up and down
    if (auto* sphere = GetSphere(m_sphereId)) {
        sphere->center = TriVector(0.0f, 2.0f + std::sin(m_time), 0.0f);
    }
}
```

### OnInput

Called every frame with the current [input state](input.md). The engine's `OrbitCamera` handles the usual right-drag orbit and scroll zoom:

```cpp
void MyScene::OnInput(const Input& input) {
    m_orbit.HandleInput(input);
    SetCamera(m_orbit.Eye(), m_orbit.target);

    if (input.WasPressed(Key::R)) Reset();
}
```

### OnGui

Called every frame to build the ImGui debug UI.

```cpp
void MyScene::OnGui() {
    ImGui::Begin("Debug");
    ImGui::Text("FPS: %.1f", GetFPS());
    ImGui::SliderFloat("Speed", &m_speed, 0.0f, 5.0f);
    ImGui::End();
}
```

## Scene Building API

Everything is added and changed through protected functions; the scene's data itself is private.

| Function | Description |
|----------|-------------|
| `SetCamera(eye, target, up)`, `SetCameraFov`, `SetExposure` | [Camera](camera.md) |
| `AddSphere`, `AddPlane`, `AddGroundPlane`, `GetSphere`, `GetPlane` | [Primitives](primitives.md) |
| `LoadMesh`, `AddMesh`, `AddMeshInstance`, `GetInstance`, `FindInstance`, `RemoveMeshInstance`, `GetInstanceIds` | [Meshes](meshes.md) |
| `AddPointLight`, `AddDirectionalLight`, `AddSpotLight`, `GetLight`, `RemoveLight`, `GetLightIds` | [Lighting](lighting.md) |
| `DrawDebugLine`, `DrawDebugPoint`, `DrawDebugAxes` | [Debug Visualization](debug.md) |
| `GetFPS`, `GetResourceDir` | Frame rate and the resources folder |

`GetSphere`, `GetPlane`, `GetLight`, `GetInstance` and `FindInstance` return a pointer (or `nullptr` for an unknown id or name) that you can modify; the renderer picks up changes in the next frame.
